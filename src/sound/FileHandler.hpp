#pragma once

#include "AudioFile.hpp"
#include <map>
#include <string>

class FileHandler
{
public:
    FileHandler();
    ~FileHandler();

    bool containsSound(const std::string& id);
    AudioFile & getSound(const std::string& id, const std::string& filename = std::string());
private:
    std::map<std::string, AudioFile> sounds;
};
