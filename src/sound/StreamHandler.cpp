#include "StreamHandler.hpp"
#include <spdlog/spdlog.h>

#include <Rocket/Core/Platform.h>

#if defined(ROCKET_PLATFORM_LINUX)
    #include <alsa/error.h>
    #include <cstdarg>
    #include <cstdio>

    void alsa_error_callback (
            const char *file,
            int line,
            const char *function,
            int err, const char *fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        char * msg;
        vasprintf(&msg, fmt, args);
        if(err >= SND_ERROR_BEGIN)
            spdlog::error("Alsa error {} in {} at {}:{}: {}", err, function, file, line, msg);
        else
            spdlog::warn("Alsa info {} in {} at {}:{}: {}", err, function, file, line, msg);
        free(msg);
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

                        unsigned int framesLeft = (unsigned int) frameCount;
                        int framesRead;

                        bool playbackEnded = false;
                        while (framesLeft > 0)
                        {
                                //sf_seek(audioFile->data, data->position, SEEK_SET);
                                int pos = data->position;
                                if (framesLeft > (audioFile->fh.frames() - data->position))
                                {
                                        framesRead = (unsigned int) (audioFile->fh.frames() - data->position);
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
                        int * outputCursor = (int *) output;
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
        return paContinue;
}

void StreamHandler::processEvent(AudioEventType audioEventType, AudioFile * audioFile, double volume, bool loop)
{
        switch (audioEventType) {
        case start:
                if (Pa_IsStreamStopped(stream))
                {
                        Pa_StartStream(stream);
                }
                data.push_back(new Playback {
                        audioFile,
                        0,
                        loop,
                        volume
                });
                break;
        case stop:
                Pa_StopStream(stream);
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
void StreamHandler::init() {
#if defined (__linux__)
        ;//char latency[] = "PULSE_LATENCY_MSEC=20\0";
        //putenv(latency);
#endif

#if defined(ROCKET_PLATFORM_LINUX)
        snd_lib_error_set_handler(alsa_error_callback);
#endif

        Pa_Initialize();
        PaError errorCode;
        PaStreamParameters outputParameters;
        outputParameters.device = Pa_GetDefaultOutputDevice();
        outputParameters.channelCount = CHANNEL_COUNT;
        outputParameters.sampleFormat = paInt32;
        outputParameters.suggestedLatency = 0.02;
        outputParameters.hostApiSpecificStreamInfo = 0;

        errorCode = Pa_OpenStream(&stream,
                                  NO_INPUT,
                                  &outputParameters,
                                  SAMPLE_RATE,
                                  paFramesPerBufferUnspecified,
                                  paNoFlag,
                                  &PortAudioCallback,
                                  this);
        Pa_StartStream(stream);

        if (errorCode)
        {
                Pa_Terminate();

                stringstream error;
                error << "Unable to open stream for output. Portaudio error code: " << errorCode;
                spdlog::error(error.str());
                throw error.str();
        }
}

StreamHandler::~StreamHandler()
{
        spdlog::debug("PortAudio Termination in progress..");
        Pa_CloseStream(stream);
        for (auto wrapper : data)
        {
                delete wrapper;
        }
        Pa_Terminate();
}
