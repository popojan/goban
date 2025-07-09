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

#include "EventManager.h"
#include <Rocket/Core/Context.h>
#include <Rocket/Core/ElementDocument.h>
#include <Shell.h>
#include "EventHandler.h"
#include <map>
#include <spdlog/spdlog.h>

// Remove static context dependency - context will be passed from events

// The event handler for the current screen. This may be NULL if the current screen has no specific functionality.
static EventHandler* event_handler = NULL;

// The event handlers registered with the manager.
typedef std::map< Rocket::Core::String, EventHandler* > EventHandlerMap;
EventHandlerMap event_handlers;
Rocket::Core::String rml_data_prefix;

EventManager::EventManager()
{
}

EventManager::~EventManager()
{
}

// Releases all event handlers registered with the manager.
void EventManager::Shutdown()
{
	for (EventHandlerMap::iterator i = event_handlers.begin(); i != event_handlers.end(); ++i)
		delete (*i).second;

	event_handlers.clear();
	event_handler = NULL;
}

// Registers a new event handler with the manager.
void EventManager::RegisterEventHandler(const Rocket::Core::String& handler_name, EventHandler* handler)
{
	// Release any handler bound under the same name.
	EventHandlerMap::iterator iterator = event_handlers.find(handler_name);
	if (iterator != event_handlers.end())
		delete (*iterator).second;

	event_handlers[handler_name] = handler;
}

// Processes an event coming through from Rocket.
void EventManager::ProcessEvent(Rocket::Core::Event& event, const Rocket::Core::String& value)
{
	spdlog::debug("EventManager::ProcessEvent called with value: '{}'", value.CString());
	spdlog::debug("Event type: {}", event.GetType().CString());
	
	Rocket::Core::StringList commands;
	Rocket::Core::StringUtilities::ExpandString(commands, value, ';');
	spdlog::debug("Found {} commands", commands.size());
	
	for (size_t i = 0; i < commands.size(); ++i)
	{
		spdlog::debug("Processing command {}: '{}'", i, commands[i].CString());
		
		// Check for a generic 'load' or 'exit' command.
		Rocket::Core::StringList values;
		Rocket::Core::StringUtilities::ExpandString(values, commands[i], ' ');

		if (values.empty())
		{
			spdlog::debug("Command is empty, returning");
			return;
		}

		spdlog::debug("Command has {} values, first value: '{}'", values.size(), values[0].CString());

		if (values[0] == "load" &&
 			values.size() > 1)
		{
			spdlog::debug("Executing load command with window: '{}'", values[1].CString());
			// Load the window using context from the event.
			auto* context = event.GetCurrentElement()->GetContext();
			LoadWindow(values[1], context);
		}
		else if (values[0] == "close")
		{
			spdlog::debug("Executing close command");
			Rocket::Core::ElementDocument* target_document = NULL;

			if (values.size() > 1)
			{
				spdlog::debug("Closing document by name: '{}'", values[1].CString());
				// Get context from the event instead of using static variable
				auto* context = event.GetCurrentElement()->GetContext();
				target_document = context->GetDocument(values[1].CString());
			}
			else
			{
				spdlog::debug("Closing current document");
				target_document = event.GetTargetElement()->GetOwnerDocument();
			}

			if (target_document != NULL)
			{
				spdlog::debug("Document found, closing");
				target_document->Close();
			}
			else
			{
				spdlog::debug("No document found to close");
			}
		}
		else if (values[0] == "exit")
		{
			spdlog::debug("Executing exit command");
			Shell::RequestExit();
		}
		else
		{
			spdlog::debug("Passing command to event handler");
			if (event_handler != NULL)
			{
				spdlog::debug("Event handler {} exists, forwarding command");
				event_handler->ProcessEvent(event, commands[i]);
			}
			else
			{
				spdlog::debug("No event handler available");
			}
		}
	}
	spdlog::debug("EventManager::ProcessEvent completed");
}

// Loads a window and binds the event handler for it.
bool EventManager::LoadWindow(const Rocket::Core::String& window_name, Rocket::Core::Context* context)
{
	// Set the event handler for the new screen, if one has been registered.
	EventHandler* old_event_handler = event_handler;
	EventHandlerMap::iterator iterator = event_handlers.find(window_name);
	if (iterator != event_handlers.end())
		event_handler = (*iterator).second;
	else
		event_handler = NULL;

	// Attempt to load the referenced RML document.
	Rocket::Core::String document_path = rml_data_prefix + Rocket::Core::String("/") + window_name + Rocket::Core::String(".rml");
	Rocket::Core::ElementDocument* document = context->LoadDocument(document_path.CString());
	if (document == NULL)
	{
		event_handler = old_event_handler;
		return false;
	}

	// Set the element's title on the title; IDd 'title' in the RML.
	Rocket::Core::Element* title = document->GetElementById("title");
	if (title != NULL)
		title->SetInnerRML(document->GetTitle());

	document->Show();

	// Remove the caller's reference.
	document->RemoveReference();
	
	Rocket::Core::Element* gameElement = document->GetElementById("game");
	
	gameElement->Focus();

	return true;
}

void EventManager::SetPrefix(const Rocket::Core::String& prefix)
{
    rml_data_prefix = prefix;
}

EventHandler* EventManager::GetEventHandler(const Rocket::Core::String& handler_name)
{
    EventHandlerMap::iterator iterator = event_handlers.find(handler_name);
    if (iterator != event_handlers.end())
        return (*iterator).second;
    return nullptr;
}