// Windows headers must come first to avoid conflicts
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX  // Prevent windows.h from defining min/max macros
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#endif

#include <clipp.h>

#include "ElementGame.h"
#include <RmlUi/Core.h>
#include <RmlUi/Debugger.h>
#include "EventManager.h"
#include "EventInstancer.h"
#include "EventHandlerNewGame.h"
#include "EventHandlerFileChooser.h"
#include "FileChooserDataSource.h"
#include "AppState.h"

// GLFW and RmlUi backends
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
#include <RmlUi_Platform_GLFW.h>
#include <RmlUi_Renderer_GL2.h>

#if defined RMLUI_PLATFORM_WIN32
  #undef __GNUC__
  #include <io.h>
  #include <fcntl.h>
#endif

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <nlohmann/json.hpp>

#include <memory>
#include <set>

#include "UserSettings.h"

Rml::Context* context = nullptr;

// Global hover listener that triggers repaint for any document
// This ensures hover states work correctly with event-driven rendering
class HoverRepaintListener : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& event) override {
        (void)event;
        // Trigger repaint on CSS state changes from ANY document
        // The repaint trigger (ElementGame) lives in game_window
        if (context) {
            if (auto doc = context->GetDocument("game_window")) {
                if (auto gameElement = dynamic_cast<ElementGame*>(doc->GetElementById("game"))) {
                    gameElement->requestRepaint();
                }
            }
        }
    }
};

static HoverRepaintListener g_hoverListener;
static std::set<Rml::ElementDocument*> g_documentsWithHoverListener;
std::shared_ptr<Configuration> config;

// Add hover listeners to any new documents in the context
// This ensures all dialogs (current and future) have hover responsiveness
static void EnsureHoverListenersForAllDocuments() {
    if (!context) return;

    for (int i = 0; i < context->GetNumDocuments(); ++i) {
        auto doc = context->GetDocument(i);
        if (doc && g_documentsWithHoverListener.find(doc) == g_documentsWithHoverListener.end()) {
            // Events that can change CSS visual state and need repaint:
            // :hover -> mouseover/mouseout (shows submenus via CSS)
            // click  -> complete click cycle (RmlUi select dropdowns, buttons with onmouseup)
            // change -> dropdown selection changes (from syncDropdown in OnUpdate)
            doc->AddEventListener(Rml::EventId::Mouseover, &g_hoverListener);
            doc->AddEventListener(Rml::EventId::Mouseout, &g_hoverListener);
            doc->AddEventListener(Rml::EventId::Click, &g_hoverListener);
            doc->AddEventListener(Rml::EventId::Change, &g_hoverListener);
            g_documentsWithHoverListener.insert(doc);
            spdlog::debug("Added hover listeners to document: {}", doc->GetSourceURL().c_str());
        }
    }
}

// For application restart with different config
static char* g_executable_path = nullptr;
static std::string g_pending_restart_config;

void RequestRestart(const std::string& configFile) {
    g_pending_restart_config = configFile;
    UserSettings::instance().setLastConfig(configFile);
    AppState::RequestExit();
}

bool HasPendingRestart() {
    return !g_pending_restart_config.empty();
}

void ExecuteRestart() {
    if (g_pending_restart_config.empty() || !g_executable_path) return;

    spdlog::info("Restarting with config: {}", g_pending_restart_config);

#ifdef _WIN32
    // Windows: use _spawnv to start new process
    std::vector<const char*> args;
    args.push_back(g_executable_path);
    args.push_back("--config");
    args.push_back(g_pending_restart_config.c_str());
    args.push_back(nullptr);

    intptr_t result = _spawnv(_P_NOWAIT, g_executable_path, args.data());
    if (result == -1) {
        spdlog::error("Failed to restart: {}", strerror(errno));
    }
    // On Windows, we exit current process after spawning new one
#else
    // Unix: use execv to replace current process
    std::vector<char*> args;
    args.push_back(g_executable_path);
    char config_flag[] = "--config";
    args.push_back(config_flag);
    char* config_path = strdup(g_pending_restart_config.c_str());
    args.push_back(config_path);
    args.push_back(nullptr);

    execv(g_executable_path, args.data());
    // If execv returns, it failed
    spdlog::error("Failed to restart: {}", strerror(errno));
    free(config_path);
#endif
}

// Custom SystemInterface to route RmlUi logs to spdlog
class GobanSystemInterface : public SystemInterface_GLFW {
public:
    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override {
        switch (type) {
            case Rml::Log::LT_ERROR:
                spdlog::error("[RmlUi] {}", message.c_str());
                break;
            case Rml::Log::LT_ASSERT:
                spdlog::critical("[RmlUi] {}", message.c_str());
                break;
            case Rml::Log::LT_WARNING:
                spdlog::warn("[RmlUi] {}", message.c_str());
                break;
            case Rml::Log::LT_INFO:
                spdlog::info("[RmlUi] {}", message.c_str());
                break;
            case Rml::Log::LT_DEBUG:
                spdlog::debug("[RmlUi] {}", message.c_str());
                break;
            default:
                spdlog::trace("[RmlUi] {}", message.c_str());
                break;
        }
        return true;
    }
};

