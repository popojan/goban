#include "ElementGame.h"
#include "EventHandlerNewGame.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include "AppState.h"
#include "Configuration.h"

extern std::shared_ptr<Configuration> config;

EventHandlerNewGame::EventHandlerNewGame()
{
}

EventHandlerNewGame::~EventHandlerNewGame()
{
}

void EventHandlerNewGame::ProcessEvent(Rml::Event& event, const Rml::String& value)
{
    spdlog::debug("EventHandlerNewGame recieved event");
    auto doc = event.GetCurrentElement()->GetContext()->GetDocument("game_window");
    if(!doc) return;
    GobanControl& controller = dynamic_cast<ElementGame*>(doc->GetElementById("game"))->getController();

    if (value == "boardsize") {
        std::istringstream ss(event.GetParameter<Rml::String>("value", "19").c_str());
        int boardSize = 19;
        ss >> boardSize;

        auto select = dynamic_cast<Rml::ElementFormControlSelect*>(doc->GetElementById("selBoard"));
        if (controller.isSyncingUI()) {
            // Just syncing UI to match state, don't trigger action
            if (select) lastBoardSelection = select->GetSelection();
        } else if(!controller.newGame(boardSize)) {
            spdlog::error("setting boardsize failed");
            select->SetSelection(lastBoardSelection);
        } else {
            lastBoardSelection = select->GetSelection();
        }
    }
    else if(value == "mdown" || value == "mup") {
        // Only forward mouse events to board controller if they're not from menu elements
        Rml::Element* target = event.GetTargetElement();
        if (target && target->GetId() == "game") {
            // Event originated from the game element itself (board area), not a menu item
            int state = value == "mdown" ? 1 : 0;
            int button = event.GetParameter< int >("button", -1);
            int x = event.GetParameter<int>("mouse_x", -1);
            int y = event.GetParameter<int>("mouse_y", -1);
            controller.mouseClick(button, state, x, y);
        }
        // If event came from a menu element, don't forward to board controller
    }
    else if (value == "handicap") {
        std::istringstream ss(event.GetParameter<Rml::String>("value", "0").c_str());
        int handicap = 0;
        ss >> handicap;

        auto select = dynamic_cast<Rml::ElementFormControlSelect*>(doc->GetElementById("selectHandicap"));
        if (controller.isSyncingUI()) {
            if (select) lastHandicapSelection = select->GetSelection();
        } else if(!controller.setHandicap(handicap)) {
            spdlog::error("setting handicap failed");
            select->SetSelection(lastHandicapSelection);
        } else {
            lastHandicapSelection = select->GetSelection();
        }
    }
    else if(value == "engine") {
        if (!controller.isSyncingUI()) {
            std::istringstream ss(event.GetParameter<Rml::String>("value", "0").c_str());
            int index = 0;
            ss >> index;
            if(event.GetCurrentElement()->GetId() == "selectBlack") {
                controller.switchPlayer(0, index);
            }
            else if(event.GetCurrentElement()->GetId() == "selectWhite") {
                controller.switchPlayer(1, index);
            }
        }
    }
    else if(value == "shader") {
        std::istringstream ss(event.GetParameter<Rml::String>("value", "0").c_str());
        int index = 0;
        ss >> index;
        spdlog::info("switching shader to #{}", index);
        controller.switchShader(index);
    }
    else if (value == "komi") {
        std::istringstream ss(event.GetParameter<Rml::String>("value", "0.5").c_str());
        float komi = 0.5;
        ss >> komi;

        auto select = dynamic_cast<Rml::ElementFormControlSelect*>(doc->GetElementById("selectKomi"));
        if (controller.isSyncingUI()) {
            if (select) lastKomiSelection = select->GetSelection();
        } else if(!controller.setKomi(komi)) {
            spdlog::error("setting komi failed");
            select->SetSelection(lastKomiSelection);
        } else {
            lastKomiSelection = select->GetSelection();
        }
    }
    else if (value == "language") {
        // Get selected language from select element
        std::string lang = event.GetParameter<Rml::String>("value", "en").c_str();
        std::string configFile = "./config/" + lang + ".json";

        // Check if this is already the current config (avoid restart loop)
        std::string currentGui = config->data.value("gui", "");
        std::string expectedGui = "./config/gui/" + lang;
        if (currentGui == expectedGui) {
            spdlog::debug("Language {} already active, skipping restart", lang);
            return;
        }

        spdlog::info("Switching to language: {} (config: {})", lang, configFile);
        controller.saveCurrentGame();
        RequestRestart(configFile);
    }
    else {
        controller.command(value.c_str());
    }
    event.StopPropagation();
}
