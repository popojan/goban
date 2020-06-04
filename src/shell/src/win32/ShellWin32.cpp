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
#include <win32/InputWin32.h>
#include "ShellFileInterface.h"
//#include "resource.h"
//#include <windows.h>
#include <stdio.h>

static LRESULT CALLBACK WindowProcedure(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param);

static bool activated = true;
static bool running = true;
static const char* instance_name = NULL;
static HWND window_handle = NULL;
static HINSTANCE instance_handle = NULL;

static double time_frequency;
static LARGE_INTEGER time_startup;

static ShellFileInterface* file_interface = NULL;
static ShellRenderInterfaceExtensions *shell_renderer = NULL;

struct SavedWindowInfo {
	bool maximized;
	LONG style;
	LONG ex_style;
	RECT window_rect;
};

bool Shell::Initialise(const Rocket::Core::String& path)
{
	instance_handle = GetModuleHandle(NULL);
	InputWin32::Initialise();

	LARGE_INTEGER time_ticks_per_second;
	QueryPerformanceFrequency(&time_ticks_per_second);
	QueryPerformanceCounter(&time_startup);

	time_frequency = 1.0 / (double) time_ticks_per_second.QuadPart;

	// Fetch the path of the executable, append the path onto that.
	char executable_file_name[MAX_PATH];
	if (GetModuleFileNameA(instance_handle, executable_file_name, MAX_PATH) >= MAX_PATH &&
		GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		executable_file_name[0] = 0;

	executable_path = Rocket::Core::String(executable_file_name);
	executable_path = executable_path.Substring(0, executable_path.RFind("\\") + 1);
	file_interface = new ShellFileInterface(executable_path + path);
	Rocket::Core::SetFileInterface(file_interface);

	return true;
}

void Shell::Shutdown()
{
	InputWin32::Shutdown();

	delete file_interface;
	file_interface = NULL;
}

bool Shell::OpenWindow(const char* name, ShellRenderInterfaceExtensions *_shell_renderer, unsigned int width, unsigned int height, bool allow_resize)
{
	WNDCLASS window_class;

	// Fill out the window class struct.
	window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	window_class.lpfnWndProc = WindowProcedure;
	window_class.cbClsExtra = 0;
	window_class.cbWndExtra = 0;
	window_class.hInstance = instance_handle;
	window_class.hIcon = LoadIcon(instance_handle, MAKEINTRESOURCE(101));
	window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
	window_class.hbrBackground = NULL;
	window_class.lpszMenuName = NULL;
	window_class.lpszClassName = name;

	if (!RegisterClass(&window_class))
	{
		DisplayError("Could not register window class.");

		CloseWindow();
		return false;
	}

	window_handle = CreateWindowEx(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE,
								   name,	// Window class name.
								   name,
								   WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW,
								   0, 0,	// Window position.
								   width, height,// Window size.
								   NULL,
								   NULL,
								   instance_handle,
								   NULL);
	if (!window_handle)
	{
		DisplayError("Could not create window.");
		CloseWindow();

		return false;
	}

	instance_name = name;

	DWORD style = (allow_resize ? WS_OVERLAPPEDWINDOW : (WS_OVERLAPPEDWINDOW & ~WS_SIZEBOX & ~WS_MAXIMIZEBOX));
	DWORD extended_style = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;

	// Adjust the window size to take into account the edges
	RECT window_rect;
	window_rect.top = 0;
	window_rect.left = 0;
	window_rect.right = width;
	window_rect.bottom = height;
	AdjustWindowRectEx(&window_rect, style, FALSE, extended_style);

	SetWindowLong(window_handle, GWL_EXSTYLE, extended_style);
	SetWindowLong(window_handle, GWL_STYLE, style);

	if (_shell_renderer != NULL)
	{
		shell_renderer = _shell_renderer;
		if(!(shell_renderer->AttachToNative(window_handle)))
		{
			CloseWindow();
			return false;
		}
	}

	// Resize the window.
	SetWindowPos(window_handle, HWND_TOP, 0, 0, window_rect.right - window_rect.left, window_rect.bottom - window_rect.top, SWP_NOACTIVATE);

	// Display the new window
	ShowWindow(window_handle, SW_SHOW);
	SetForegroundWindow(window_handle);
	SetFocus(window_handle);

    return true;
}

void Shell::CloseWindow()
{
	if(shell_renderer) {
		shell_renderer->DetachFromNative();
	}

	DestroyWindow(window_handle);  
	UnregisterClass(instance_name, instance_handle);
}

// Returns a platform-dependent handle to the window.
void* Shell::GetWindowHandle()
{
	return window_handle;
}

void Shell::EventLoop(ShellIdleFunction idle_function)
{
	MSG message;
	running = true;

	// Loop on PeekMessage() / GetMessage() until exit has been requested.
	while (running)
	{
		if (PeekMessage(&message, NULL, 0, 0, PM_NOREMOVE))
		{
			GetMessage(&message, NULL, 0, 0);

			TranslateMessage(&message);
			DispatchMessage(&message);
		}

		idle_function();

		if (!activated)
			Sleep(100);
	}
	spdlog::warn("Exiting gracefully...");
	//exit(0);
}

void Shell::RequestExit()
{
	running = false;
}

bool Shell::ToggleFullscreen() {
	static bool fullscreen_ = false;
	bool for_metro = false;
	HWND hwnd_ = window_handle;
	static SavedWindowInfo saved_window_info_;

	bool fullscreen = !fullscreen_;
	// Save current window state if not already fullscreen.
	if (!fullscreen_) {
		// Save current window information.  We force the window into restored mode
		// before going fullscreen because Windows doesn't seem to hide the
		// taskbar if the window is in the maximized state.
		saved_window_info_.maximized = !!::IsZoomed(hwnd_);
		if (saved_window_info_.maximized)
			::SendMessage(hwnd_, WM_SYSCOMMAND, SC_RESTORE, 0);
		saved_window_info_.style = GetWindowLong(hwnd_, GWL_STYLE);
		saved_window_info_.ex_style = GetWindowLong(hwnd_, GWL_EXSTYLE);
		GetWindowRect(hwnd_, &saved_window_info_.window_rect);
	}

	fullscreen_ = fullscreen;

	if (fullscreen_) {
		// Set new window style and size.
		SetWindowLong(hwnd_, GWL_STYLE,
			saved_window_info_.style & ~(WS_CAPTION | WS_THICKFRAME));
		SetWindowLong(hwnd_, GWL_EXSTYLE,
			saved_window_info_.ex_style & ~(WS_EX_DLGMODALFRAME |
			WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

		// On expand, if we're given a window_rect, grow to it, otherwise do
		// not resize.
		if (!for_metro) {
			MONITORINFO monitor_info;
			monitor_info.cbSize = sizeof(monitor_info);
			GetMonitorInfo(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST),
				&monitor_info);
			//gfx::Rect window_rect(monitor_info.rcMonitor);
			SetWindowPos(hwnd_, NULL, monitor_info.rcMonitor.left, monitor_info.rcMonitor.top, monitor_info.rcMonitor.right - monitor_info.rcMonitor.left, monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
		}
	}
	else {
		// Reset original window style and size.  The multiple window size/moves
		// here are ugly, but if SetWindowPos() doesn't redraw, the taskbar won't be
		// repainted.  Better-looking methods welcome.
		SetWindowLong(hwnd_, GWL_STYLE, saved_window_info_.style);
		SetWindowLong(hwnd_, GWL_EXSTYLE, saved_window_info_.ex_style);

		if (!for_metro) {
			SetWindowPos(hwnd_, NULL, saved_window_info_.window_rect.left, saved_window_info_.window_rect.top,
				saved_window_info_.window_rect.right - saved_window_info_.window_rect.left, saved_window_info_.window_rect.bottom - saved_window_info_.window_rect.top,
				SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
		}
		if (saved_window_info_.maximized)
			::SendMessage(hwnd_, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
	}
	return true;
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

	MessageBox(window_handle, buffer, "Shell Error", MB_OK);
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

	OutputDebugString(buffer);
}

float Shell::GetElapsedTime() 
{
	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);

	return (float)((counter.QuadPart - time_startup.QuadPart) * time_frequency);
}

static LRESULT CALLBACK WindowProcedure(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param)
{
	// See what kind of message we've got.
	switch (message)
	{		
		case WM_ACTIVATE:
		{
			if (LOWORD(w_param) != WA_INACTIVE)
			{
				activated = true;
			}
			else
			{
				activated = false;
			}
		}
		break;

		// When the window closes, request exit
		case WM_CLOSE:
		{
			running = false;
			return 0;
		}
		break;

		case WM_SIZE:
		{
			int width = LOWORD(l_param);;
			int height = HIWORD(l_param);;
			shell_renderer->SetViewport(width, height);
		}
		break;

		default:
		{
			InputWin32::ProcessWindowsEvent(message, w_param, l_param);
		}
		break;
	}

	// All unhandled messages go to DefWindowProc.
	return DefWindowProc(window_handle, message, w_param, l_param);
}