static GobanSystemInterface system_interface;
static RenderInterface_GL2 render_interface;

// Centralized cleanup for graceful exit on errors
// Safe to call multiple times or with partially initialized resources
static void CleanupResources(GLFWwindow* window) {
    // Flush logs first to ensure all error messages are written
    if (spdlog::default_logger()) {
        spdlog::default_logger()->flush();
    }

    // Clear all GLFW callbacks to prevent accessing freed resources during cleanup
    // This is critical: callbacks can fire during glfwTerminate() and access spdlog
    glfwSetErrorCallback(nullptr);
    if (window) {
        glfwSetWindowSizeCallback(window, nullptr);
        glfwSetKeyCallback(window, nullptr);
        glfwSetCharCallback(window, nullptr);
        glfwSetCursorPosCallback(window, nullptr);
        glfwSetMouseButtonCallback(window, nullptr);
        glfwSetScrollCallback(window, nullptr);
    }

    // Cleanup RmlUi if it was initialized
    if (context) {
        context = nullptr;  // Will be destroyed by Rml::Shutdown()
    }
    Rml::Shutdown();  // Safe to call even if not initialized

    // Cleanup GLFW
    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();  // Safe to call even if not initialized
}

// GLFW callbacks
static void GlfwErrorCallback(int error, const char* description) {
    // Wayland doesn't support window positioning - demote to debug
    if (error == GLFW_FEATURE_UNAVAILABLE) {
        spdlog::debug("GLFW: {}", description);
    } else {
        spdlog::warn("GLFW Error {}: {}", error, description);
    }
}

static void GlfwWindowSizeCallback(GLFWwindow* window, int width, int height) {
    (void)window;
    if (context) {
        context->SetDimensions(Rml::Vector2i(width, height));
        // Trigger repaint on window resize
        if (auto gameDoc = context->GetDocument("game_window")) {
            if (auto gameElement = dynamic_cast<ElementGame*>(gameDoc->GetElementById("game"))) {
                gameElement->requestRepaint();
            }
        }
    }
    render_interface.SetViewport(width, height);
}

static void GlfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)window;
    (void)scancode;
    if (!context) return;

    // Toggle debugger with F8
    if (key == GLFW_KEY_F8 && action == GLFW_PRESS) {
        Rml::Debugger::SetVisible(!Rml::Debugger::IsVisible());
        return;
    }

    RmlGLFW::ProcessKeyCallback(context, key, action, mods);
}

static void GlfwCharCallback(GLFWwindow* window, unsigned int codepoint) {
    (void)window;
    if (context) {
        RmlGLFW::ProcessCharCallback(context, codepoint);
    }
}

static void GlfwCursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    if (context) {
        int mods = RmlGLFW::ConvertKeyModifiers(0);
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
            mods |= Rml::Input::KM_SHIFT;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS)
            mods |= Rml::Input::KM_CTRL;
        if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS)
            mods |= Rml::Input::KM_ALT;

        RmlGLFW::ProcessCursorPosCallback(context, window, xpos, ypos, mods);
    }
}

static void GlfwMouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    (void)window;
    if (!context) return;
    RmlGLFW::ProcessMouseButtonCallback(context, button, action, mods);
}

static void GlfwScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)xoffset;
    if (context) {
        int mods = 0;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
            mods |= Rml::Input::KM_SHIFT;

        RmlGLFW::ProcessScrollCallback(context, yoffset, mods);
    }
}

static void LoadFonts(const nlohmann::json& fonts) {
    size_t count = fonts.size();
    size_t i = 0;
    for (const auto& font : fonts) {
        std::string fontPath = font.get<std::string>();
        bool isFallback = (i == count - 1) && (count > 1);  // Last font is fallback
        if (!Rml::LoadFontFace(fontPath.c_str(), isFallback)) {
            spdlog::warn("Failed to load font: {}", fontPath);
        } else {
            spdlog::info("Loaded font{}: {}", isFallback ? " (fallback)" : "", fontPath);
        }
        ++i;
    }
}

