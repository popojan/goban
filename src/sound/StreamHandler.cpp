#include "StreamHandler.hpp"
#include <spdlog/spdlog.h>

#include <RmlUi/Core/Platform.h>

#if defined(RMLUI_PLATFORM_LINUX)
    #include <alsa/asoundlib.h>
    #include <cstdarg>

    void alsa_error_callback (
            const char *file,
            int line,
            const char *function,
            int err, const char *fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        char * msg = nullptr;
        if (vasprintf(&msg, fmt, args) >= 0 && msg) {
            if(err >= SND_ERROR_BEGIN)
                spdlog::warn("Alsa error {} in {} at {}:{}: {}", err, function, file, line, msg);
            else
                spdlog::trace("Alsa info {} in {} at {}:{}: {}", err, function, file, line, msg);
            free(msg);
        }
        va_end(args);
    }
#endif

int StreamHandler::PortAudioCallback(const void * input,
                                     void * output,
                                     unsigned long frameCount,
                                     const PaStreamCallbackTimeInfo * paTimeInfo,
                                     PaStreamCallbackFlags statusFlags,
                                     void * userData)
{
        StreamHandler * handler = static_cast<StreamHandler *>(userData);

        unsigned long stereoFrameCount = frameCount * handler->CHANNEL_COUNT;
        memset((int *) output, 0, stereoFrameCount * sizeof(int));

	int * outputBuffer = new int[stereoFrameCount];
	memset(outputBuffer, 0, stereoFrameCount*sizeof(int));

        if (handler->data.size() > 0)
        {
                auto it = handler->data.begin();
                while (it != handler->data.end())
                {
                        Playback * data = (*it);
                        AudioFile * audioFile = data->audioFile;

                        int * bufferCursor = outputBuffer;

                        unsigned int framesLeft = static_cast<unsigned int>(frameCount);
                        int framesRead;

                        bool playbackEnded = false;
                        while (framesLeft > 0)
                        {
                                //sf_seek(audioFile->data, data->position, SEEK_SET);
                                int pos = data->position;
                                if (framesLeft > (audioFile->fh.frames() - data->position))
                                {
                                        framesRead = static_cast<unsigned int>(audioFile->fh.frames() - data->position);
                                        if (data->loop)
                                        {
                                                data->position = 0;
                                        } else
                                        {
                                                playbackEnded = true;
                                                framesLeft = framesRead;
                                        }
                                } else
                                {
                                        framesRead = framesLeft;
                                        data->position += framesRead;
                                }

                                std::copy(&audioFile->data[pos], &audioFile->data[pos + framesRead], bufferCursor);

                                bufferCursor += framesRead;

                                framesLeft -= framesRead;
                        }
                        int * outputCursor = static_cast<int *>(output);
                        if (audioFile->fh.channels() == 1) {
                                for (unsigned long i = 0; i < stereoFrameCount; ++i)
                                {
                                        *outputCursor += (data->volume * 0.5 * outputBuffer[i]);
                                        ++outputCursor;
                                        *outputCursor += (data->volume * 0.5 * outputBuffer[i]);
                                        ++outputCursor;
                                }
                        } else {
                                for (unsigned long i = 0; i < stereoFrameCount; ++i)
                                {
                                        *outputCursor += (data->volume * 0.5 * outputBuffer[i]);
                                        ++outputCursor;
                                }
                        }


                        if (playbackEnded) {
                                it = handler->data.erase(it);
                                delete data;
                        } else
                        {
                                ++it;
                        }
                }
        }
	delete [] outputBuffer;

        // Stop stream when no more audio to play
        if (handler->data.empty()) {
                return paComplete;
        }
        return paContinue;
}

void StreamHandler::processEvent(AudioEventType audioEventType, AudioFile * audioFile, double volume, bool loop)
{
        switch (audioEventType) {
        case start:
                ensureInitialized();
                if (!initialized) {
                        spdlog::warn("Audio: init failed, skipping sound");
                        return;
                }

                // Use !Pa_IsStreamActive() instead of Pa_IsStreamStopped() because
                // stream can be in "complete" state (not stopped, but not active either)
                if (!Pa_IsStreamActive(stream))
                {
                        // If stream is in "complete" state (not stopped), stop it first
                        if (!Pa_IsStreamStopped(stream)) {
                                Pa_StopStream(stream);
                        }
                        PaError err = Pa_StartStream(stream);
                        if (err != paNoError) {
                                spdlog::warn("Audio: Pa_StartStream failed: {}", Pa_GetErrorText(err));
                        }
                }
                data.push_back(new Playback {
                        audioFile,
                        0,
                        loop,
                        volume
                });
                break;
        case stop:
                if (initialized && stream) {
                        Pa_StopStream(stream);
                }
                for (auto instance : data)
                {
                        delete instance;
                }
                data.clear();
                break;
        }
}

StreamHandler::StreamHandler()
        : data() {
}

void StreamHandler::stopIfInactive() {
        // Only shutdown if stream finished AND no pending playbacks (allow overlapping sounds)
        if (initialized && stream && data.empty() && !Pa_IsStreamActive(stream)) {
                shutdown();
        }
}

void StreamHandler::shutdown() {
        if (!initialized) return;

        if (stream) {
                Pa_StopStream(stream);
                Pa_CloseStream(stream);
                stream = nullptr;
        }
        Pa_Terminate();
        initialized = false;
}

void StreamHandler::init() {
#if defined(RMLUI_PLATFORM_LINUX)
        snd_lib_error_set_handler(alsa_error_callback);
#endif
        // Don't initialize PortAudio here - lazy init on first sound
}

void StreamHandler::ensureInitialized() {
        if (initialized) return;

        Pa_Initialize();
        PaStreamParameters outputParameters;
        outputParameters.device = Pa_GetDefaultOutputDevice();
        outputParameters.channelCount = CHANNEL_COUNT;
        outputParameters.sampleFormat = paInt32;
        outputParameters.suggestedLatency = 0.02;
        outputParameters.hostApiSpecificStreamInfo = nullptr;

        PaError errorCode = Pa_OpenStream(&stream,
                                          NO_INPUT,
                                          &outputParameters,
                                          SAMPLE_RATE,
                                          paFramesPerBufferUnspecified,
                                          paNoFlag,
                                          &PortAudioCallback,
                                          this);

        if (errorCode)
        {
                Pa_Terminate();
                stringstream error;
                error << "Unable to open stream for output. Portaudio error code: " << errorCode;
                spdlog::error(error.str());
                return;  // Don't throw - just disable audio
        }
        initialized = true;
}

StreamHandler::~StreamHandler()
{
        for (auto wrapper : data)
        {
                delete wrapper;
        }
        shutdown();
}
