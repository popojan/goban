#include "AppState.h"

namespace AppState {

static GLFWwindow* g_window = nullptr;
static bool g_exitRequested = false;
static bool g_fullscreen = false;
static int g_windowedX = 100;
static int g_windowedY = 100;
static int g_windowedWidth = 1024;
static int g_windowedHeight = 768;

void SetWindow(GLFWwindow* window) {
    g_window = window;
}

GLFWwindow* GetWindow() {
    return g_window;
}

void RequestExit() {
    g_exitRequested = true;
    if (g_window) {
        glfwSetWindowShouldClose(g_window, GLFW_TRUE);
    }
}

bool ExitRequested() {
    return g_exitRequested || (g_window && glfwWindowShouldClose(g_window));
}

bool ToggleFullscreen() {
    if (!g_window) return g_fullscreen;

    g_fullscreen = !g_fullscreen;

    if (g_fullscreen) {
        // Save windowed position and size
        glfwGetWindowPos(g_window, &g_windowedX, &g_windowedY);
        glfwGetWindowSize(g_window, &g_windowedWidth, &g_windowedHeight);

        // Get primary monitor and its video mode
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);

        // Switch to fullscreen
        glfwSetWindowMonitor(g_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    } else {
        // Restore windowed mode
        glfwSetWindowMonitor(g_window, nullptr, g_windowedX, g_windowedY, g_windowedWidth, g_windowedHeight, 0);
    }

    return g_fullscreen;
}

float GetElapsedTime() {
    return static_cast<float>(glfwGetTime());
}

} // namespace AppState