#if defined RMLUI_PLATFORM_WIN32
void DoAllocConsole()
{
    static const WORD MAX_CONSOLE_LINES = 500;
    int hConHandle;
    intptr_t lStdHandle;
    CONSOLE_SCREEN_BUFFER_INFO coninfo;
    FILE *fp;

    // allocate a console for this app
    AllocConsole();

    // set the screen buffer to be big enough to let us scroll text
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
    coninfo.dwSize.Y = MAX_CONSOLE_LINES;
    SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);

    // redirect unbuffered STDOUT to the console
    lStdHandle = reinterpret_cast<intptr_t>(GetStdHandle(STD_OUTPUT_HANDLE));
    hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
    fp = _fdopen(hConHandle, "w");

    *stdout = *fp;
    setvbuf(stdout, NULL, _IONBF, 0);

    // redirect unbuffered STDIN to the console
    lStdHandle =  reinterpret_cast<intptr_t>(GetStdHandle(STD_INPUT_HANDLE));
    hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
    fp = _fdopen(hConHandle, "r");

    *stdin = *fp;
    setvbuf(stdin, NULL, _IONBF, 0);

    // redirect unbuffered STDERR to the console
    lStdHandle =  reinterpret_cast<intptr_t>(GetStdHandle(STD_ERROR_HANDLE));
    hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
    fp = _fdopen(hConHandle, "w");
    *stderr = *fp;

    setvbuf(stderr, NULL, _IONBF, 0);
    ShowWindow(GetConsoleWindow(), SW_HIDE);
}

int APIENTRY WinMain(HINSTANCE instance_handle, HINSTANCE previous_instance_handle, char* command_line, int command_show)
#else
int main(int argc, char** argv)
#endif
{
    // Store executable path for potential restart
#ifdef RMLUI_PLATFORM_WIN32
    static char exe_path[MAX_PATH];
    if (GetModuleFileNameA(NULL, exe_path, MAX_PATH) > 0) {
        g_executable_path = exe_path;
    }
#else
    g_executable_path = argv[0];
#endif

    using namespace clipp;
    std::string logLevel("warning");

    // Load user preferences
    UserSettings::instance().load();
    std::string configurationFile = UserSettings::instance().getLastConfig();
    bool startFullscreen = UserSettings::instance().getFullscreen();

    auto cli = (
        option("-v", "--verbosity") & word("level", logLevel),
        option("-c", "--config") & value("file", configurationFile)
    );
#ifdef RMLUI_PLATFORM_WIN32
    (void)instance_handle;
    (void)previous_instance_handle;
    (void)command_line;
    (void)command_show;
    parse(__argc, __argv, cli);
#else
    parse(argc, argv, cli);
#endif

    const char* WINDOW_NAME = "Goban";

#ifdef RMLUI_PLATFORM_WIN32
    DoAllocConsole();
#endif

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("last_run.log", true);
    spdlog::sinks_init_list sink_list = { file_sink, console_sink };
    auto logger = std::make_shared<spdlog::logger>("multi_sink", sink_list.begin(), sink_list.end());
    spdlog::set_default_logger(logger);
    logger->set_level(spdlog::level::from_str(logLevel));
    logger->flush_on(spdlog::level::from_str(logLevel));  // Flush immediately for crash debugging

    int window_width = 1024;
    int window_height = 768;

    // Initialize GLFW
    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit()) {
        spdlog::critical("Failed to initialize GLFW");
        CleanupResources(nullptr);
        return -1;
    }

    // Request OpenGL 2.1 compatibility profile for GL2 renderer
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    // Create GLFW window
    GLFWwindow* window = glfwCreateWindow(window_width, window_height, WINDOW_NAME, nullptr, nullptr);
    if (!window) {
        spdlog::critical("Failed to create GLFW window");
        CleanupResources(nullptr);
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Set window icon from executable resource (Windows only)
#ifdef _WIN32
    HWND hwnd = glfwGetWin32Window(window);
    HICON icon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(101));
    if (icon) {
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)icon);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);
    }
