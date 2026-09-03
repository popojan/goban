#include "AppState.h"

namespace AppState {

/// The monitor the window is actually on, rather than the primary one.
///
/// glfwGetPrimaryMonitor() was used unconditionally, so on a multi-monitor
/// setup fullscreen always landed on the primary output — not the one you were
/// looking at. Matching by the window's centre point handles unequal
/// resolutions and stacked layouts, where testing a corner does not.
///
/// Wayland has no global window position, so glfwGetWindowPos() reports nothing
/// there and this falls back to the primary monitor — the old behaviour, and
/// still wrong on a multi-head Wayland session. GLFW 3.4 exposes no way to ask
/// which output a surface is on, so the escape hatch is to run on X11 (see
/// `--platform` in main.cpp), where this works properly.
static GLFWmonitor* MonitorForWindow(GLFWwindow* window) {
    if (!window) return glfwGetPrimaryMonitor();

    int wx = 0, wy = 0, ww = 0, wh = 0;
    glfwGetWindowPos(window, &wx, &wy);
    glfwGetWindowSize(window, &ww, &wh);
    const int cx = wx + ww / 2;
    const int cy = wy + wh / 2;

    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    for (int i = 0; i < count; ++i) {
        int mx = 0, my = 0, mw = 0, mh = 0;
        glfwGetMonitorWorkarea(monitors[i], &mx, &my, &mw, &mh);
        if (cx >= mx && cx < mx + mw && cy >= my && cy < my + mh) {
            return monitors[i];
        }
    }
    return glfwGetPrimaryMonitor();
}

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

static GLFWwindow* g_shaderContext = nullptr;

void SetShaderContext(GLFWwindow* window) {
    g_shaderContext = window;
}

GLFWwindow* GetShaderContext() {
    return g_shaderContext;
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
        // Note: glfwGetWindowPos may fail on Wayland - that's OK, we use stored defaults
        glfwGetWindowPos(g_window, &g_windowedX, &g_windowedY);
        glfwGetWindowSize(g_window, &g_windowedWidth, &g_windowedHeight);

        // The monitor the window is on, not whichever one is primary.
        GLFWmonitor* monitor = MonitorForWindow(g_window);
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);

        // Switch to fullscreen
        glfwSetWindowMonitor(g_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    } else {
        // Restore windowed mode
        glfwSetWindowMonitor(g_window, nullptr, g_windowedX, g_windowedY, g_windowedWidth, g_windowedHeight, 0);
    }

    return g_fullscreen;
}

bool IsFullscreen() {
    return g_fullscreen;
}

void SetFullscreen(bool fullscreen) {
    if (g_fullscreen == fullscreen) return;

    g_fullscreen = fullscreen;

    if (!g_window) return;

    if (g_fullscreen) {
        // Save windowed position and size
        glfwGetWindowPos(g_window, &g_windowedX, &g_windowedY);
        glfwGetWindowSize(g_window, &g_windowedWidth, &g_windowedHeight);

        // The monitor the window is on, not whichever one is primary.
        GLFWmonitor* monitor = MonitorForWindow(g_window);
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);

        // Switch to fullscreen
        glfwSetWindowMonitor(g_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    } else {
        // Restore windowed mode
        glfwSetWindowMonitor(g_window, nullptr, g_windowedX, g_windowedY, g_windowedWidth, g_windowedHeight, 0);
    }
}

float GetElapsedTime() {
    return static_cast<float>(glfwGetTime());
}

} // namespace AppState
