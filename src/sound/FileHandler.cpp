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

bool FileHandler::containsSound(const std::string& filename)
{
        return sounds.find(filename) != sounds.end();
}

AudioFile & FileHandler::getSound(const std::string& filename)
{
        if(sounds.find(filename) == sounds.end()){
            //spdlog::get("console")->info("Preloading sound [{}]...", filename);
            sounds[filename] = AudioFile();
            sounds[filename].load(filename);
        }
        return sounds[filename];
}
