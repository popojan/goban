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

// Sends the event value through to Invader's event processing system.
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
