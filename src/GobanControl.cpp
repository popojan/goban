//
// Created by jan on 7.5.17.
//
#include "ElementGame.h"
#include <Rocket/Core.h>
#include <Shell.h>
#include "GobanControl.h"

bool GobanControl::newGame(int boardSize) {
	engine.interrupt();
    engine.reset();
	if(engine.clearGame(boardSize, model.state.komi, model.state.handicap)) {
        view.animateIntro();
        return true;
	}
    return false;
}

void GobanControl::mouseClick(int button, int state, int x, int y) {

    mouseX = static_cast<float>(x);
    mouseY = static_cast<float>(y);
    view.mouseMoved(mouseX, mouseY);

    Position coord = view.getBoardCoordinate(x, y);
    spdlog::debug("COORD [{},{}]", coord.x, coord.y);
    if(model.isPointOnBoard(coord)) {
        if (button == 0 && state == 1) {
            bool playNow = true;
            if (model.isGameOver()) {
                newGame(model.getBoardSize());
                playNow = false;
            }
            else if(!engine.isRunning() && !model.over) {
               engine.run();
            }
            if(playNow) {
                bool humanMove = engine.humanToMove();
                if(model.state.holdsStone || !humanMove) {
                    auto move = engine.getLocalMove(coord);
                    //model.playMove(move);
                    engine.playLocalMove(move);
                    view.requestRepaint();
                    firstGame = false;
                }
                else if(humanMove) {
                    model.state.holdsStone = true;
                    view.requestRepaint(GobanView::UPDATE_STONES);
                }
            }
        }
    }
    if (button == 1 && state == 1) {
        view.initRotation(x, y);
        view.requestRepaint();
    }
    else if (button == 1 && state == 0) {
        view.endRotation();
        view.requestRepaint();
    }
    else if (button == 2 && state == 1) {
        view.initPan(x, y);
        view.requestRepaint();
    }
    else if (button == 2 && state == 0) {
        view.endPan();
        view.requestRepaint();
    }
    else if (button == 3 && state == 1) {
        view.zoomRelative(-1);
        view.requestRepaint();
    }
    else if (button == 4 && state == 1) {
        view.zoomRelative(+1);
        view.requestRepaint();
    }
}

bool GobanControl::command(const std::string& cmd) {

    if(cmd == "quit") {
        exit = true;
        Shell::RequestExit();
    }
    else if (cmd == "fullscreen") {
        Shell::ToggleFullscreen();
    }
    else if (cmd == "fps") {
        view.toggleFpsLimit();
    }
    else if (cmd == "animate") {
        view.lastTime = 0.0;
        view.startTime = Shell::GetElapsedTime();
        view.animationRunning = true;
        view.requestRepaint();
    }
    else if (cmd == "territory") {
        model.board.toggleTerritory();
        view.requestRepaint();
    }
    else if (cmd == "overlay") {
        view.toggleOverlay();
        view.requestRepaint();
    }
    else if (cmd == "play pass") {
        bool playNow = !firstGame;
        if (model.isGameOver()) {
            newGame(model.getBoardSize());
            playNow = false;
        }
        else if(view.board.isEmpty()) {
            if(!engine.isRunning())
                engine.run();
        }
        if(playNow) {
            auto move = model.getPassMove();
            engine.playLocalMove(move);
            view.requestRepaint();
            firstGame = false;
        }
    }
    else if (cmd == "reset camera") {
        view.resetView();
        view.requestRepaint();
    }
    else if (cmd == "undo move") {
        engine.playLocalMove(model.getUndoMove());
    }
    else if (cmd == "pan camera") {
        view.endPan();
    }
    else if (cmd == "rotate camera") {
        view.endRotation();
    }
    else if (cmd == "zoom camera") {
        view.endZoom();
    }
    else if (cmd == "cycle shaders") {
        //TODO generalize linked select boxes and cycle commands
        auto doc = parent->GetContext()->GetDocument("game_window");
        if(doc) {
            auto select = dynamic_cast<Rocket::Controls::ElementFormControlSelect *>(doc->GetElementById("selectShader"));
            int currentProgram = select->GetSelection();
            select->SetSelection((currentProgram + 1) % select->GetNumOptions());
        }
    }
    else if (cmd == "increase gamma") {
        spdlog::debug("new gamma = {0}", view.getGamma() + 0.025f);
        view.setGamma(view.getGamma() + 0.025f);
    }
    else if (cmd == "decrease gamma") {
        spdlog::debug("new gamma = {0}", view.getGamma() + 0.025f);
        view.setGamma(view.getGamma() - 0.025f);
    }
    else if (cmd == "increase contrast") {
        spdlog::debug("new contrast = {0}", view.getContrast() + 0.025f);
        view.setContrast(view.getContrast() + 0.025f);
    }
    else if (cmd == "decrease contrast") {
        spdlog::debug("new contrast = {0}", view.getContrast() - 0.025f);
        view.setContrast(view.getContrast() - 0.025f);
    }
    else if (cmd == "reset contrast and gamma") {
        view.resetAdjustments();
    }
    return true;
}

