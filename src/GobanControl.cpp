//
// Created by jan on 7.5.17.
//
#include <Rocket/Core.h>
#include <Shell.h>
#include "ElementGame.h"
#include "GobanControl.h"

void GobanControl::newGame(int boardSize) {
	engine.interrupt();
    engine.reset();
	engine.clearGame(boardSize, model.state.komi, model.state.handicap);
    view.animateIntro();
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
                    model.state.holdsStone = false;
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

void GobanControl::keyPress(int key, int x, int y, bool downNotUp){
    (void) x;
    (void) y;

    if (!downNotUp) {
        if (key == Rocket::Core::Input::KI_ESCAPE) {
            exit = true;
            Shell::RequestExit();
        }
        else if (key == Rocket::Core::Input::KI_F) {
            Shell::ToggleFullscreen();
        }
        else if (key == Rocket::Core::Input::KI_X) {
            view.toggleFpsLimit();
        }
        else if (key == Rocket::Core::Input::KI_O) {
            //view.toggleAnimation(Shell::GetElapsedTime());
            view.lastTime = 0.0;
            view.startTime = Shell::GetElapsedTime();
			view.animationRunning = true;
			view.requestRepaint();
        }
        else if (key == Rocket::Core::Input::KI_T) {
            model.board.toggleTerritory();
			//engine.toggleTerritory();
			view.requestRepaint();
        }
		else if (key == Rocket::Core::Input::KI_N) {
			view.toggleOverlay();
			view.requestRepaint();
		}
		else if (key == Rocket::Core::Input::KI_K) {
			//switchPlayer(0);
		}
		else if (key == Rocket::Core::Input::KI_L) {
			//switchPlayer(1);
		}

		else if (key == Rocket::Core::Input::KI_P) {
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
                //model.playMove(move);
                engine.playLocalMove(move);
                view.requestRepaint();
				firstGame = false;
            }
        }
        else if (key == Rocket::Core::Input::KI_C) {
            view.resetView();
			view.requestRepaint();
        }
        else if (key == Rocket::Core::Input::KI_U || key == Rocket::Core::Input::KI_LEFT) {
            engine.playLocalMove(model.getUndoMove());
        }
        else if (key == Rocket::Core::Input::KI_RIGHT) {
            engine.playLocalMove(model.getPassMove());
        }
        else if (key == Rocket::Core::Input::KI_D) {
            view.endPan();
        }
        else if (key == Rocket::Core::Input::KI_A) {
            view.endRotation();
        }
        else if (key == Rocket::Core::Input::KI_S) {
            view.endZoom();
        }
        else if (key == Rocket::Core::Input::KI_Q){
            newGame(9);
        }
        else if (key == Rocket::Core::Input::KI_W) {
            newGame(13);
        }
        else if (key == Rocket::Core::Input::KI_E) {
            newGame(19);
        }
        else if (key == Rocket::Core::Input::KI_V) {
            view.cycleShaders();
            view.gobanShader.setReady();
			view.gobanShader.setReady();
        }
        //view.requestRepaint();
    }
    else {
        if (key == Rocket::Core::Input::KI_RIGHT) {
            console->debug("new gamma = {0}", view.getGamma() + 0.025f);
            view.setGamma(view.getGamma() + 0.025f);
        }
        else if (key == Rocket::Core::Input::KI_LEFT) {
            console->debug("new gamma = {0}", view.getGamma() + 0.025f);
            view.setGamma(view.getGamma() - 0.025f);
        }
        else if (key == Rocket::Core::Input::KI_UP) {
            console->debug("new contrast = {0}", view.getContrast() + 0.025f);
            view.setContrast(view.getContrast() + 0.025f);
        }
        else if (key == Rocket::Core::Input::KI_DOWN) {
            console->debug("new contrast = {0}", view.getContrast() - 0.025f);
            view.setContrast(view.getContrast() - 0.025f);
        }
        else if (key == Rocket::Core::Input::KI_HOME) {
            view.resetAdjustments();
        }
        else if (key == Rocket::Core::Input::KI_D) {
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


void GobanControl::togglePlayer(int which, int delta) {
    engine.activatePlayer(which, delta);
    model.state.holdsStone = false;
}

void GobanControl::switchPlayer(int which, int newPlayerIndex) {
    console->warn("switching player {}", newPlayerIndex);
    int idx = engine.getActivePlayer(which);
    engine.activatePlayer(which, newPlayerIndex - idx);
    model.state.holdsStone = false;
}
void GobanControl::destroy() {
    console->debug("GAME DESTRUCT");
    engine.interrupt();
}
