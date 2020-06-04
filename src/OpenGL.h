#ifndef GOBAN_OPENGL_H
#define GOBAN_OPENGL_H

#include <Rocket/Core/Platform.h>

#define SGFCPLUSPLUS_STATIC_DEFINE 1
#include <ISgfcTreeBuilder.h>
#include <SgfcPlusPlusFactory.h>
#include <ISgfcPropertyFactory.h>
#include <ISgfcPropertyValueFactory.h>
#include <ISgfcPropertyValue.h>
#include <SgfcPropertyType.h>
#include <SgfcPlusPlusExport.h>
//#include <SgfcPropertyValueType.h>
#include <ISgfcGoMovePropertyValue.h>
#include <SgfcGameType.h>
#include <ISgfcGame.h>
#include <ISgfcNode.h>
#include <ISgfcDocument.h>
#include <ISgfcDocumentWriter.h>
#include <SgfcConstants.h>

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
