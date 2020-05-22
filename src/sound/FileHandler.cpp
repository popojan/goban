#include "FileHandler.hpp"
#include <spdlog/spdlog.h>

FileHandler::FileHandler()
        : sounds()
{

}

FileHandler::~FileHandler()
{
        /*for (auto entry : sounds)
        {
                sf_close(entry.second.data);
        }*/
}

bool FileHandler::containsSound(const std::string& id)
{
        return sounds.find(id) != sounds.end();
}

AudioFile & FileHandler::getSound(const std::string& id, const std::string& filename)
{
        if(sounds.find(id) == sounds.end()) {
            sounds[id] = AudioFile();
            sounds[id].load(filename);
        }
        return sounds[id];
}
