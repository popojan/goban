#pragma once

#include "sndfile.h"

struct AudioFile
{
        SNDFILE * data;
        SF_INFO info;
};
