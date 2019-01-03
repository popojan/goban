#include "AudioPlayer.hpp"

AudioPlayer::AudioPlayer()
        : fileHandler()
        , streamHandler()
{

}

void AudioPlayer::play(string soundfile, double volume)
{
        streamHandler.processEvent(AudioEventType::start, &fileHandler.getSound(soundfile), volume);
}

void AudioPlayer::loop(string soundfile, double volume)
{
        streamHandler.processEvent(AudioEventType::start, &fileHandler.getSound(soundfile), volume, true);

}

void AudioPlayer::stop()
{
        streamHandler.processEvent(AudioEventType::stop);
}
