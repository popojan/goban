//
// Created by jan on 7.5.17.
//
#include <Rocket/Core.h>
#include <Shell.h>
#include "ElementGame.h"
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
    console->debug("COORD [{},{}]", coord.x, coord.y);
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

void GobanControl::initControls() {
    controls
    .add("quit", Rocket::Core::Input::KI_ESCAPE)
    .add("fullscreen", Rocket::Core::Input::KI_F)
    .add("fps", Rocket::Core::Input::KI_X)
    .add("animate", Rocket::Core::Input::KI_O)
    .add("territory", Rocket::Core::Input::KI_T)
    .add("overlay", Rocket::Core::Input::KI_N)
    .add("play/pass", Rocket::Core::Input::KI_P)
    .add("reset camera", Rocket::Core::Input::KI_C)
    .add("undo", Rocket::Core::Input::KI_U)
    //.add("prev move", Rocket::Core::Input::KI_LEFT)
    //.add("next move", Rocket::Core::Input::KI_RIGHT)
    .add("pan camera", Rocket::Core::Input::KI_D)
    .add("rotate camera", Rocket::Core::Input::KI_A)
    .add("zoom camera", Rocket::Core::Input::KI_S)
    .add("cycle shaders", Rocket::Core::Input::KI_V)
    .add("increase gamma", Rocket::Core::Input::KI_RIGHT)
    .add("decrease gamma", Rocket::Core::Input::KI_LEFT)
    .add("increase contrast", Rocket::Core::Input::KI_UP)
    .add("decrease contrast", Rocket::Core::Input::KI_DOWN)
    .add("reset contrast and gamma", Rocket::Core::Input::KI_HOME);
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
    else if (cmd == "play/pass") {
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
        view.cycleShaders();
        view.gobanShader.setReady();
        view.gobanShader.setReady();
    }
    else if (cmd == "increase gamma") {
        console->debug("new gamma = {0}", view.getGamma() + 0.025f);
        view.setGamma(view.getGamma() + 0.025f);
    }
    else if (cmd == "decrease gamma") {
        console->debug("new gamma = {0}", view.getGamma() + 0.025f);
        view.setGamma(view.getGamma() - 0.025f);
    }
    else if (cmd == "increase contrast") {
        console->debug("new contrast = {0}", view.getContrast() + 0.025f);
        view.setContrast(view.getContrast() + 0.025f);
    }
    else if (cmd == "decrease contrast") {
        console->debug("new contrast = {0}", view.getContrast() - 0.025f);
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
        std::string cmd;
        auto it = controls.keyToCommand.find(static_cast<Rocket::Core::Input::KeyIdentifier>(key));
        if (it != controls.keyToCommand.end()) {
            cmd = it->second;
        }
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
void GobanControl::destroy() {
    console->debug("GAME DESTRUCT");
    engine.interrupt();
}
