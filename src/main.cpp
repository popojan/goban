/*
 * Copyright (c) 2006 - 2008
 * Wandering Monster Studios Limited
 *
 * Any use of this program is governed by the terms of Wandering Monster
 * Studios Limited's Licence Agreement included with this program, a copy
 * of which can be obtained by contacting Wandering Monster Studios
 * Limited at info@wanderingmonster.co.nz.
 *
 */

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

#include <memory>

Rml::Context* context = nullptr;
std::shared_ptr<Configuration> config;

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

// GLFW callbacks
static void GlfwErrorCallback(int error, const char* description) {
    spdlog::error("GLFW Error {}: {}", error, description);
}

static void GlfwWindowSizeCallback(GLFWwindow* window, int width, int height) {
    (void)window;
    if (context) {
        context->SetDimensions(Rml::Vector2i(width, height));
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
    for (const auto& font : fonts) {
        std::string fontPath = font.get<std::string>();
        if (!Rml::LoadFontFace(fontPath.c_str())) {
            spdlog::warn("Failed to load font: {}", fontPath);
        } else {
            spdlog::info("Loaded font: {}", fontPath);
        }
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

int APIENTRY WinMain(HINSTANCE RMLUI_UNUSED_PARAMETER(instance_handle), HINSTANCE RMLUI_UNUSED_PARAMETER(previous_instance_handle), char* RMLUI_UNUSED_PARAMETER(command_line), int RMLUI_UNUSED_PARAMETER(command_show))
#else
int main(int argc, char** argv)
#endif
{
    using namespace clipp;
    std::string logLevel("warning");
    std::string configurationFile("./data/config.json");
    auto cli = (
        option("-v", "--verbosity") & word("level", logLevel),
        option("-c", "--config") & value("file", configurationFile)
    );
#ifdef RMLUI_PLATFORM_WIN32
    RMLUI_UNUSED(instance_handle);
    RMLUI_UNUSED(previous_instance_handle);
    RMLUI_UNUSED(command_line);
    RMLUI_UNUSED(command_show);
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

    int window_width = 1024;
    int window_height = 768;

    // Initialize GLFW
    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit()) {
        spdlog::critical("Failed to initialize GLFW");
        return -1;
    }

    // Request OpenGL 2.1 compatibility profile for GL2 renderer
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    // Create GLFW window
    GLFWwindow* window = glfwCreateWindow(window_width, window_height, WINDOW_NAME, nullptr, nullptr);
    if (!window) {
        spdlog::critical("Failed to create GLFW window");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Store window in AppState for fullscreen toggle etc.
    AppState::SetWindow(window);

    // Initialize glad
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        spdlog::critical("Failed to initialize GLAD");
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    spdlog::info("OpenGL version: {}", (const char*)glGetString(GL_VERSION));

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

    // Create the main RmlUi context
    context = Rml::CreateContext("main", Rml::Vector2i(window_width, window_height));
    if (context == nullptr) {
        spdlog::critical("Failed to create RmlUi context");
        Rml::Shutdown();
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
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

    EventManager::SetPrefix(config->data.value("gui", "./data/gui").c_str());
    EventManager::RegisterEventHandler("goban", new EventHandlerNewGame());

    // Initialize file chooser
    auto fileChooserHandler = new EventHandlerFileChooser();
    EventManager::RegisterEventHandler("open", fileChooserHandler);
    fileChooserHandler->LoadDialog(context);

    auto windowLoaded = EventManager::LoadWindow("goban", context);

    if (windowLoaded) {
        // Main loop
        while (!glfwWindowShouldClose(window) && !AppState::ExitRequested()) {
            glfwPollEvents();

            // Get game element and run game loop
            auto gameDoc = context->GetDocument("game_window");
            if (gameDoc) {
                auto gameElement = dynamic_cast<ElementGame*>(gameDoc->GetElementById("game"));
                if (gameElement) {
                    gameElement->gameLoop();
                }
            }

            // Clear and render
            int fb_width, fb_height;
            glfwGetFramebufferSize(window, &fb_width, &fb_height);
            glViewport(0, 0, fb_width, fb_height);

            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // RmlUi rendering
            render_interface.BeginFrame();
            context->Render();
            render_interface.EndFrame();

            glfwSwapBuffers(window);
        }
    } else {
        spdlog::critical("Cannot create window, exiting immediately");
        fileChooserHandler->UnloadDialog(context);
        EventManager::Shutdown();
        Rml::Shutdown();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 13;
    }

    fileChooserHandler->UnloadDialog(context);
    EventManager::Shutdown();

    spdlog::debug("Before context destroy");

    // Context is cleaned up by Rml::Shutdown()
    context = nullptr;

    spdlog::debug("Before RmlUi shutdown");

    Rml::ReleaseTextures();
    Rml::Shutdown();

    spdlog::debug("Before window close");

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
