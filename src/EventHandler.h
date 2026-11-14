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
	/// Whether ProcessEvent() has a branch of its own for `value`. Read by the
	/// `check_menu_wiring` command; keep it beside the branches it lists.
	virtual bool Handles(const Rml::String& value) const = 0;
};
