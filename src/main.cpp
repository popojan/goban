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

#include <Rocket/Core.h>
#include <Rocket/Controls.h>
#include <Rocket/Debugger.h>
#include <Input.h>
#include <Shell.h>
#include "ElementGame.h"
#include "EventManager.h"
#include "EventInstancer.h"
#include "EventHandlerNewGame.h"

#if defined ROCKET_PLATFORM_WIN32
  #undef __GNUC__
#endif

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

Rocket::Core::Context* context = NULL;
static ElementGame* gameElement = NULL;

void GameLoop() {
    gameElement->gameLoop();
}


#if defined ROCKET_PLATFORM_WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>



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
int main(int ROCKET_UNUSED_PARAMETER(argc), char** ROCKET_UNUSED_PARAMETER(argv))
#endif
{
#ifdef ROCKET_PLATFORM_WIN32
    ROCKET_UNUSED(instance_handle);
    ROCKET_UNUSED(previous_instance_handle);
    ROCKET_UNUSED(command_line);
    ROCKET_UNUSED(command_show);
#else
    ROCKET_UNUSED(argc);
    ROCKET_UNUSED(argv);
#endif

const Rocket::Core::String APP_PATH(".");

const char * WINDOW_NAME = "Goban";

#ifdef ROCKET_PLATFORM_WIN32
        DoAllocConsole();
#endif
    auto console = spdlog::stderr_color_mt("console");
    spdlog::set_level(spdlog::level::info);
    console->info("TODO log using spdlog!");

    unsigned window_width = 1024;
    unsigned window_height = 768;

    ShellRenderInterfaceOpenGL opengl_renderer;
    ShellRenderInterfaceExtensions *shell_renderer = &opengl_renderer;


    // Generic OS initialisation, creates a window and attaches OpenGL.

    if (!Shell::Initialise(APP_PATH) ||
        !Shell::OpenWindow(WINDOW_NAME, shell_renderer, window_width, window_height, true))
    {
        Shell::Shutdown();
        return -1;
    }

    // Rocket initialisation.
    Rocket::Core::SetRenderInterface(&opengl_renderer);
    //(&opengl_renderer)->SetViewport(window_width, window_height);

    ShellSystemInterface system_interface;
    Rocket::Core::SetSystemInterface(&system_interface);

    Rocket::Core::Initialise();
    Rocket::Controls::Initialise();

    glewExperimental = 1; //win
    GLenum err = glewInit(); //win
    if (GLEW_OK != err)    { //win
        printf("Error: %s\n",glewGetErrorString(err)); //win
    }

    // Create the main Rocket context and set it on the shell's input layer.
    context = Rocket::Core::CreateContext("main", Rocket::Core::Vector2i(window_width, window_height));
    if (context == NULL)
    {
        Rocket::Core::Shutdown();
        Shell::Shutdown();
        return -1;
    }

    Rocket::Debugger::Initialise(context);
    //Rocket::Debugger::SetVisible(true);
    Input::SetContext(context);
	std::cerr << "preSetContext = " << glGetError() << std::endl;
	shell_renderer->SetContext(context);
	std::cerr << "preLoadFonts = " << glGetError() << std::endl;
    Shell::LoadFonts("data/gui/");

    Rocket::Core::ElementInstancer* element_instancer = new Rocket::Core::ElementInstancerGeneric< ElementGame >();
    Rocket::Core::Factory::RegisterElementInstancer("game", element_instancer);
    element_instancer->RemoveReference();

    EventInstancer* event_instancer = new EventInstancer();
    Rocket::Core::Factory::RegisterEventListenerInstancer(event_instancer);
    event_instancer->RemoveReference();

    EventManager::RegisterEventHandler("goban", new EventHandlerNewGame());

    //Shell::ToggleFullscreen();

	std::cerr << "preLoadWindow = " << glGetError() << std::endl;

    if(EventManager::LoadWindow("goban")) {
        gameElement = dynamic_cast<ElementGame*>(context->GetDocument("game_window")->GetElementById("game"));
		std::cerr << "postLoadWindow = " << glGetError() << std::endl;
		Shell::EventLoop(GameLoop);
    }

    std::cerr << "Before context destroy" << std::endl;

    context->RemoveReference();
    context = 0;

    std::cerr << "Before Rocket shutdown" << std::endl;

    Rocket::Core::ReleaseTextures();
    Rocket::Core::Shutdown();
    std::cerr << "Before Window Close" << std::endl;

    Shell::CloseWindow();
    Shell::Shutdown();

    return 0;
}