void GobanControl::keyPress(int key, int x, int y, bool downNotUp){
    (void) x;
    (void) y;

    if (!downNotUp) {
        std::string cmd(config.getCommand(static_cast<Rocket::Core::Input::KeyIdentifier>(key)));
        if (!cmd.empty()) {
            command(cmd);
            return;
        }
        if (key == Rocket::Core::Input::KI_D) {
            view.endPan();
        } else if (key == Rocket::Core::Input::KI_A) {
            view.endRotation();
        } else if (key == Rocket::Core::Input::KI_S) {
            view.endZoom();
        }
    }
    else {
        if (key == Rocket::Core::Input::KI_D) {
            view.initPan(mouseX, mouseY);
        }
        else if (key == Rocket::Core::Input::KI_A) {
            view.initRotation(mouseX, mouseY);
        }
        else if (key == Rocket::Core::Input::KI_S) {
            view.initZoom(mouseX, mouseY);
        }
    }
}

void GobanControl::mouseMove(int x, int y){
    mouseX = static_cast<float>(x);
    mouseY = static_cast<float>(y);
    view.mouseMoved(mouseX, mouseY);
    view.moveCursor(mouseX, mouseY);
}

bool GobanControl::setKomi(float komi) {
    bool isRunning = engine.isRunning();
    if(!isRunning) {
        engine.setKomi(komi);
        model.state.komi = komi;
        return true;
    }
    return false;
}

void GobanControl::increaseHandicap(){
    bool isOver = model.state.adata.reason != GameState::NOREASON;
    bool isRunning = engine.isRunning();
    if(!isRunning && !isOver) {
        int nextHandicap = model.state.handicap + 1;
		if (engine.setFixedHandicap(nextHandicap)) {
      //komi configurable independent of handicap
    }
	    view.requestRepaint(GobanView::UPDATE_STONES | GobanView::UPDATE_OVERLAY);
    }
}

bool GobanControl::setHandicap(int handicap){
    bool isOver = model.state.adata.reason != GameState::NOREASON;
    bool isRunning = engine.isRunning();
    bool success = false;
    if(!isRunning && !isOver) {
        model.state.handicap = handicap;
        success = newGame(model.getBoardSize());
    }
    view.requestRepaint(GobanView::UPDATE_STONES | GobanView::UPDATE_OVERLAY);
    return success;
}

void GobanControl::togglePlayer(int which, int delta) {
    engine.activatePlayer(which, delta);
    model.state.holdsStone = false;
}

void GobanControl::switchPlayer(int which, int newPlayerIndex) {
    int idx = engine.getActivePlayer(which);
    engine.activatePlayer(which, newPlayerIndex - idx);
    model.state.holdsStone = false;
}

void GobanControl::switchShader(int newShaderIndex) {
    view.switchShader(newShaderIndex);
}

void GobanControl::destroy() {
    spdlog::debug("GAME DESTRUCT");
    engine.interrupt();
}
