#include "FileHandler.hpp"
#include <spdlog/spdlog.h>

FileHandler::FileHandler()
        : sounds()
{

}

FileHandler::~FileHandler()
{
        for (auto entry : sounds)
        {
                sf_close(entry.second.data);
        }
}

bool FileHandler::containsSound(string filename)
{
        return sounds.find(filename) != sounds.end();
}

AudioFile & FileHandler::getSound(string filename)
{
        if (!containsSound(filename)) {
                string fullFilename = util::getApplicationPath() + "/" + filename;
                SF_INFO info;
                info.format = 0;
                SNDFILE * audioFile = sf_open(fullFilename.c_str(), SFM_READ, &info);

                AudioFile sound {
                        audioFile,
                        info
                };

                if (!audioFile)
                {
                        stringstream error;
                        error << "Unable to open audio file '"
                              << filename << "' with full filename '"
                              << fullFilename << "'";
                        spdlog::get("console")->error(error.str());
                        throw error.str();
                }
                sounds[filename] = sound;
        }
        return sounds[filename];
}
