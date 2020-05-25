#ifndef ROCKETINVADERSEVENTHANDLERSTARTGAME_H
#define ROCKETINVADERSEVENTHANDLERSTARTGAME_H

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

	virtual void ProcessEvent(Rocket::Core::Event& event, const Rocket::Core::String& value);
private:
    int lastKomiSelection = -1;
    int lastBoardSelection = -1;
    int lastHandicapSelection = -1;
};

#endif
