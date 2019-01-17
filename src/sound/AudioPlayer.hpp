#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include "StreamHandler.hpp"
#include "FileHandler.hpp"

class AudioPlayer
{
public:
    AudioPlayer();

    void play(const std::string& soundfile, double volume = 1.0);
    void loop(const std::string& soundfile, double volume = 1.0);
    void stop();
    void preload(const std::vector<std::string>& files);
    size_t playbackCount() { return streamHandler.playbackCount(); }
private:
    FileHandler fileHandler;
    StreamHandler streamHandler;
};
#endif
