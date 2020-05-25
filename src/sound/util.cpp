#include "util.hpp"
#include <spdlog/spdlog.h>

string util::getApplicationPathAndName()
{
        string fullPath;

#if defined (__APPLE__) && defined (__MACH__)

        int ret;
        pid_t pid;
        char pathbuf[PROC_PIDPATHINFO_MAXSIZE];

        pid = getpid();
        ret = proc_pidpath(pid, pathbuf, sizeof(pathbuf));
        if (ret <= 0)
        {
            string error("Unable to ascertain application path");
            spdlog::error(error);
            throw string(error);
        } else
        {
                fullPath = pathbuf;
        }

#elif defined (__linux__)

        char buff[1024];
        ssize_t len = readlink("/proc/self/exe", buff, sizeof(buff)-1);
        if (len != -1) {
                buff[len] = '\0';
                fullPath = buff;
        } else {
            string error("Unable to ascertain application path");
            spdlog::error(error);
            throw string(error);
        }

#else

        string error("OS not supported for finding paths");
        spdlog::error(error);
        throw string(error);

#endif

        return fullPath;
}

string util::getApplicationPath()
{
        string fullPath = getApplicationPathAndName();
        return fullPath.substr(0, fullPath.find_last_of("/"));
}

#if not defined (_WIN32) && not defined (_WIN64)

void util::changemode(int dir)
{
        static struct termios oldt, newt;

        if (dir == 1)
        {
                tcgetattr(STDIN_FILENO, &oldt);
                newt = oldt;
                newt.c_lflag &= ~(ICANON | ECHO);
                tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        }
        else
        {
                tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        }
}

int util::kbhit()
{
        struct timeval tv;
        fd_set rdfs;

        tv.tv_sec = 0;
        tv.tv_usec = 0;

        FD_ZERO(&rdfs);
        FD_SET(STDIN_FILENO, &rdfs);

        select(STDIN_FILENO + 1, &rdfs, NULL, NULL, &tv);

        return FD_ISSET(STDIN_FILENO, &rdfs);
}

#endif
