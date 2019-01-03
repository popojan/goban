#pragma once

#include "StreamHandler.hpp"
#include "FileHandler.hpp"

class AudioPlayer
{
public:
    AudioPlayer();

    void play(string soundfile, double volume = 1.0);
    void loop(string soundfile, double volume = 1.0);
    void stop();
    size_t playbackCount() { return streamHandler.playbackCount(); }
private:
    FileHandler fileHandler;
    StreamHandler streamHandler;
};
