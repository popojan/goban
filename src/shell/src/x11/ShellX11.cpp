/*
 * This source file is part of libRocket, the HTML/CSS Interface Middleware
 *
 * For the latest information, see http://www.librocket.com
 *
 * Copyright (c) 2008-2010 CodePoint Ltd, Shift Technology Ltd
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <Shell.h>
#include <Rocket/Core.h>
#include "ShellFileInterface.h"
#include <x11/InputX11.h>
#include <X11/Xlib.h>
#include <X11/extensions/xf86vmode.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>

static bool running = false;
static int screen = -1;
static timeval start_time;

static ShellFileInterface* file_interface = NULL;

bool Shell::Initialise(const Rocket::Core::String& path)
{
	gettimeofday(&start_time, NULL);
	InputX11::Initialise();

	file_interface = new ShellFileInterface(path);
	Rocket::Core::SetFileInterface(file_interface);

	return true;
}

void Shell::Shutdown()
{
	InputX11::Shutdown();

	delete file_interface;
	file_interface = NULL;
}

static Display* display = NULL;
static XVisualInfo* visual_info = NULL;
static Window window = 0;

static ShellRenderInterfaceExtensions *shell_renderer = NULL;

bool Shell::OpenWindow(const char* name, ShellRenderInterfaceExtensions *_shell_renderer, unsigned int width, unsigned int height, bool allow_resize)
{
	display = XOpenDisplay(0);
  	if (display == NULL)
		return false;

	// This initialise they keyboard to keycode mapping system of X11
	// itself.  It must be done here as it needs to query the connected
	// X server display for information about its install keymap abilities.
	InputX11::InitialiseX11Keymap(display);

	screen = XDefaultScreen(display);

	// Fetch an appropriate 32-bit visual interface.
	int attribute_list[] = {GLX_RGBA,
							GLX_DOUBLEBUFFER,
							GLX_RED_SIZE, 8,
							GLX_GREEN_SIZE, 8,
							GLX_BLUE_SIZE, 8,
                            GLX_ALPHA_SIZE, 0,
							GLX_DEPTH_SIZE, 8,
							None};
    if(!gladLoadGLX(display, screen)) {
        spdlog::critical("Error: cannot initialize GLX");
    }
	visual_info = glXChooseVisual(display, screen, attribute_list);
	if (visual_info == NULL)
	{
		return false;
  	}


	// Build up our window attributes.
	XSetWindowAttributes window_attributes;
	window_attributes.colormap = XCreateColormap(display, RootWindow(display, visual_info->screen), visual_info->visual, AllocNone);
	window_attributes.border_pixel = 0;
	window_attributes.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask;

	// Create the window.
	window = XCreateWindow(display,
						   RootWindow(display, visual_info->screen),
						   0, 0,
						   width, height,
						   0,
						   visual_info->depth,
						   InputOutput,
						   visual_info->visual,
						   CWBorderPixel | CWColormap | CWEventMask,
						   &window_attributes);

	// Handle delete events in windowed mode.
	Atom delete_atom = XInternAtom(display, "WM_DELETE_WINDOW", True);
	XSetWMProtocols(display, window, &delete_atom, 1);

	// Capture the events we're interested in.
	XSelectInput(display, window, ExposureMask |
                                  KeyPressMask |
								  KeyReleaseMask |
								  ButtonPressMask |
								  ButtonReleaseMask |
								  PointerMotionMask |
								  StructureNotifyMask);

	if(!allow_resize)
	{
		// Force the window to remain at the fixed size by asking the window manager nicely, it may choose to ignore us
		XSizeHints* win_size_hints = XAllocSizeHints();		// Allocate a size hint structure
		if (win_size_hints == NULL)
		{
			fprintf(stderr, "XAllocSizeHints - out of memory\n");
		}
		else
		{
			// Initialize the structure and specify which hints will be providing
			win_size_hints->flags = PSize | PMinSize | PMaxSize;

			// Set the sizes we want the window manager to use
			win_size_hints->base_width = width;
			win_size_hints->base_height = height;
			win_size_hints->min_width = width;
			win_size_hints->min_height = height;
			win_size_hints->max_width = width;
			win_size_hints->max_height = height;

			// {ass the size hints to the window manager.
			XSetWMNormalHints(display, window, win_size_hints);

			// Free the size buffer
			XFree(win_size_hints);
		}
	}

	// Set the window title and show the window.
	XSetStandardProperties(display, window, name, "", None, NULL, 0, NULL);
	XMapRaised(display, window);

	shell_renderer = _shell_renderer;
	if(shell_renderer != NULL)
	{
		struct __X11NativeWindowData nwData;
		nwData.display = display;
		nwData.window = window;
		nwData.visual_info = visual_info;
		return shell_renderer->AttachToNative(&nwData);
	}
    return true;
}

void Shell::CloseWindow()
{
	if(shell_renderer != NULL)
	{
		shell_renderer->DetachFromNative();
	}

    XDestroyWindow(display, window);

	if (visual_info != NULL) {
        XFree(visual_info);
    }

	if (display != NULL)
	{
		XCloseDisplay(display);
		display = NULL;
	}
}

// Returns a platform-dependent handle to the window.
void* Shell::GetWindowHandle()
{
	return NULL;
}

void Shell::EventLoop(ShellIdleFunction idle_function)
{
	running = true;

	// Loop on Peek/GetMessage until and exit has been requested
	while (running)
	{
		while (XPending(display) > 0)
		{
			XEvent event;
			char *event_type = NULL;
			XNextEvent(display, &event);


			switch (event.type)
			{

                case Expose:
                {
                    shell_renderer->Invalidate();
                }
                break;

				case ClientMessage: 
				{
					// The only message we register for is WM_DELETE_WINDOW, so if we receive a client message then the
					// window has been closed.
					event_type = XGetAtomName(display, event.xclient.message_type);
					if (strcmp(event_type, "WM_PROTOCOLS") == 0)
						running = false;
					XFree(event_type);
					event_type = NULL;
				}
				break;

				case ConfigureNotify: 
				{
					int x = event.xconfigure.width;
					int y = event.xconfigure.height;

					shell_renderer->SetViewport(x, y);
				}
				break;

				default:
				{
					InputX11::ProcessXEvent(display, event);
				}
			}
		}

		idle_function();
	}
}


#define _NET_WM_STATE_TOGGLE    2
static int fghResizeFullscrToggle(void)
{
    XWindowAttributes attributes;
    
    if(false) {
        /* restore original window size 
        SFG_Window *win = fgStructure.CurrentWindow;
        fgStructure.CurrentWindow->State.NeedToResize = GL_TRUE;
        fgStructure.CurrentWindow->State.Width  = win->State.OldWidth;
        fgStructure.CurrentWindow->State.Height = win->State.OldHeight;
    */
    } else {
        /* resize the window to cover the entire screen */
        XGetWindowAttributes(display, window, &attributes);

        /*
 *          * The "x" and "y" members of "attributes" are the window's coordinates
 *                   * relative to its parent, i.e. to the decoration window.
 *                            */
        XMoveResizeWindow(display,
                window,
                -attributes.x,
                -attributes.y,
                WidthOfScreen(ScreenOfDisplay(display, DefaultScreen(display))),
                HeightOfScreen(ScreenOfDisplay(display, DefaultScreen(display))));
    }
    return 0;
}

