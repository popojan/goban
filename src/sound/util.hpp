#pragma once

#include <string>

#if defined (__APPLE__) && defined (__MACH__)

#include <dirent.h>
#include <libproc.h>
#include <unistd.h>
#include <cstdio>
#include <termios.h>
#include <sys/types.h>
#include <sys/time.h>

#elif defined (__linux__)

#include <dirent.h>
#include <unistd.h>
#include <cstdio>
#include <termios.h>
#include <sys/types.h>
#include <sys/time.h>

#endif

using std::string;

namespace util
{
    string getApplicationPathAndName();
    string getApplicationPath();

#if not defined (_WIN32) && not defined (_WIN64)

    void changemode(int dir);
    int kbhit();

#endif
}
