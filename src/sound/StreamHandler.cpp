#include "StreamHandler.hpp"
#include <spdlog/spdlog.h>

int StreamHandler::PortAudioCallback(const void * input,
                                     void * output,
                                     unsigned long frameCount,
                                     const PaStreamCallbackTimeInfo * paTimeInfo,
                                     PaStreamCallbackFlags statusFlags,
                                     void * userData)
{
        StreamHandler * handler = (StreamHandler *) userData;

        unsigned long stereoFrameCount = frameCount * handler->CHANNEL_COUNT;
        memset((int *) output, 0, stereoFrameCount * sizeof(int));

        if (handler->data.size() > 0)
        {
                auto it = handler->data.begin();
                while (it != handler->data.end())
                {
                        Playback * data = (*it);
                        AudioFile * audioFile = data->audioFile;

                        int * outputBuffer = new int[stereoFrameCount];
                        int * bufferCursor = outputBuffer;

                        unsigned int framesLeft = (unsigned int) frameCount;
                        unsigned int framesRead;

                        bool playbackEnded = false;
                        while (framesLeft > 0)
                        {
                                sf_seek(audioFile->data, data->position, SEEK_SET);

                                if (framesLeft > (audioFile->info.frames - data->position))
                                {
                                        framesRead = (unsigned int) (audioFile->info.frames - data->position);
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

                                sf_readf_int(audioFile->data, bufferCursor, framesRead);
                                bufferCursor += framesRead;

                                framesLeft -= framesRead;
                        }


                        int * outputCursor = (int *) output;
                        if (audioFile->info.channels == 1) {
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
                {
                    std::lock_guard<std::mutex> lock(mut);
                    data.push_back(new Playback {
                            audioFile,
                            0,
                            loop,
                            volume
                    });
                }
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
        : data()
{

#if defined (__linux__)
        putenv("PULSE_LATENCY_MSEC=20");
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

        if (errorCode)
        {
                Pa_Terminate();

                stringstream error;
                error << "Unable to open stream for output. Portaudio error code: " << errorCode;
                spdlog::get("console")->error(error.str());
                throw error.str();
        }
}

StreamHandler::~StreamHandler()
{
        Pa_CloseStream(stream);
        for (auto wrapper : data)
        {
                delete wrapper;
        }
        Pa_Terminate();
}
