#ifndef GOBAN_OPENGL_H
#define GOBAN_OPENGL_H

#include <Rocket/Core/Platform.h>
#if defined(ROCKET_PLATFORM_MACOSX)
#elif defined(ROCKET_PLATFORM_LINUX)
    #include <glad/glad.h>
    #include <glad/glad_glx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#elif defined(ROCKET_PLATFORM_WIN32)
    #include <boost/process.hpp>
    #include <windows.h>
    #include <glad/glad.h>
    #include <glad/glad_wgl.h>
#else
    #error Platform is undefined, this must be resolved so gl_context is usable.
#endif

#endif //GOBAN_OPENGL_H
