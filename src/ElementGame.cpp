#include "ElementGame.h"
#include <Rocket/Core/ElementDocument.h>
#include <Rocket/Core/Factory.h>
#include <Rocket/Core/Input.h>
#include <Rocket/Core.h>
#include <Shell.h>
#include <Rocket/Controls/ElementFormControlSelect.h>

ElementGame::ElementGame(const Rocket::Core::String& tag)
        : Rocket::Core::Element(tag), model(this), view(model), engine(model),
          control(this, model, view, engine), hasResults(false), calculatingScore(false)
{
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
        selectBlack->SetSelection(players.size() - 1);
        selectWhite->SetSelection(0);
    }

    auto selectShader = dynamic_cast<Rocket::Controls::ElementFormControlSelect*>(
            GetContext()->GetDocument("game_window")->GetElementById("selectShader"));

    if(!selectShader) {
        spdlog::warn("missing GUI element [selectShader]");
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
        const Rocket::Core::String sfps(128, "FPS: %.1f", cnt / (currentTime - lastTime));
        if (debugElement != 0) {
            debugElement->SetInnerRML(sfps.CString());
            view.requestRepaint();
        }
        spdlog::debug(sfps.CString());
        //if(frames % 10 == 0)
        frames += 1;
		lastTime = currentTime;
		cnt = 0;
	}
	ElementGame* game = dynamic_cast<ElementGame*>(context->GetDocument("game_window")->GetElementById("game"));
	if (game != 0 && game->isExiting()) {
        return;
    }
	context->Update();
	if (view.animationRunning || view.MAX_FPS) {
		view.requestRepaint();
	}
	if (view.updateFlag) {
		auto shell_renderer = static_cast<ShellRenderInterfaceOpenGL*>(Rocket::Core::GetRenderInterface());
		if (shell_renderer != 0) {
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
ElementGame::~ElementGame() {

}

void ElementGame::ProcessEvent(Rocket::Core::Event& event)
{
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

    //model.update();
    view.board.setStoneRadius(2.0f * model.metrics.stoneRadius / model.metrics.squareSize);
    //view.board.updateStones(model.board, model.territory, view.showTerritory);
    //nstate = state;
    bool isOver = model.state.reason != GameState::NOREASON;
    bool isRunning = engine.isRunning();

    Rocket::Core::Context* context = GetContext();

	if (view.state.black != model.state.black) {
        //context->GetDocument("game_window")->GetElementById("lblBlack")
        //    ->SetInnerRML(model.state.black.c_str());
    }
    if (view.state.white != model.state.white) {
        //context->GetDocument("game_window")->GetElementById("lblWhite")
        //    ->SetInnerRML(model.state.white.c_str());
    }


	std::string cmd(isOver ? "New" : !isRunning ? "Play" : "Pass");
	model.state.cmd = cmd;
	if (view.state.cmd != model.state.cmd) {
		Rocket::Core::Element* cmdPass = context->GetDocument("game_window")->GetElementById("cmdPass");
		if (cmdPass != 0) {
			cmdPass->SetInnerRML(cmd.c_str());
			requestRepaint();
			view.state.cmd = cmd;
		}
	}
	//Color colorToMove = engine.getCurrentColor();
	//model.state.colorToMove = colorToMove;
	if (view.state.colorToMove != model.state.colorToMove) {
		bool blackMove = model.state.colorToMove == Color::BLACK;
		Rocket::Core::Element* elBlack = context->GetDocument("game_window")->GetElementById("blackMoves");
		Rocket::Core::Element* elWhite = context->GetDocument("game_window")->GetElementById("whiteMoves");
		if (elBlack != NULL) {
			elBlack->SetClass("active", blackMove);
			view.state.colorToMove = model.state.colorToMove;
			requestRepaint();
		}
		if (elWhite != NULL) {
			elWhite->SetClass("active", !blackMove);
			view.state.colorToMove = model.state.colorToMove;
			requestRepaint();
		}
	}
	if ((view.state.capturedBlack != model.state.capturedBlack)
	    || (view.state.capturedWhite != model.state.capturedWhite) /*stones captured */
		|| (view.state.reason != GameState::NOREASON && model.state.reason == GameState::NOREASON) /* new game */)
	{
		Rocket::Core::Element* elWhiteCnt = context->GetDocument("game_window")->GetElementById("cntWhite");
		Rocket::Core::Element* elBlackCnt = context->GetDocument("game_window")->GetElementById("cntBlack");
		if (elWhiteCnt != NULL) {
			elWhiteCnt->SetInnerRML(Rocket::Core::String(128, "White: %d", model.state.capturedBlack).CString());
			requestRepaint();
		}
		if (elBlackCnt != NULL) {
			elBlackCnt->SetInnerRML(Rocket::Core::String(128, "Black: %d", model.state.capturedWhite).CString());
			requestRepaint();
		}
		view.state.capturedBlack = model.state.capturedBlack;
		view.state.capturedWhite = model.state.capturedWhite;
        view.state.reason = model.state.reason;
	}
	if (view.state.handicap != model.state.handicap) {
		Rocket::Core::Element* hand = context->GetDocument("game_window")->GetElementById("lblHandicap");
		if (hand != NULL) {
			hand->SetInnerRML(Rocket::Core::String(128, "Handicap: %d", model.state.handicap).CString());
			requestRepaint();
			view.state.handicap = model.state.handicap;
		}
	}
	if (view.state.komi != model.state.komi) {
		Rocket::Core::Element* elKomi = context->GetDocument("game_window")->GetElementById("lblKomi");
		if (elKomi != NULL) {
			elKomi->SetInnerRML(Rocket::Core::String(128, "Komi: %.1f", model.state.komi).CString());
			view.state.komi = model.state.komi;
			requestRepaint();
		}
	}
    Rocket::Core::Element* msg = context->GetDocument("game_window")->GetElementById("lblMessage");
	if (view.state.msg != model.state.msg) {
		switch (model.state.msg) {
		case GameState::CALCULATING_SCORE:
			msg->SetInnerRML("Calculating score...");
			break;
		case GameState::BLACK_RESIGNS:
			msg->SetInnerRML("Black resigns");
			break;
		case GameState::WHITE_RESIGNS:
			msg->SetInnerRML("White resigns");
			break;
		case GameState::BLACK_RESIGNED:
			msg->SetInnerRML("White wins by resignation.");
			break;
		case GameState::WHITE_RESIGNED:
			msg->SetInnerRML("Black wins by resignation.");
			break;
		case GameState::BLACK_PASS:
			msg->SetInnerRML("Black passes");
			break;
		case GameState::WHITE_PASS:
			msg->SetInnerRML("White passes");
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
                        Rocket::Core::String(128, "White wins by %.1f", model.state.adata.delta).CString());
            else
                msg->SetInnerRML(
                        Rocket::Core::String(128, "Black wins by %.1f", -model.state.adata.delta).CString());
            //engine.showTerritory = model.board.toggleTerritoryAuto(true);
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
