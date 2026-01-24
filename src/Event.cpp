#include "Event.h"
#include "EventManager.h"
#include <spdlog/spdlog.h>

#include "EventHandler.h"
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

Event::Event(const Rml::String& value) : value(value)
{
}

Event::~Event()
{
}

void Event::ProcessEvent(Rml::Event& event)
{
	spdlog::debug("Event::ProcessEvent called with value: '{}' type: '{}'", value.c_str(), event.GetType().c_str());

	// Get the document that owns this event
	auto* document = event.GetTargetElement()->GetOwnerDocument();
	if (!document) {
		spdlog::debug("Event has no owner document, falling back to EventManager");
		EventManager::ProcessEvent(event, value);
		return;
	}

	// Determine which handler to use based on the document ID
	Rml::String documentId = document->GetId();
	spdlog::debug("Event from document: '{}' value: '{}'", documentId.c_str(), value.c_str());

	EventHandler* handler = nullptr;
	if (documentId == "open_dialog") {
		// File chooser dialog events
		handler = EventManager::GetEventHandler("open");
	} else {
		// Main game window events (or fallback)
		handler = EventManager::GetEventHandler("goban");
	}

	if (handler) {
		spdlog::debug("Routing event to specific handler for document '{}'", documentId.c_str());
		handler->ProcessEvent(event, value);
	} else {
		spdlog::debug("No specific handler found, using EventManager fallback");
		EventManager::ProcessEvent(event, value);
	}
}

// Destroys the event.
void Event::OnDetach(Rml::Element* /*element*/)
{
	delete this;
}
