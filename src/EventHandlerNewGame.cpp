#include "EventHandlerNewGame.h"
#include <Rocket/Core/Context.h>
#include <Rocket/Controls/ElementFormControlSelect.h>
#include <Rocket/Controls/SelectOption.h>
#include <Rocket/Core/ElementDocument.h>
#include <Rocket/Core/ElementUtilities.h>
#include <Rocket/Core/Event.h>
#include "EventManager.h"
#include <iostream>
#include "ElementGame.h"

EventHandlerNewGame::EventHandlerNewGame()
{
    console = spdlog::get("console");
}

EventHandlerNewGame::~EventHandlerNewGame()
{
}

void EventHandlerNewGame::ProcessEvent(Rocket::Core::Event& event, const Rocket::Core::String& value)
{

  auto doc = event.GetCurrentElement()->GetContext()->GetDocument("game_window");
  GobanControl& controller = dynamic_cast<ElementGame*>(doc->GetElementById("game"))->getController();

    if(value == "new 9") {
        controller.newGame(9);
    }
    else if(value == "new 13") {
        controller.newGame(13);
    }
    else if(value == "new 19") {
        controller.newGame(19);
    }
    else if(value == "mdown" || value == "mup") {
        int state = value == "mdown" ? 1 : 0;
        int button = event.GetParameter< int >("button", -1);
        int x = event.GetParameter<int>("mouse_x", -1);
        int y = event.GetParameter<int>("mouse_y", -1);
        controller.mouseClick(button, state, x, y);
    }
    else if (value == "fullscreen") {
        controller.keyPress(Rocket::Core::Input::KI_F, -1, -1);
    }
    else if (value == "aa") {
        controller.keyPress(Rocket::Core::Input::KI_A, -1, -1);
    }
    else if (value == "fps") {
        controller.keyPress(Rocket::Core::Input::KI_X, -1, -1);
    }
    else if (value == "pass") {
        controller.keyPress(Rocket::Core::Input::KI_P, -1, -1);
    }
    else if (value == "territory") {
        controller.keyPress(Rocket::Core::Input::KI_T, -1, -1);
    }
    else if (value == "black") {
        //controller.switchPlayer(0);
    }
    else if (value == "white") {
        //controller.switchPlayer(1);
    }
    else if (value == "hand") {
        controller.increaseHandicap();
    }
    else if(value == "engine") {
        std::istringstream ss(event.GetParameter<Rocket::Core::String>("value", "0.5").CString());
        int index = 0;
        ss >> index;
        console->warn("engine {}", index);
        if(event.GetCurrentElement()->GetId() == "selectBlack") {
            controller.switchPlayer(0, index);
        }
        else if(event.GetCurrentElement()->GetId() == "selectWhite") {
            controller.switchPlayer(1, index);
        }
    }
    else if (value == "komi") {
      std::istringstream ss(event.GetParameter<Rocket::Core::String>("value", "0.5").CString());
      float komi = 0.5;
      ss >> komi;

      auto select = dynamic_cast<Rocket::Controls::ElementFormControlSelect*>(doc->GetElementById("selectKomi"));
      if(!controller.setKomi(komi)) {
          console->error("setting komi failed");
          select->SetSelection(lastKomiSelection);
      } else {
          lastKomiSelection = select->GetSelection();
      }
    }
    event.StopPropagation();
}
