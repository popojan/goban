//
// Created by popoj on 04.06.2020.
//

#ifndef GOBAN_OPENGL_H
#define GOBAN_OPENGL_H

#include <Rocket/Core/Platform.h>

#if defined(ROCKET_PLATFORM_MACOSX)
AGLContext gl_context;
#elif defined(ROCKET_PLATFORM_LINUX)
struct __X11NativeWindowData nwData;
	GLXContext gl_context;
#elif defined(ROCKET_PLATFORM_WIN32)
    #include <boost/process.hpp>
    #include <windows.h>
    #include <glad/glad.h>
    #include <glad/glad_wgl.h>
#else
    #error Platform is undefined, this must be resolved so gl_context is usable.
#endif

#endif //GOBAN_OPENGL_H
