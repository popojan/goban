#ifndef GOBAN_OPENGL_H
#define GOBAN_OPENGL_H

#include <RmlUi/Core/Platform.h>
#if defined(RMLUI_PLATFORM_MACOSX)
#elif defined(RMLUI_PLATFORM_LINUX)
    #include <glad/glad.h>
    #include <glad/glad_glx.h>
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
#elif defined(RMLUI_PLATFORM_WIN32)
    #include <boost/process.hpp>
    #include <windows.h>
    #include <glad/glad.h>
    #include <glad/glad_wgl.h>
#else
    #error Platform is undefined, this must be resolved so gl_context is usable.
#endif

#endif //GOBAN_OPENGL_H
