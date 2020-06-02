#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include "StreamHandler.hpp"
#include "FileHandler.hpp"
#include "Configuration.h"

class AudioPlayer
{
public:
    AudioPlayer();

    void play(const std::string& id, double volume = 1.0);
    void loop(const std::string& id, double volume = 1.0);
    void stop();
    void preload(const std::shared_ptr<Configuration> config);
    size_t playbackCount() { return streamHandler.playbackCount(); }
private:
    FileHandler fileHandler;
    StreamHandler streamHandler;
};
#endif