#endif

    // Store window in AppState for fullscreen toggle etc.
    AppState::SetWindow(window);

    // Apply saved fullscreen state
    if (startFullscreen) {
        AppState::SetFullscreen(true);
        // Get actual window size after fullscreen switch
        glfwGetFramebufferSize(window, &window_width, &window_height);
    }

    // Initialize glad
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        spdlog::critical("Failed to initialize GLAD");
        CleanupResources(window);
        return -1;
    }

    spdlog::info("OpenGL version: {}", reinterpret_cast<const char *>(glGetString(GL_VERSION)));

    // Show window immediately with black screen (before shader compilation)
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(window);

    // Set up GLFW callbacks
    glfwSetWindowSizeCallback(window, GlfwWindowSizeCallback);
    glfwSetKeyCallback(window, GlfwKeyCallback);
    glfwSetCharCallback(window, GlfwCharCallback);
    glfwSetCursorPosCallback(window, GlfwCursorPosCallback);
    glfwSetMouseButtonCallback(window, GlfwMouseButtonCallback);
    glfwSetScrollCallback(window, GlfwScrollCallback);

    // Initialize RmlUi
    render_interface.SetViewport(window_width, window_height);
    Rml::SetRenderInterface(&render_interface);
    Rml::SetSystemInterface(&system_interface);

    Rml::Initialise();

    config = std::make_shared<Configuration>(configurationFile);
    if (!config->valid) {
        spdlog::critical("Failed to load configuration from '{}'. Cannot continue.", configurationFile);
        CleanupResources(window);
        return -1;
    }

    // Create the main RmlUi context
    context = Rml::CreateContext("main", Rml::Vector2i(window_width, window_height));
    if (context == nullptr) {
        spdlog::critical("Failed to create RmlUi context");
        CleanupResources(window);
        return -1;
    }

    // Ensure dimensions are set correctly at startup (callback not called on init)
    {
        int w, h;
        glfwGetWindowSize(window, &w, &h);
        GlfwWindowSizeCallback(window, w, h);
    }

    Rml::Debugger::Initialise(context);

    // Load fonts
    using nlohmann::json;
    auto fonts = config->data
            .value("fonts", json({}))
            .value("gui", json::array());
    LoadFonts(fonts);

    // Register element and event instancers
    // RmlUi takes ownership of the raw pointers
    Rml::Factory::RegisterElementInstancer("game", new Rml::ElementInstancerGeneric<ElementGame>());
    Rml::Factory::RegisterEventListenerInstancer(new EventInstancer());

    EventManager::SetPrefix(config->data.value("gui", "./config/gui").c_str());
    EventManager::RegisterEventHandler("goban", new EventHandlerNewGame());

    // Initialize file chooser
    auto fileChooserHandler = new EventHandlerFileChooser();
    EventManager::RegisterEventHandler("open", fileChooserHandler);
    fileChooserHandler->LoadDialog(context);

    if (EventManager::LoadWindow("goban", context)) {
        // Main loop
        while (!glfwWindowShouldClose(window) && !AppState::ExitRequested()) {
            // Get game element
            auto gameDoc = context->GetDocument("game_window");
            ElementGame* gameElement = nullptr;
            if (gameDoc) {
                gameElement = dynamic_cast<ElementGame*>(gameDoc->GetElementById("game"));
            }

            // Process events
            glfwPollEvents();

            // Ensure all documents have hover listeners for proper repaint triggering
            EnsureHoverListenersForAllDocuments();

            // Run game loop (processes RmlUi events)
            if (gameElement) {
                gameElement->gameLoop();
            }

            // Render if needed (check AFTER event processing)
            if (!gameElement || gameElement->needsRender()) {
                render_interface.BeginFrame();
                context->Render();
                render_interface.EndFrame();
                glfwSwapBuffers(window);
            } else {
                // Nothing to render - wait for next event instead of busy-polling
                // Use timeout if FPS display needs one more update to show "0"
                double timeout = gameElement ? gameElement->getIdleTimeout() : -1.0;
                if (timeout > 0) {
                    glfwWaitEventsTimeout(timeout);
                } else {
                    glfwWaitEvents();
                }
            }
        }
    } else {
        spdlog::critical("Cannot create window, exiting immediately");
        fileChooserHandler->UnloadDialog(context);
        EventManager::Shutdown();
        CleanupResources(window);
        return 13;
    }

    fileChooserHandler->UnloadDialog(context);

    // Save current game and stop thread before destroying RmlUi elements
    spdlog::debug("Stopping game thread");
    if (auto gameDoc = context->GetDocument("game_window")) {
        if (auto gameElement = dynamic_cast<ElementGame*>(gameDoc->GetElementById("game"))) {
            gameElement->getController().saveCurrentGame();
            gameElement->getGameThread().shutdown();
        }
    }

    EventManager::Shutdown();

    // Clear document tracking before RmlUi cleanup
    g_documentsWithHoverListener.clear();

    spdlog::debug("Before context destroy");

    // Context is cleaned up by Rml::Shutdown()
    context = nullptr;

    spdlog::debug("Before RmlUi shutdown");

    Rml::ReleaseTextures();
    Rml::Shutdown();

    spdlog::debug("Before window close");

    // Unregister all callbacks before cleanup to prevent access to freed resources
    // Error callback especially important - SystemInterface_GLFW destructor triggers
    // GLFW errors after main() returns, when spdlog is already destroyed
    glfwSetErrorCallback(nullptr);
    glfwSetWindowSizeCallback(window, nullptr);
    glfwSetKeyCallback(window, nullptr);
    glfwSetCharCallback(window, nullptr);
    glfwSetCursorPosCallback(window, nullptr);
    glfwSetMouseButtonCallback(window, nullptr);
    glfwSetScrollCallback(window, nullptr);

    glfwDestroyWindow(window);
    glfwTerminate();

    // If restart was requested, execute it now (after cleanup)
    if (HasPendingRestart()) {
        ExecuteRestart();
    }

    return 0;
}
