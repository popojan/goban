#pragma once

#include "AudioFile.hpp"

#include "sndfile.h"
#include "portaudio.h"

#if defined (__linux__)
    #include <cstdlib> // required for putenv.
#endif

#include <cstring>
#include <sstream>
#include <vector>
#include <mutex>

using std::stringstream;
using std::vector;

struct Playback
{
        AudioFile * audioFile;
        int position;
        bool loop;
        double volume;
};

enum AudioEventType
{
        start, stop
};

class StreamHandler
{
    public:
        StreamHandler();
        ~StreamHandler();

        void processEvent(AudioEventType audioEventType,
                          AudioFile * audioFile = nullptr,
                          double volume = 1.0,
                          bool loop = false);

        static int PortAudioCallback(const void * input,
                                     void * output,
                                     unsigned long frameCount,
                                     const PaStreamCallbackTimeInfo * paTimeInfo,
                                     PaStreamCallbackFlags statusFlags,
                                     void * userData);
        size_t playbackCount() {  std::lock_guard<std::mutex> lock(mut); return data.size(); }
    private:
        const int CHANNEL_COUNT = 2;
        const int SAMPLE_RATE = 44100;
        const PaStreamParameters * NO_INPUT = nullptr;
        PaStream * stream;
        vector<Playback *> data;
        std::mutex mut;
};
