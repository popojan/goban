#pragma once
#include <RmlUi/Core/EventListenerInstancer.h>

class EventInstancer : public Rml::EventListenerInstancer
{
public:
	EventInstancer();
	~EventInstancer() override;

	/// Instances a new event handle
	Rml::EventListener* InstanceEventListener(
            const Rml::String& value, Rml::Element* element) override;
};
