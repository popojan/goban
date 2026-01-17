#ifndef GOBAN_EVENTHANDLERNEWGAME_H
#define GOBAN_EVENTHANDLERNEWGAME_H

#include "EventHandler.h"
#include "spdlog/spdlog.h"

/**
	@author Peter Curry
 */

class EventHandlerNewGame : public EventHandler
{
public:
	EventHandlerNewGame();
	virtual ~EventHandlerNewGame();

	virtual void ProcessEvent(Rml::Event& event, const Rml::String& value) override;
private:
    int lastKomiSelection = -1;
    int lastBoardSelection = -1;
    int lastHandicapSelection = -1;
};

#endif
