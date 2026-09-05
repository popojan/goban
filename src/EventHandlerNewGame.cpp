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
    auto* gameElement = dynamic_cast<ElementGame*>(doc->GetElementById("game"));
    GobanControl& controller = gameElement->getController();
    const auto& model = gameElement->getModel();

    if (value == "boardsize") {
        std::istringstream ss(event.GetParameter<Rml::String>("value", "19").c_str());
        int boardSize = 19;
        ss >> boardSize;

        auto select = dynamic_cast<Rml::ElementFormControlSelect*>(doc->GetElementById("selBoard"));
        if (!controller.acceptsUiEvents()) {
            if (select) lastBoardSelection = select->GetSelection();
        } else if (static_cast<int>(model.getBoardSize()) == boardSize) {
            if (select) lastBoardSelection = select->GetSelection();
        } else {
            // Changing the board size replaces the game, so it asks first —
            // the answer arrives later, hence the callback rather than a
            // return value.
            controller.requestNewGame(boardSize, [this, select](bool changed) {
                if (!select) return;
                if (changed) {
                    lastBoardSelection = select->GetSelection();
                } else {
                    select->SetSelection(lastBoardSelection);
                }
            });
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
        if (!controller.acceptsUiEvents()) {
            if (select) lastHandicapSelection = select->GetSelection();
        } else if (model.state.handicap == handicap) {
            if (select) lastHandicapSelection = select->GetSelection();
        } else {
            // Handicap restarts the game too, so it takes the same route.
            controller.requestHandicap(handicap, [this, select](bool changed) {
                if (!select) return;
                if (changed) {
                    lastHandicapSelection = select->GetSelection();
                } else {
                    select->SetSelection(lastHandicapSelection);
                }
            });
        }
    }
    else if(value == "engine") {
        if (controller.acceptsUiEvents()) {
            std::istringstream ss(event.GetParameter<Rml::String>("value", "0").c_str());
            int index = 0;
            ss >> index;
            auto& engine = gameElement->getGameThread();
            if(event.GetCurrentElement()->GetId() == "selectBlack") {
                if (static_cast<int>(engine.getActivePlayer(0)) != index)
                    controller.switchPlayer(0, index);
            }
            else if(event.GetCurrentElement()->GetId() == "selectWhite") {
                if (static_cast<int>(engine.getActivePlayer(1)) != index)
                    controller.switchPlayer(1, index);
            }
        }
    }
    else if(value == "evaluation_moves") {
        // Three states, so a select rather than a checkbox: "on demand" and
        // "always" both looked ticked on one item.
        if (!controller.acceptsUiEvents()) return;
        const std::string mode = event.GetParameter<Rml::String>("value", "off").c_str();
        controller.command("toggle_evaluation_moves " + mode);
    }
    else if(value == "game_mode" || value == "prisoners") {
        // A select's chosen value does not reach the command by itself — every
        // one of them needs a branch here to lift it out of the event. Three
        // selects were added without one, and the symptom was quiet: the bare
        // command reports the current setting instead of changing it, so
        // `prisoners` printed "auto" into #lblMessage and the widget snapped
        // back. The scenarios could not see it, because they drive the command
        // rather than the widget.
        if (!controller.acceptsUiEvents()) return;
        const std::string arg = event.GetParameter<Rml::String>("value", "").c_str();
        if (arg.empty()) return;   // no value is the *query* form; never from a menu
        controller.command(std::string(value.c_str()) + " " + arg);
    }
    else if(value == "toggle_evaluation") {
        // Its own branch for the same reason, and it is not merely cosmetic: a
        // bare `toggle_evaluation` *toggles*, so picking "off" while already off
        // would have switched the evaluation on.
        if (!controller.acceptsUiEvents()) return;
        const std::string arg = event.GetParameter<Rml::String>("value", "").c_str();
        if (arg.empty()) return;
        controller.command("toggle_evaluation " + arg);
    }
    else if(value == "shader") {
        // Like its four siblings above and below. This branch was the one that
        // did not ask, so filling the dropdown counted as the user choosing a
        // shader — see populateUIElements(), which now also suppresses events
        // while it fills. Either fix alone would do here; both, because the rule
        // is that a programmatic repopulation is not a choice, and it should
        // hold from both ends.
        if (!controller.acceptsUiEvents()) return;
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
        if (!controller.acceptsUiEvents()) {
            if (select) lastKomiSelection = select->GetSelection();
        } else if (model.state.komi == komi) {
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
