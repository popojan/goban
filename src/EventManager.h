#pragma once
#include <RmlUi/Core/Context.h>

class EventHandler;

class EventManager
{
public:
	static void Shutdown();

	static void RegisterEventHandler(const Rml::String& handler_name, EventHandler* handler);

	static void ProcessEvent(Rml::Event& event, const Rml::String& value);
	static bool LoadWindow(const Rml::String& window_name, Rml::Context* context);
    static void SetPrefix(const Rml::String& prefix);
	static const Rml::String& GetPrefix();

    static EventHandler* GetEventHandler(const Rml::String& handler_name);
private:
	EventManager();
	~EventManager();
};

