#include "EventInstancer.h"
#include "Event.h"

EventInstancer::EventInstancer() {}

EventInstancer::~EventInstancer() {}

Rml::EventListener* EventInstancer::InstanceEventListener(const Rml::String& value, Rml::Element* element) {
    (void)element;
    return new Event(value);
}
