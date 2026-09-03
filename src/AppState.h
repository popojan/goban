#ifndef GOBAN_APPSTATE_H
#define GOBAN_APPSTATE_H

#include <GLFW/glfw3.h>
#include <string>

// Simple application state management replacing Shell layer
namespace AppState {

void SetWindow(GLFWwindow* window);
GLFWwindow* GetWindow();

/// A hidden 1x1 window whose GL context *shares objects* with the main one, so
/// a shader can be linked off the UI thread. Null when it could not be created,
/// which is not an error: GobanShader then links synchronously, exactly as it
/// always did.
///
/// It lives here rather than in GobanShader because glfwCreateWindow may only be
/// called from the main thread, and GobanShader is constructed deep inside
/// RmlUi's document load. This namespace already owns the window handle for the
/// same reason.
void SetShaderContext(GLFWwindow* window);
GLFWwindow* GetShaderContext();

// Request application exit
void RequestExit();

// Check if exit was requested
bool ExitRequested();

// Toggle fullscreen mode
bool ToggleFullscreen();

// Get fullscreen state
bool IsFullscreen();

// Set fullscreen state (for restoring from user preferences)
void SetFullscreen(bool fullscreen);

// Get elapsed time since initialization
float GetElapsedTime();

} // namespace AppState

// Request application restart with different config (Linux only)
// Defined in main.cpp
void RequestRestart(const std::string& configFile);

#endif // GOBAN_APPSTATE_H
