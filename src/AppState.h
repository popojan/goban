#ifndef GOBAN_APPSTATE_H
#define GOBAN_APPSTATE_H

#include <GLFW/glfw3.h>

// Simple application state management replacing Shell layer
namespace AppState {

void SetWindow(GLFWwindow* window);
GLFWwindow* GetWindow();

// Request application exit
void RequestExit();

// Check if exit was requested
bool ExitRequested();

// Toggle fullscreen mode
bool ToggleFullscreen();

// Get elapsed time since initialization
float GetElapsedTime();

} // namespace AppState

#endif // GOBAN_APPSTATE_H
