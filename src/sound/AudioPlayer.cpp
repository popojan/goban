#include "AudioPlayer.hpp"

AudioPlayer::AudioPlayer()
    : fileHandler()
    , streamHandler()
{

}

void AudioPlayer::play(const std::string& id, double volume)
{
    streamHandler.processEvent(AudioEventType::start, &fileHandler.getSound(id), volume);
}

void AudioPlayer::loop(const std::string& id, double volume)
{
    streamHandler.processEvent(AudioEventType::start, &fileHandler.getSound(id), volume, true);
}

void AudioPlayer::stop()
{
    streamHandler.processEvent(AudioEventType::stop);
}

void AudioPlayer::preload(const std::shared_ptr<Configuration> config) {
    auto sounds = config->data.find("sounds");
    if (sounds != config->data.end()) {
        for (auto sit = sounds->begin(); sit != sounds->end(); ++sit) {
            (void) fileHandler.getSound(sit.key(), sit.value());
        }
    }
}
