#pragma once

#include "AudioFile.hpp"
#include "portaudio.h"
#include <sstream>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>

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

        static void init();

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

        /// How many sounds have been mixed all the way to their end since
        /// startup. Incremented by the audio callback — an atomic add, which is
        /// the only kind of bookkeeping that belongs on that thread.
        ///
        /// It exists because "the sound did not play" had no observable symptom:
        /// the request was logged, the mixer silently never saw it, and there was
        /// nothing a test could assert on. This is that symptom.
        [[nodiscard]] unsigned long long completedPlaybacks() const {
                return completed.load(std::memory_order_relaxed);
        }

        void stopIfInactive();  // Stop stream if it finished playing (releases pipewire connection)
    private:
        void ensureInitialized();  // Lazy init of PortAudio
        void shutdown();           // Full shutdown of PortAudio

        const int CHANNEL_COUNT = 2;
        const int SAMPLE_RATE = 44100;
        const int FRAMES_PER_BUFFER = 2048;  // Fixed buffer size for predictable callbacks
        const PaStreamParameters * NO_INPUT = nullptr;
        PaStream * stream = nullptr;
        vector<Playback *> data;
        std::mutex mut;
        bool initialized = false;
        std::chrono::steady_clock::time_point lastActivityTime;
        static constexpr int IDLE_SHUTDOWN_SECONDS = 180;  // 3 minutes
        std::vector<int> mixBuffer;  // Pre-allocated mixing buffer
        std::atomic<unsigned long long> completed{0};
};
