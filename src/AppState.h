#ifndef GOBAN_APPSTATE_H
#define GOBAN_APPSTATE_H

#include <GLFW/glfw3.h>
#include <string>

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
