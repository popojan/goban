#pragma once

#include "sndfile.hh"
#include <vector>
#include <memory>

struct AudioFile
{
        SndfileHandle fh;
        int* data;

        AudioFile(const std::string& fname = "", bool preload = false):data(nullptr) {
            if(preload && !fname.empty()) load(fname);
        }

        void load(const std::string& fname);
        ~AudioFile() {if(data) delete[]data;}
};
