#include "ElementGame.h"
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core.h>
#include <RmlUi/Core/StringUtilities.h>
#include <GLFW/glfw3.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>

ElementGame::ElementGame(const Rml::String& tag)
        : Rml::Element(tag), model(this), view(model), engine(model),
          control(this, model, view, engine)
{
    engine.loadEngines(config);
    engine.addGameObserver(&model);
    engine.addGameObserver(&view);

    engine.clearGame(19, 0.5, 0);
    model.createNewRecord();
    control.togglePlayer(0, 0);
    control.togglePlayer(1, 0);
}

void ElementGame::populateEngines() {
    auto selectBlack = dynamic_cast<Rml::ElementFormControlSelect*>(
            GetContext()->GetDocument("game_window")->GetElementById("selectBlack"));
    auto selectWhite = dynamic_cast<Rml::ElementFormControlSelect*>(
            GetContext()->GetDocument("game_window")->GetElementById("selectWhite"));
    const auto players(engine.getPlayers());

    if(!selectBlack) {
        spdlog::warn("missing GUI element [selectBlack]");
    }
    if(!selectWhite) {
        spdlog::warn("missing GUI element [selectWhite]");
    }

    if(selectBlack && selectWhite) {
        for (unsigned i = 0; i < players.size(); ++i) {
            std::ostringstream ss;
            ss << i;
            std::string playerName(players[i]->getName());
            std::string playerIndex(ss.str());
            selectBlack->Add(playerName.c_str(), playerIndex.c_str());
            selectWhite->Add(playerName.c_str(), playerIndex.c_str());
        }
        selectBlack->SetSelection((int)players.size() - 1);
        selectWhite->SetSelection(0);
    }

    auto selectShader = dynamic_cast<Rml::ElementFormControlSelect*>(
            GetContext()->GetDocument("game_window")->GetElementById("selectShader"));

    if(!selectShader) {
        spdlog::warn("missing GUI element [selectShader]");
        return;
    }

    using nlohmann::json;
    const auto shaders(config->data.value("shaders", json::array()));

    int i = 0;
    for(json::const_iterator it = shaders.begin(); it != shaders.end(); ++it, ++i){
        std::ostringstream ss;
        ss << i;
        std::string shaderName(it->value("name", ss.str()));
        std::string shaderIndex(ss.str());
        selectShader->Add(shaderName.c_str(),shaderIndex.c_str());
    }
    selectShader->SetSelection(0);
}

