#include "ElementGame.h"
#include <Rocket/Core/ElementDocument.h>
#include <Rocket/Core/Factory.h>
#include <Rocket/Core/Input.h>
#include <Rocket/Core.h>
#include <Shell.h>
#include <Rocket/Controls/ElementFormControlSelect.h>

ElementGame::ElementGame(const Rocket::Core::String& tag)
        : Rocket::Core::Element(tag), model(this), view(model), engine(model),
          control(this, model, view, engine)
{
    engine.loadEngines(config);
    engine.addGameObserver(&model);
    engine.addGameObserver(&view);

    engine.clearGame(19, 0.5, 0);
    control.togglePlayer(0, 0);
    control.togglePlayer(1, 0);
}

void ElementGame::populateEngines() {
    auto selectBlack = dynamic_cast<Rocket::Controls::ElementFormControlSelect*>(
            GetContext()->GetDocument("game_window")->GetElementById("selectBlack"));
    auto selectWhite = dynamic_cast<Rocket::Controls::ElementFormControlSelect*>(
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

    auto selectShader = dynamic_cast<Rocket::Controls::ElementFormControlSelect*>(
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

    float currentTime = Shell::GetElapsedTime();
    if (currentTime - lastTime >= 1.0) {
        static int frames = 1;
        auto debugElement = context->GetDocument("game_window")->GetElementById("lblFPS");
        auto fpsTemplate = context->GetDocument("game_window")->GetElementById("templateFPS");
        const Rocket::Core::String sFps(128, fpsTemplate->GetInnerRML().CString(), (float)cnt / (currentTime - lastTime));
        if (debugElement != nullptr) {
            debugElement->SetInnerRML(sFps.CString());
            view.requestRepaint();
        }
        spdlog::debug(sFps.CString());
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
        auto shell_renderer = dynamic_cast<ShellRenderInterfaceOpenGL*>(Rocket::Core::GetRenderInterface());
        if (shell_renderer != nullptr) {
            shell_renderer->PrepareRenderBuffer();
            glPushAttrib(GL_ALL_ATTRIB_BITS);
            context->Render();
            glPopAttrib();
            shell_renderer->PresentRenderBuffer();
            cnt++;
        }
    }
    if (!view.MAX_FPS){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
ElementGame::~ElementGame() = default;

void ElementGame::ProcessEvent(Rocket::Core::Event& event)
{
    spdlog::debug("ElementGame processes event");
    Rocket::Core::Element::ProcessEvent(event);
    if(event.GetTargetElement() != this && !(event == "mousemove")) {
        view.requestRepaint();
    }

    if (event == "keydown" || event == "keyup") {
        Rocket::Core::Input::KeyIdentifier key_identifier = (Rocket::Core::Input::KeyIdentifier) event.GetParameter< int >("key_identifier", 0);
        control.keyPress(key_identifier, 0, 0, event == "keydown");
    }
    else if (event == "mousemove") {
        int x = event.GetParameter<int>("mouse_x", -1);
        int y = event.GetParameter<int>("mouse_y", -1);
        control.mouseMove(x, y);
    }
    else if (event == "mousescroll") {
        //int button = event.GetParameter< int >("button", -1);
        int x = event.GetParameter<int>("mouse_x", -1);
        int y = event.GetParameter<int>("mouse_y", -1);
        int delta = event.GetParameter<int>("wheel_delta", -1);
        control.mouseClick((delta < 0 ? 3 : 4), 1, x, y);
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

    Rocket::Core::Context* context = GetContext();

    std::string gameState(!isOver && isRunning ? "1" : (isOver ? "2" : "4"));
    model.state.cmd = gameState;
    if(model.state.cmd != view.state.cmd) {
        auto cmdStart = context->GetDocument("game_window")->GetElementById("cmdStart");
        auto cmdClear = context->GetDocument("game_window")->GetElementById("cmdClear");
        auto grpMoves = context->GetDocument("game_window")->GetElementById("grpMoves");
        auto grpGame  = context->GetDocument("game_window")->GetElementById("grpGame");
        const Rocket::Core::String DISPLAY("display");
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
        Rocket::Core::Element* elBlack = context->GetDocument("game_window")->GetElementById("blackMoves");
        Rocket::Core::Element* elWhite = context->GetDocument("game_window")->GetElementById("whiteMoves");
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
        Rocket::Core::Element* elWhiteCnt = context->GetDocument("game_window")->GetElementById("cntWhite");
        Rocket::Core::Element* elBlackCnt = context->GetDocument("game_window")->GetElementById("cntBlack");
        if (elWhiteCnt != nullptr) {
            elWhiteCnt->SetInnerRML(Rocket::Core::String(128, "White: %d", model.state.capturedBlack).CString());
            requestRepaint();
        }
        if (elBlackCnt != nullptr) {
            elBlackCnt->SetInnerRML(Rocket::Core::String(128, "Black: %d", model.state.capturedWhite).CString());
            requestRepaint();
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
        Rocket::Core::Element* hand = context->GetDocument("game_window")->GetElementById("lblHandicap");
        if (hand != nullptr) {
            hand->SetInnerRML(Rocket::Core::String(128, "Handicap: %d", model.state.handicap).CString());
            requestRepaint();
            view.state.handicap = model.state.handicap;
        }
    }
    if (view.state.komi != model.state.komi) {
        Rocket::Core::Element* elKomi = context->GetDocument("game_window")->GetElementById("lblKomi");
        if (elKomi != nullptr) {
            elKomi->SetInnerRML(Rocket::Core::String(128, "Komi: %.1f", model.state.komi).CString());
            view.state.komi = model.state.komi;
            requestRepaint();
        }
    }
    Rocket::Core::Element* msg = context->GetDocument("game_window")->GetElementById("lblMessage");
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
            Rocket::Core::Element *elWhiteCnt = context->GetDocument("game_window")->GetElementById("cntWhite");
            Rocket::Core::Element *elBlackCnt = context->GetDocument("game_window")->GetElementById("cntBlack");
            elWhiteCnt->SetInnerRML(
                    Rocket::Core::String(128, "White: %d + %d + %d", model.state.adata.black_captured,
                                         model.state.adata.black_prisoners,
                                         model.state.adata.white_territory).CString());
            elBlackCnt->SetInnerRML(
                    Rocket::Core::String(128, "Black: %d + %d + %d", model.state.adata.white_captured,
                                         model.state.adata.white_prisoners,
                                         model.state.adata.black_territory).CString());
            if (model.state.msg == GameState::WHITE_WON)
                msg->SetInnerRML(
                    Rocket::Core::String(128,
                        context->GetDocument("game_window")
                        ->GetElementById("templateWhiteWon")
                        ->GetInnerRML().CString(),
                    -model.state.adata.delta).CString()
                );
            else
                msg->SetInnerRML(
                    Rocket::Core::String(128,
                        context->GetDocument("game_window")
                        ->GetElementById("templateBlackWon")
                        ->GetInnerRML().CString(),
                    model.state.adata.delta).CString()
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
    Rocket::Core::Context* context = GetContext();
    Rocket::Core::Vector2i d = context->GetDimensions();
    if (WINDOW_HEIGHT != d.y || WINDOW_WIDTH != d.x) {
        Rocket::Core::StyleSheet* style = context->GetDocument("game_window")->GetStyleSheet();
        Rocket::Core::StyleSheet* newStyle = Rocket::Core::Factory::InstanceStyleSheetString(
                Rocket::Core::String(128, "body{ font-size:%.1fpt; }", 25.0*d.y/1050).CString());
        auto combined = style->CombineStyleSheet(newStyle);
        context->GetDocument("game_window")->SetStyleSheet(combined);
        newStyle->RemoveReference();
        combined->RemoveReference();
        WINDOW_WIDTH = d.x;
        WINDOW_HEIGHT = d.y;
    }

}

void ElementGame::OnMenuToggle(const std::string &cmd, bool checked) {
    if(cmd.substr(0, 7) == "toggle_") {
        std::vector<Rocket::Core::Element*> elements;
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

void ElementGame::OnChildAdd(Rocket::Core::Element* element)
{
    Rocket::Core::Element::OnChildAdd(element);

    if (element == this)
        GetOwnerDocument()->AddEventListener("load", this);
}
