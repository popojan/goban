#pragma once

#include "EventHandler.h"

class EventHandlerNewGame : public EventHandler
{
public:
	EventHandlerNewGame();
	~EventHandlerNewGame() override;

	void ProcessEvent(Rml::Event& event, const Rml::String& value) override;
private:
    int lastKomiSelection = -1;
    int lastBoardSelection = -1;
    int lastHandicapSelection = -1;
};