void ElementGame::gameLoop() {
    static int cnt = 0;
    static float lastTime = -1;
    auto context = GetContext();

    float currentTime = static_cast<float>(glfwGetTime());
    if (currentTime - lastTime >= 1.0) {
        static int frames = 1;
        auto debugElement = context->GetDocument("game_window")->GetElementById("lblFPS");
        auto fpsTemplate = context->GetDocument("game_window")->GetElementById("templateFPS");
        const Rml::String sFps = Rml::CreateString(fpsTemplate->GetInnerRML().c_str(), (float)cnt / (currentTime - lastTime));
        if (debugElement != nullptr) {
            debugElement->SetInnerRML(sFps.c_str());
            view.requestRepaint();
        }
        spdlog::debug(sFps.c_str());
        //if(frames % 10 == 0)
        frames += 1;
        lastTime = currentTime;
        cnt = 0;
    }
    ElementGame* game = dynamic_cast<ElementGame*>(context->GetDocument("game_window")->GetElementById("game"));
    if (game != nullptr && game->isExiting()) {
        return;
    }
    context->Update();
    if (view.animationRunning || view.MAX_FPS) {
        view.requestRepaint();
    }
    if (view.updateFlag) {
        // Rendering is now managed in the main loop with GLFW
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        context->Render();
        glPopAttrib();
        cnt++;
    }
    if (!view.MAX_FPS){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
ElementGame::~ElementGame() = default;

void ElementGame::ProcessEvent(Rml::Event& event)
{
    spdlog::debug("ElementGame processes event: {}", event.GetType().c_str());
    // RmlUi doesn't have Element::ProcessEvent - event handling is different
    if(event.GetTargetElement() != this && !(event == "mousemove")) {
        view.requestRepaint();
    }

    if (event == "keydown" || event == "keyup") {
        Rml::Input::KeyIdentifier key_identifier = (Rml::Input::KeyIdentifier) event.GetParameter< int >("key_identifier", 0);
        control.keyPress(key_identifier, 0, 0, event == "keydown");
    }
    else if (event == "mousemove") {
        int x = event.GetParameter<int>("mouse_x", -1);
        int y = event.GetParameter<int>("mouse_y", -1);
        control.mouseMove(x, y);
    }
    else if (event == "mousescroll") {
        int x = event.GetParameter<int>("mouse_x", -1);
        int y = event.GetParameter<int>("mouse_y", -1);
        // RmlUi uses wheel_delta_y for vertical scroll
        float deltaY = event.GetParameter<float>("wheel_delta_y", 0.0f);
        control.mouseClick((deltaY < 0 ? 3 : 4), 1, x, y);
    }
    else if (event == "resize") {
        view.requestRepaint();
    }
    if (event == "load")
    {
        //control.Initialise();
        spdlog::debug("Load");
        populateEngines();
    }
}

void ElementGame::OnUpdate()
{
    if(!view.gobanShader.isReady())
        return;

    //view.board.setStoneRadius(2.0f * model.metrics.stoneRadius / model.metrics.squareSizeX);
    view.board.updateMetrics(model.metrics);

    bool isOver = model.state.reason != GameState::NO_REASON;
    bool isRunning = engine.isRunning();

    Rml::Context* context = GetContext();

    std::string gameState(!isOver && isRunning ? "1" : (isOver ? "2" : "4"));
    model.state.cmd = gameState;
    if(model.state.cmd != view.state.cmd) {
        auto cmdStart = context->GetDocument("game_window")->GetElementById("cmdStart");
        auto cmdClear = context->GetDocument("game_window")->GetElementById("cmdClear");
        auto grpMoves = context->GetDocument("game_window")->GetElementById("grpMoves");
        auto grpGame  = context->GetDocument("game_window")->GetElementById("grpGame");
        const Rml::String DISPLAY("display");
        bool gameInProgress = !isOver && isRunning;
        grpMoves->SetProperty(DISPLAY, gameInProgress ? "block" : "none");
        grpGame->SetProperty(DISPLAY, gameInProgress ? "none" : "block");
        if (!gameInProgress) {
            if (!isOver) {
                cmdClear->SetProperty(DISPLAY, "none");
                cmdStart->SetProperty(DISPLAY, "block");
            } else {
                cmdClear->SetProperty(DISPLAY, "block");
                cmdStart->SetProperty(DISPLAY, "none");
            }
        }
        requestRepaint();
        view.state.cmd = gameState;
    }

    if (view.state.colorToMove != model.state.colorToMove) {
        bool blackMove = model.state.colorToMove == Color::BLACK;
        Rml::Element* elBlack = context->GetDocument("game_window")->GetElementById("blackMoves");
        Rml::Element* elWhite = context->GetDocument("game_window")->GetElementById("whiteMoves");
        if (elBlack != nullptr) {
            elBlack->SetClass("active", blackMove);
            view.state.colorToMove = model.state.colorToMove;
            requestRepaint();
        }
        if (elWhite != nullptr) {
            elWhite->SetClass("active", !blackMove);
            view.state.colorToMove = model.state.colorToMove;
            requestRepaint();
        }
    }
    if ((view.state.capturedBlack != model.state.capturedBlack)
        || (view.state.capturedWhite != model.state.capturedWhite) /*stones captured */
        || (view.state.reason != GameState::NO_REASON && model.state.reason == GameState::NO_REASON) /* new game */)
    {
        Rml::Element* elWhiteCnt = context->GetDocument("game_window")->GetElementById("cntWhite");
        Rml::Element* elBlackCnt = context->GetDocument("game_window")->GetElementById("cntBlack");
        if (elWhiteCnt != nullptr) {
            elWhiteCnt->SetInnerRML(Rml::CreateString( "White: %d", model.state.capturedBlack).c_str());
            requestRepaint();
        }
        if (elBlackCnt != nullptr) {
            elBlackCnt->SetInnerRML(Rml::CreateString( "Black: %d", model.state.capturedWhite).c_str());
            requestRepaint();
        }

        // Update prisoner counts in Analysis menu
        Rml::Element* elPrisonersWhite = context->GetDocument("game_window")->GetElementById("lblPrisonersWhite");
        Rml::Element* elPrisonersBlack = context->GetDocument("game_window")->GetElementById("lblPrisonersBlack");
        if (elPrisonersWhite != nullptr) {
            auto templateWhite = context->GetDocument("game_window")->GetElementById("templatePrisonersWhite");
            if (templateWhite != nullptr) {
                elPrisonersWhite->SetInnerRML(
                    Rml::CreateString( templateWhite->GetInnerRML().c_str(), model.state.capturedBlack).c_str()
                );
            }
        }
        if (elPrisonersBlack != nullptr) {
            auto templateBlack = context->GetDocument("game_window")->GetElementById("templatePrisonersBlack");
            if (templateBlack != nullptr) {
                elPrisonersBlack->SetInnerRML(
                    Rml::CreateString( templateBlack->GetInnerRML().c_str(), model.state.capturedWhite).c_str()
                );
            }
        }

        view.state.capturedBlack = model.state.capturedBlack;
        view.state.capturedWhite = model.state.capturedWhite;
        view.state.reason = model.state.reason;
    }
    if(view.state.reservoirBlack != model.state.reservoirBlack
        || view.state.reservoirWhite != model.state.reservoirWhite) {
        view.state.reservoirBlack = model.state.reservoirBlack;
        view.state.reservoirWhite = model.state.reservoirWhite;
        requestRepaint();
    }
    if (view.state.handicap != model.state.handicap) {
        Rml::Element* hand = context->GetDocument("game_window")->GetElementById("lblHandicap");
        if (hand != nullptr) {
            hand->SetInnerRML(Rml::CreateString( "Handicap: %d", model.state.handicap).c_str());
            requestRepaint();
            view.state.handicap = model.state.handicap;
        }
    }
    if (view.state.komi != model.state.komi) {
        Rml::Element* elKomi = context->GetDocument("game_window")->GetElementById("lblKomi");
        if (elKomi != nullptr) {
            elKomi->SetInnerRML(Rml::CreateString( "Komi: %.1f", model.state.komi).c_str());
            view.state.komi = model.state.komi;
            requestRepaint();
        }
    }
    Rml::Element* msg = context->GetDocument("game_window")->GetElementById("lblMessage");
    if (view.state.msg != model.state.msg) {
        switch (model.state.msg) {
        case GameState::CALCULATING_SCORE:
            msg->SetInnerRML(
                context->GetDocument("game_window")
                    ->GetElementById("templateCalculatingScore")
                    ->GetInnerRML()
            );
            break;
        case GameState::BLACK_RESIGNS:
            msg->SetInnerRML(
                context->GetDocument("game_window")
                    ->GetElementById("templateBlackResigns")
                    ->GetInnerRML()
            );
            break;
        case GameState::WHITE_RESIGNS:
            msg->SetInnerRML(
                context->GetDocument("game_window")
                    ->GetElementById("templateWhiteReigns")
                    ->GetInnerRML()
            );
            break;
        case GameState::BLACK_RESIGNED:
            msg->SetInnerRML(
                      context->GetDocument("game_window")
                        ->GetElementById("templateResignWhiteWon")
                        ->GetInnerRML()
            );
            break;
        case GameState::WHITE_RESIGNED:
            msg->SetInnerRML(
                      context->GetDocument("game_window")
                        ->GetElementById("templateResignBlackWon")
                        ->GetInnerRML());
            break;
        case GameState::BLACK_PASS:
            msg->SetInnerRML(
                context->GetDocument("game_window")
                    ->GetElementById("templateBlackPasses")
                    ->GetInnerRML()
            );
            break;
        case GameState::WHITE_PASS:
            msg->SetInnerRML(
                context->GetDocument("game_window")
                    ->GetElementById("templateWhitePasses")
                    ->GetInnerRML()
            );
            break;
        case GameState::BLACK_WON:
        case GameState::WHITE_WON: {
            Rml::Element *elWhiteCnt = context->GetDocument("game_window")->GetElementById("cntWhite");
            Rml::Element *elBlackCnt = context->GetDocument("game_window")->GetElementById("cntBlack");
            // Show simplified captured stone counts (no detailed scoring breakdown)
            elWhiteCnt->SetInnerRML(
                    Rml::CreateString( "White captured: %d", model.state.capturedWhite).c_str());
            elBlackCnt->SetInnerRML(
                    Rml::CreateString( "Black captured: %d", model.state.capturedBlack).c_str());
            if (model.state.winner == Color::WHITE)
                msg->SetInnerRML(
                    Rml::CreateString(
                        context->GetDocument("game_window")
                        ->GetElementById("templateWhiteWon")
                        ->GetInnerRML().c_str(),
                    std::abs(model.state.scoreDelta)).c_str()
                );
            else
                msg->SetInnerRML(
                    Rml::CreateString(
                        context->GetDocument("game_window")
                        ->GetElementById("templateBlackWon")
                        ->GetInnerRML().c_str(),
                    std::abs(model.state.scoreDelta)).c_str()
                );
            view.state.reason = model.state.reason;
        }
            break;
        default:
            msg->SetInnerRML("");
        }
        requestRepaint();
        view.state.msg = model.state.msg;
    }
    if(view.state.msg == GameState::NONE) {
        //compose move comment
        for(auto &p: engine.getPlayers()) {
            if(p->isTypeOf(Player::ENGINE)) {
                std::string newMsg(dynamic_cast<GtpEngine*>(p)->lastError());
                if(!newMsg.empty() && newMsg != model.state.err) {
                    model.state.err = newMsg;
                }
            }
        }
        if(view.state.err != model.state.err) {
            msg->SetInnerRML(model.state.err.c_str());
            view.state.err = model.state.err;
            requestRepaint();
        }
    }
    view.Update();
}

void ElementGame::Reshape() {
    Rml::Context* context = GetContext();
    Rml::Vector2i d = context->GetDimensions();
    if (WINDOW_HEIGHT != d.y || WINDOW_WIDTH != d.x) {
        // Note: RmlUi's StyleSheet API has changed significantly.
        // Dynamic font-size adjustment is disabled for now.
        // The font size could be adjusted via CSS calc() or data bindings in RmlUi.
        WINDOW_WIDTH = d.x;
        WINDOW_HEIGHT = d.y;
        spdlog::debug("ElementGame::Reshape - context: {}x{}", d.x, d.y);
    }
}

void ElementGame::OnMenuToggle(const std::string &cmd, bool checked) {
    if(cmd.substr(0, 7) == "toggle_") {
        std::vector<Rml::Element*> elements;
        GetContext()->GetDocument("game_window")->GetElementsByClassName(elements, cmd.c_str());
        for(auto el: elements) {
            el->SetClass("selected", checked);
            el->SetClass("unselected", !checked);
        }
    }

}

void ElementGame::OnRender()
{
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    Reshape();
    view.Render(WINDOW_WIDTH, WINDOW_HEIGHT);
    glPopAttrib();
}

void ElementGame::OnChildAdd(Rml::Element* element)
{
    Rml::Element::OnChildAdd(element);

    if (element == this) {
        GetOwnerDocument()->AddEventListener("load", this);
        GetOwnerDocument()->AddEventListener("mousemove", this);
        GetOwnerDocument()->AddEventListener("mousescroll", this);
        GetOwnerDocument()->AddEventListener("keydown", this);
        GetOwnerDocument()->AddEventListener("keyup", this);
    }
}
