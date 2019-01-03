#pragma once

#include "AudioFile.hpp"
#include "util.hpp"

#include <map>
#include <sstream>
#include <string>

using std::map;
using std::string;
using std::stringstream;

class FileHandler
{
public:
    FileHandler();
    ~FileHandler();

    bool containsSound(string filename);
    AudioFile & getSound(string filename);
private:
    map<string, AudioFile> sounds;
};
