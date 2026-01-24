#pragma once

#include <RmlUi/Core/Types.h>

namespace Rml {
class Event;
}

class EventHandler
{
public:
	virtual ~EventHandler();
	virtual void ProcessEvent(Rml::Event& event, const Rml::String& value) = 0;
};
