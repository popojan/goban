#pragma once

#include "AudioFile.hpp"
#include "util.hpp"

#include <map>
#include <sstream>
#include <string>

class FileHandler
{
public:
    FileHandler();
    ~FileHandler();

    bool containsSound(const std::string& filename);
    AudioFile & getSound(const std::string& filename);
private:
    std::map<std::string, AudioFile> sounds;
};