static int fghEwmhFullscrToggle(void)
{
    XEvent xev;
    long evmask = SubstructureRedirectMask | SubstructureNotifyMask;

    //if(!fgDisplay.State || !fgDisplay.StateFullScreen) {
    //    return -1;
    //}

    xev.type = ClientMessage;
    xev.xclient.window = window;
    xev.xclient.message_type = XInternAtom(display, "_NET_WM_STATE", False);
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = _NET_WM_STATE_TOGGLE;
    xev.xclient.data.l[1] = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
    xev.xclient.data.l[2] = 0;  /* no second property to toggle */
    xev.xclient.data.l[3] = 1;  /* source indication: application */
    xev.xclient.data.l[4] = 0;  /* unused */

    if(!XSendEvent(display, DefaultRootWindow(display), 0, evmask, &xev)) {
        return -1;
    }
    return 0;
}

bool Shell::ToggleFullscreen() {
    if(fghEwmhFullscrToggle() != -1) {
        return false;
    }

    /* fall back to resizing the window */
    if(fghResizeFullscrToggle() != -1) {
        return false;
    }
    return true;
}
void Shell::RequestExit()
{
	running = false;
}

void Shell::DisplayError(const char* fmt, ...)
{
	const int buffer_size = 1024;
	char buffer[buffer_size];
	va_list argument_list;

	// Print the message to the buffer.
	va_start(argument_list, fmt);
	int len = vsnprintf(buffer, buffer_size - 2, fmt, argument_list);	
	if ( len < 0 || len > buffer_size - 2 )	
	{
		len = buffer_size - 2;
	}	
	buffer[len] = '\n';
	buffer[len + 1] = '\0';
	va_end(argument_list);

	printf("%s", buffer);
}

void Shell::Log(const char* fmt, ...)
{
	const int buffer_size = 1024;
	char buffer[buffer_size];
	va_list argument_list;

	// Print the message to the buffer.
	va_start(argument_list, fmt);
	int len = vsnprintf(buffer, buffer_size - 2, fmt, argument_list);	
	if ( len < 0 || len > buffer_size - 2 )	
	{
		len = buffer_size - 2;
	}	
	buffer[len] = '\n';
	buffer[len + 1] = '\0';
	va_end(argument_list);

	printf("%s", buffer);
}

// Returns the seconds that have elapsed since program startup.
float Shell::GetElapsedTime() 
{
	struct timeval now;

	gettimeofday(&now, NULL);

	double sec = now.tv_sec - start_time.tv_sec;
	double usec = now.tv_usec - start_time.tv_usec;
	double result = sec + (usec / 1000000.0);

	return (float)result;
}
