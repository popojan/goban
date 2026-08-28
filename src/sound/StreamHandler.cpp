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

        // The list is shared with the UI thread, which push_back()s into it. This
        // used to run unlocked while processEvent() locked — the writers-and-not-
        // readers shape again — so a reallocation could invalidate the iterator
        // below mid-mix. The critical section is a memcpy of a few hundred
        // samples per active sound; that is short enough to hold a mutex for in a
        // callback, and enormously preferable to the alternative.
        std::lock_guard<std::mutex> lock(handler->mut);

        // Clamp to pre-allocated buffer size (PortAudio may request more)
        if (frameCount > static_cast<unsigned long>(handler->FRAMES_PER_BUFFER))
                frameCount = handler->FRAMES_PER_BUFFER;

        unsigned long stereoFrameCount = frameCount * handler->CHANNEL_COUNT;
        memset((int *) output, 0, stereoFrameCount * sizeof(int));

        // Use pre-allocated buffer (sized to FRAMES_PER_BUFFER * CHANNEL_COUNT)
        int * outputBuffer = handler->mixBuffer.data();
        memset(outputBuffer, 0, stereoFrameCount * sizeof(int));

        if (handler->data.size() > 0)
        {
                auto it = handler->data.begin();
                while (it != handler->data.end())
                {
                        Playback * data = (*it);
                        AudioFile * audioFile = data->audioFile;

                        if (!audioFile->data) {
                                it = handler->data.erase(it);
                                delete data;
                                continue;
                        }

                        int * bufferCursor = outputBuffer;
                        int channels = audioFile->fh.channels();

                        unsigned int framesLeft = static_cast<unsigned int>(frameCount);
                        int framesRead;

                        bool playbackEnded = false;
                        while (framesLeft > 0)
                        {
                                int pos = data->position;  // position in frames
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

                                // Copy samples: frames * channels (for stereo, 2 samples per frame)
                                int samplePos = pos * channels;
                                int sampleCount = framesRead * channels;
                                std::copy(&audioFile->data[samplePos], &audioFile->data[samplePos + sampleCount], bufferCursor);

                                bufferCursor += sampleCount;

                                framesLeft -= framesRead;
                        }
                        int * outputCursor = static_cast<int *>(output);
                        if (audioFile->fh.channels() == 1) {
                                // Mono → stereo: duplicate each mono sample to L+R
                                for (unsigned long i = 0; i < frameCount; ++i)
                                {
                                        int sample = static_cast<int>(data->volume * 0.5 * outputBuffer[i]);
                                        outputCursor[2 * i]     += sample;
                                        outputCursor[2 * i + 1] += sample;
                                }
                        } else {
                                // Stereo: copy L+R pairs directly
                                for (unsigned long i = 0; i < stereoFrameCount; ++i)
                                {
                                        outputCursor[i] += static_cast<int>(data->volume * 0.5 * outputBuffer[i]);
                                }
                        }


                        if (playbackEnded) {
                                handler->completed.fetch_add(1, std::memory_order_relaxed);
                                it = handler->data.erase(it);
                                delete data;
                        } else
                        {
                                ++it;
                        }
                }
        }
        // No delete needed - using pre-allocated buffer

        // Never paComplete. Returning it when the list happened to be empty is
        // what silently swallowed sounds: processEvent() started the stream
        // *before* pushing the playback, the callback fired on its own thread in
        // between, saw nothing to play and stopped the stream — and the sound
        // that had just been queued was never heard. It bit the first sound after
        // a quiet spell, because that is when the stream is not already running,
        // which in practice means the stone you place yourself after thinking.
        //
        // The stream now runs until stopIfInactive() releases it after the idle
        // timeout, which was always the intended way to give the device back.
        // Silence costs one memset per buffer, about twenty times a second.
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

                // Queue it *before* the stream is running. The other order is the
                // bug: Pa_StartStream() returns as soon as the callback thread is
                // live, and that thread would find nothing to play. Whatever the
                // callback does with an empty list, it cannot play a sound that
                // has not been handed to it yet.
                {
                        std::lock_guard<std::mutex> lock(mut);
                        data.push_back(new Playback {
                                audioFile,
                                0,
                                loop,
                                volume
                        });
                }
                lastActivityTime = std::chrono::steady_clock::now();

                if (!Pa_IsStreamActive(stream))
                {
                        // A stream that was stopped by stopIfInactive() is closed
                        // outright, so this only has to cope with "stopped but
                        // open" — which Pa_StartStream handles directly.
                        if (!Pa_IsStreamStopped(stream)) {
                                Pa_StopStream(stream);
                        }
                        PaError err = Pa_StartStream(stream);
                        if (err != paNoError) {
                                spdlog::warn("Audio: Pa_StartStream failed: {}", Pa_GetErrorText(err));
                        }
                }
                break;
        case stop:
                if (initialized && stream) {
                        Pa_StopStream(stream);
                }
                {
                        std::lock_guard<std::mutex> lock(mut);
                        for (auto instance : data)
                        {
                                delete instance;
                        }
                        data.clear();
                }
                break;
        }
}

StreamHandler::StreamHandler()
        : data() {
}

void StreamHandler::stopIfInactive() {
        // This is now the *only* thing that gives the device back — the callback
        // no longer stops the stream on its own. It deliberately does not test
        // Pa_IsStreamActive(): the stream is active for as long as it is open,
        // by design, so that test would never pass again.
        if (!initialized || !stream) return;
        {
                std::lock_guard<std::mutex> lock(mut);
                if (!data.empty()) return;   // something is still playing
        }
        const auto now = std::chrono::steady_clock::now();
        const auto idleSeconds = std::chrono::duration_cast<std::chrono::seconds>(
                now - lastActivityTime).count();
        if (idleSeconds >= IDLE_SHUTDOWN_SECONDS) {
                spdlog::debug("Audio: idle for {}s, releasing the device", idleSeconds);
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

        // Pre-allocate mixing buffer to match fixed frames per buffer
        mixBuffer.resize(FRAMES_PER_BUFFER * CHANNEL_COUNT);

        Pa_Initialize();
        PaStreamParameters outputParameters;
        outputParameters.device = Pa_GetDefaultOutputDevice();
        outputParameters.channelCount = CHANNEL_COUNT;
        outputParameters.sampleFormat = paInt32;
        outputParameters.suggestedLatency = 0.05;  // 50ms - more forgiving than 20ms
        outputParameters.hostApiSpecificStreamInfo = nullptr;

        PaError errorCode = Pa_OpenStream(&stream,
                                          NO_INPUT,
                                          &outputParameters,
                                          SAMPLE_RATE,
                                          FRAMES_PER_BUFFER,  // Fixed size for predictable callback
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
        shutdown();
        for (auto wrapper : data)
        {
                delete wrapper;
        }
}
