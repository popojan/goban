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
#include <Input.h>
#include <Shell.h>

#if defined RMLUI_PLATFORM_WIN32
  #undef __GNUC__
  #include <io.h>
  #include <fcntl.h>
#endif

//#define SHOW_CONSOLE 1

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <memory>


Rml::Context* context = nullptr;
std::shared_ptr<Configuration> config;

void GameLoop() {
    dynamic_cast<ElementGame*>(context->GetDocument("game_window")->GetElementById("game"))->gameLoop();
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

const Rml::String APP_PATH(".");

const char * WINDOW_NAME = "Goban";

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

    std::shared_ptr<ShellRenderInterfaceOpenGL> popengl_renderer(new ShellRenderInterfaceOpenGL());
    ShellRenderInterfaceOpenGL *shell_renderer = popengl_renderer.get();

    // Generic OS initialisation, creates a window and attaches OpenGL.

    if (!Shell::Initialise(APP_PATH) ||
        !Shell::OpenWindow(WINDOW_NAME, shell_renderer, window_width, window_height, true))
    {
        spdlog::critical("cannot Shell::OpenWindow.");
        Shell::Shutdown();
        return -1;
    }

    // Rocket initialisation.
    Rml::SetRenderInterface(popengl_renderer.get());
    //(&opengl_renderer)->SetViewport(window_width, window_height);

    ShellSystemInterface system_interface;
    Rml::SetSystemInterface(&system_interface);

    //std::locale::global(std::locale("C"));

    Rml::Initialise();
    Rml::Initialise();

    config = std::make_shared<Configuration>(configurationFile);

    // Create the main Rocket context and set it on the shell's input layer.
    context = Rml::CreateContext("main",
            Rml::Vector2i(window_width, window_height));
    if (context == nullptr)
    {
        Rml::Shutdown();
        Shell::Shutdown();
        return -1;
    }

    Rml::Debugger::Initialise(context);
    //Rml::Debugger::SetVisible(true);
    Input::SetContext(context);
	shell_renderer->SetContext(context);

	using nlohmann::json;
	auto fonts = config->data
	        .value("fonts", json({}))
	        .value("gui", json::array());

    Shell::LoadFonts(fonts);

    Rml::ElementInstancer* element_instancer = new Rml::ElementInstancerGeneric< ElementGame >();
    Rml::Factory::RegisterElementInstancer("game", element_instancer);
    element_instancer->RemoveReference();

    auto event_instancer = new EventInstancer();
    Rml::Factory::RegisterEventListenerInstancer(event_instancer);
    event_instancer->RemoveReference();

    EventManager::SetPrefix(config->data.value("gui","./data/gui").c_str());
    EventManager::RegisterEventHandler("goban", new EventHandlerNewGame());
    
    // Initialize file chooser
    auto fileChooserHandler = new EventHandlerFileChooser();
    EventManager::RegisterEventHandler("open", fileChooserHandler);
    fileChooserHandler->LoadDialog(context);

    //Shell::ToggleFullscreen();

    auto window = EventManager::LoadWindow("goban", context);

    if(window) {
		Shell::EventLoop(GameLoop);
    }
    else {
        spdlog::critical("cannot create window, exiting immediately");
        return 13;
    }

    fileChooserHandler->UnloadDialog(context);
    EventManager::Shutdown();

    spdlog::debug("Before context destroy");;

    context->RemoveReference();
    context = nullptr;

    spdlog::debug("Before Rocket shutdown");

    Rml::ReleaseTextures();
    Rml::Shutdown();
    spdlog::debug("Before Window Close");

    Shell::CloseWindow();
    Shell::Shutdown();

    return 0;
}
