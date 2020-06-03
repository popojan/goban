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
#include <boost/process.hpp>
#include <windows.h>
#include <glad/glad.h>

#include "ElementGame.h"
#include <Rocket/Core.h>
#include <Rocket/Controls.h>
#include <Rocket/Debugger.h>
#include "EventManager.h"
#include "EventInstancer.h"
#include "EventHandlerNewGame.h"
#include <Input.h>
#include <Shell.h>
#include <locale>

#if defined ROCKET_PLATFORM_WIN32
  #undef __GNUC__
  #include <io.h>
  #include <fcntl.h>
#endif

#include <spdlog/spdlog.h>
#include <memory>

Rocket::Core::Context* context = NULL;
std::shared_ptr<Configuration> config;

void GameLoop() {
    dynamic_cast<ElementGame*>(context->GetDocument("game_window")->GetElementById("game"))->gameLoop();
}


#if defined ROCKET_PLATFORM_WIN32

void DoAllocConsole()
{
    static const WORD MAX_CONSOLE_LINES = 500;
    int hConHandle;
    long lStdHandle;
    CONSOLE_SCREEN_BUFFER_INFO coninfo;
    FILE *fp;

    // allocate a console for this app
    AllocConsole();

    // set the screen buffer to be big enough to let us scroll text
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
    coninfo.dwSize.Y = MAX_CONSOLE_LINES;
    SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);

    // redirect unbuffered STDOUT to the console
    lStdHandle = (long)GetStdHandle(STD_OUTPUT_HANDLE);
    hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
    fp = _fdopen(hConHandle, "w");

    *stdout = *fp;
    setvbuf(stdout, NULL, _IONBF, 0);

    // redirect unbuffered STDIN to the console
    lStdHandle = (long)GetStdHandle(STD_INPUT_HANDLE);
    hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
    fp = _fdopen(hConHandle, "r");

    *stdin = *fp;
    setvbuf(stdin, NULL, _IONBF, 0);

    // redirect unbuffered STDERR to the console
    lStdHandle = (long)GetStdHandle(STD_ERROR_HANDLE);
    hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
    fp = _fdopen(hConHandle, "w");
    *stderr = *fp;

    setvbuf(stderr, NULL, _IONBF, 0);
    ShowWindow(GetConsoleWindow(), SW_SHOW);
}

int APIENTRY WinMain(HINSTANCE ROCKET_UNUSED_PARAMETER(instance_handle), HINSTANCE ROCKET_UNUSED_PARAMETER(previous_instance_handle), char* ROCKET_UNUSED_PARAMETER(command_line), int ROCKET_UNUSED_PARAMETER(command_show))
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
#ifdef ROCKET_PLATFORM_WIN32
    ROCKET_UNUSED(instance_handle);
    ROCKET_UNUSED(previous_instance_handle);
    ROCKET_UNUSED(command_line);
    ROCKET_UNUSED(command_show);
    parse(__argc, __argv, cli);
#else
    parse(argc, argv, cli);
#endif

const Rocket::Core::String APP_PATH(".");

const char * WINDOW_NAME = "Goban";

#ifdef ROCKET_PLATFORM_WIN32
        DoAllocConsole();
#endif
    spdlog::set_level(spdlog::level::from_str(logLevel));

    unsigned window_width = 1024;
    unsigned window_height = 768;

    ShellRenderInterfaceOpenGL opengl_renderer;
    ShellRenderInterfaceExtensions *shell_renderer = &opengl_renderer;


    // Generic OS initialisation, creates a window and attaches OpenGL.

    if (!Shell::Initialise(APP_PATH) ||
        !Shell::OpenWindow(WINDOW_NAME, shell_renderer, window_width, window_height, true))
    {
        spdlog::critical("cannot Shell::OpenWindow.");
        Shell::Shutdown();
        return -1;
    }

    // Rocket initialisation.
    Rocket::Core::SetRenderInterface(&opengl_renderer);
    //(&opengl_renderer)->SetViewport(window_width, window_height);

    ShellSystemInterface system_interface;
    Rocket::Core::SetSystemInterface(&system_interface);

    //std::locale::global(std::locale("C"));

    Rocket::Core::Initialise();
    Rocket::Controls::Initialise();

    if(!gladLoadGL()) {
        spdlog::critical("Error: cannot initialize GL");
        //throw std::runtime_error("cannot initialize GL");
    }

    config.reset(new Configuration(configurationFile));

    // Create the main Rocket context and set it on the shell's input layer.
    context = Rocket::Core::CreateContext("main",
            Rocket::Core::Vector2i(window_width, window_height));
    if (context == NULL)
    {
        Rocket::Core::Shutdown();
        Shell::Shutdown();
        return -1;
    }

    Rocket::Debugger::Initialise(context);
    //Rocket::Debugger::SetVisible(true);
    Input::SetContext(context);
	shell_renderer->SetContext(context);

	using nlohmann::json;
	auto fonts = config->data
	        .value("fonts", json({}))
	        .value("gui", json::array());

    Shell::LoadFonts(fonts);

    Rocket::Core::ElementInstancer* element_instancer = new Rocket::Core::ElementInstancerGeneric< ElementGame >();
    Rocket::Core::Factory::RegisterElementInstancer("game", element_instancer);
    element_instancer->RemoveReference();

    EventInstancer* event_instancer = new EventInstancer();
    Rocket::Core::Factory::RegisterEventListenerInstancer(event_instancer);
    event_instancer->RemoveReference();

    EventManager::RegisterEventHandler("goban", new EventHandlerNewGame());

    //Shell::ToggleFullscreen();

    auto window = EventManager::LoadWindow("goban");

    if(window) {
		Shell::EventLoop(GameLoop);
    }
    else {
        spdlog::critical("cannot create window, exiting immediately");
        return 13;
    }
    EventManager::Shutdown();

    spdlog::debug("Before context destroy");;

    context->RemoveReference();
    context = 0;

    spdlog::debug("Before Rocket shutdown");

    Rocket::Core::ReleaseTextures();
    Rocket::Core::Shutdown();
    spdlog::debug("Before Window Close");

    Shell::CloseWindow();
    Shell::Shutdown();

    return 0;
}
