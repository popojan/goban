#include "EventManager.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/StringUtilities.h>
#include "AppState.h"
#include "EventHandler.h"
#include <map>
#include <spdlog/spdlog.h>

// The event handler for the current screen. This may be NULL if the current screen has no specific functionality.
static EventHandler* event_handler = nullptr;

// The event handlers registered with the manager.
typedef std::map< Rml::String, EventHandler* > EventHandlerMap;
EventHandlerMap event_handlers;
Rml::String rml_data_prefix;

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
void EventManager::RegisterEventHandler(const Rml::String& handler_name, EventHandler* handler)
{
	// Release any handler bound under the same name.
	EventHandlerMap::iterator iterator = event_handlers.find(handler_name);
	if (iterator != event_handlers.end())
		delete (*iterator).second;

	event_handlers[handler_name] = handler;
}

// Processes an event coming through from Rocket.
void EventManager::ProcessEvent(Rml::Event& event, const Rml::String& value)
{
	spdlog::debug("EventManager::ProcessEvent called with value: '{}'", value.c_str());
	spdlog::debug("Event type: {}", event.GetType().c_str());
	
	Rml::StringList commands;
	Rml::StringUtilities::ExpandString(commands, value, ';');
	spdlog::debug("Found {} commands", commands.size());
	
	for (size_t i = 0; i < commands.size(); ++i)
	{
		spdlog::debug("Processing command {}: '{}'", i, commands[i].c_str());
		
		// Check for a generic 'load' or 'exit' command.
		Rml::StringList values;
		Rml::StringUtilities::ExpandString(values, commands[i], ' ');

		if (values.empty())
		{
			spdlog::debug("Command is empty, returning");
			return;
		}

		spdlog::debug("Command has {} values, first value: '{}'", values.size(), values[0].c_str());

		if (values[0] == "load" &&
 			values.size() > 1)
		{
			spdlog::debug("Executing load command with window: '{}'", values[1].c_str());
			// Load the window using context from the event.
			auto* context = event.GetCurrentElement()->GetContext();
			LoadWindow(values[1], context);
		}
		else if (values[0] == "close")
		{
			spdlog::debug("Executing close command");
			Rml::ElementDocument* target_document = NULL;

			if (values.size() > 1)
			{
				spdlog::debug("Closing document by name: '{}'", values[1].c_str());
				// Get context from the event instead of using static variable
				auto* context = event.GetCurrentElement()->GetContext();
				target_document = context->GetDocument(values[1].c_str());
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
			AppState::RequestExit();
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
bool EventManager::LoadWindow(const Rml::String& window_name, Rml::Context* context)
{
	// Set the event handler for the new screen, if one has been registered.
	EventHandler* old_event_handler = event_handler;
	EventHandlerMap::iterator iterator = event_handlers.find(window_name);
	if (iterator != event_handlers.end())
		event_handler = (*iterator).second;
	else
		event_handler = NULL;

	// Attempt to load the referenced RML document.
	Rml::String document_path = rml_data_prefix + Rml::String("/") + window_name + Rml::String(".rml");
	Rml::ElementDocument* document = context->LoadDocument(document_path.c_str());
	if (document == NULL)
	{
		event_handler = old_event_handler;
		return false;
	}

	// Set the element's title on the title; IDd 'title' in the RML.
	Rml::Element* title = document->GetElementById("title");
	if (title != NULL)
		title->SetInnerRML(document->GetTitle());

	document->Show();

	Rml::Element* gameElement = document->GetElementById("game");
	
	gameElement->Focus();

	return true;
}

void EventManager::SetPrefix(const Rml::String& prefix)
{
    rml_data_prefix = prefix;
}

const Rml::String& EventManager::GetPrefix()
{
    return rml_data_prefix;
}

EventHandler* EventManager::GetEventHandler(const Rml::String& handler_name)
{
    EventHandlerMap::iterator iterator = event_handlers.find(handler_name);
    if (iterator != event_handlers.end())
        return (*iterator).second;
    return nullptr;
}