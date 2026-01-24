#pragma once

#include <RmlUi/Core/EventListener.h>

class Event : public Rml::EventListener
{
public:
	explicit Event(const Rml::String& value);
	~Event() override;

	/// Sends the event value
	virtual void ProcessEvent(Rml::Event& event) override;

	/// Destroys the event.
	virtual void OnDetach(Rml::Element* element) override;

private:
	Rml::String value;
};
