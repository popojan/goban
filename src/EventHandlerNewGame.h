#ifndef ROCKETINVADERSEVENTHANDLERSTARTGAME_H
#define ROCKETINVADERSEVENTHANDLERSTARTGAME_H

#include "EventHandler.h"

/**
	@author Peter Curry
 */

class EventHandlerNewGame : public EventHandler
{
public:
	EventHandlerNewGame();
	virtual ~EventHandlerNewGame();

	virtual void ProcessEvent(Rocket::Core::Event& event, const Rocket::Core::String& value);
};

#endif
