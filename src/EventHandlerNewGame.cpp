#include "ElementGame.h"
#include "EventHandlerNewGame.h"
#include <Rocket/Core/Context.h>
#include <Rocket/Controls/ElementFormControlSelect.h>
#include <Rocket/Controls/SelectOption.h>
#include <Rocket/Core/ElementDocument.h>
#include <Rocket/Core/ElementUtilities.h>
#include <Rocket/Core/Event.h>
#include "EventManager.h"
#include <iostream>

EventHandlerNewGame::EventHandlerNewGame()
{
}

EventHandlerNewGame::~EventHandlerNewGame()
{
}

void EventHandlerNewGame::ProcessEvent(Rocket::Core::Event& event, const Rocket::Core::String& value)
{

  auto doc = event.GetCurrentElement()->GetContext()->GetDocument("game_window");
  if(!doc) return;
  GobanControl& controller = dynamic_cast<ElementGame*>(doc->GetElementById("game"))->getController();

    if (value == "boardsize") {
        std::istringstream ss(event.GetParameter<Rocket::Core::String>("value", "19").CString());
        float boardSize = 19;
        ss >> boardSize;

        auto select = dynamic_cast<Rocket::Controls::ElementFormControlSelect*>(doc->GetElementById("selBoard"));
        if(!controller.newGame(boardSize)) {
            spdlog::error("setting boardsize failed");
            select->SetSelection(lastBoardSelection);
        } else {
            lastBoardSelection = select->GetSelection();
        }
    }
    else if(value == "mdown" || value == "mup") {
        int state = value == "mdown" ? 1 : 0;
        int button = event.GetParameter< int >("button", -1);
        int x = event.GetParameter<int>("mouse_x", -1);
        int y = event.GetParameter<int>("mouse_y", -1);
        controller.mouseClick(button, state, x, y);
    }
      else if (value == "handicap") {
        std::istringstream ss(event.GetParameter<Rocket::Core::String>("value", "0").CString());
        float handicap = 0;
        ss >> handicap;

        auto select = dynamic_cast<Rocket::Controls::ElementFormControlSelect*>(doc->GetElementById("selectHandicap"));
        if(!controller.setHandicap(handicap)) {
            spdlog::error("setting handicap failed");
            select->SetSelection(lastHandicapSelection);
        } else {
            lastHandicapSelection = select->GetSelection();
        }
    }
    else if(value == "engine") {
        std::istringstream ss(event.GetParameter<Rocket::Core::String>("value", "0").CString());
        int index = 0;
        ss >> index;
        if(event.GetCurrentElement()->GetId() == "selectBlack") {
            controller.switchPlayer(0, index);
        }
        else if(event.GetCurrentElement()->GetId() == "selectWhite") {
            controller.switchPlayer(1, index);
        }
    }
    else if(value == "shader") {
        std::istringstream ss(event.GetParameter<Rocket::Core::String>("value", "0").CString());
        int index = 0;
        ss >> index;
        spdlog::info("switching shader to #{}", index);
        controller.switchShader(index);
    }
    else if (value == "komi") {
      std::istringstream ss(event.GetParameter<Rocket::Core::String>("value", "0.5").CString());
      float komi = 0.5;
      ss >> komi;

      auto select = dynamic_cast<Rocket::Controls::ElementFormControlSelect*>(doc->GetElementById("selectKomi"));
      if(!controller.setKomi(komi)) {
          spdlog::error("setting komi failed");
          select->SetSelection(lastKomiSelection);
      } else {
          lastKomiSelection = select->GetSelection();
      }
    }
    else {
        controller.command(value.CString());
    }
    event.StopPropagation();
}
