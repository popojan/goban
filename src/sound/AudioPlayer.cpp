#include "AudioPlayer.hpp"

AudioPlayer::AudioPlayer()
        : fileHandler()
        , streamHandler()
{

}

void AudioPlayer::play(const std::string& soundfile, double volume)
{
        streamHandler.processEvent(AudioEventType::start, &fileHandler.getSound(soundfile), volume);
}

void AudioPlayer::loop(const std::string& soundfile, double volume)
{
        streamHandler.processEvent(AudioEventType::start, &fileHandler.getSound(soundfile), volume, true);

}

void AudioPlayer::stop()
{
        streamHandler.processEvent(AudioEventType::stop);
}

void AudioPlayer::preload(const std::vector<std::string>& files) {
    for(auto sit = files.begin(); sit != files.end(); ++sit)
        (void)fileHandler.getSound(*sit);
}
