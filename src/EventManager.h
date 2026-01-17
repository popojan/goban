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

#ifndef ROCKETINVADERSEVENTMANAGER_H
#define ROCKETINVADERSEVENTMANAGER_H

#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Context.h>

class EventHandler;

/**
	@author Peter Curry
 */

class EventManager
{
public:
	/// Releases all event handlers registered with the manager.
	static void Shutdown();

	/// Registers a new event handler with the manager.
	/// @param[in] handler_name The name of the handler; this must be the same as the window it is handling events for.
	/// @param[in] handler The event handler.
	static void RegisterEventHandler(const Rml::String& handler_name, EventHandler* handler);

	/// Processes an event coming through from RmlUi.
	/// @param[in] event The RmlUi event that spawned the application event.
	/// @param[in] value The application-specific event value.
	static void ProcessEvent(Rml::Event& event, const Rml::String& value);
	/// Loads a window and binds the event handler for it.
	/// @param[in] window_name The name of the window to load.
	/// @param[in] context The RmlUi context to use for loading the document.
	static bool LoadWindow(const Rml::String& window_name, Rml::Context* context);
    static void SetPrefix(const Rml::String& prefix);
    /// Gets the RML data prefix path.
    static const Rml::String& GetPrefix();

    /// Gets an event handler by name.
    /// @param[in] handler_name The name of the handler to retrieve.
    /// @return Pointer to the event handler, or nullptr if not found.
    static EventHandler* GetEventHandler(const Rml::String& handler_name);
private:
	EventManager();
	~EventManager();
};


#endif
