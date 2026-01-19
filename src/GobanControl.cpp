//
// Created by jan on 7.5.17.
//
#include "ElementGame.h"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include "AppState.h"
#include "GobanControl.h"
#include "EventHandlerFileChooser.h"
#include "EventManager.h"
#include "UserSettings.h"

bool GobanControl::newGame(unsigned boardSize) {
    engine.interrupt();
    engine.reset();
	if(engine.clearGame(boardSize, model.state.komi, model.state.handicap)) {
        model.createNewRecord();
        view.animateIntro();
        return true;
	}
    return false;
}

void GobanControl::mouseClick(int button, int state, int x, int y) {

    mouseX = static_cast<float>(x);
    mouseY = static_cast<float>(y);
    view.mouseMoved(mouseX, mouseY);

    Position coord = view.getBoardCoordinate((float)x, (float)y);
    spdlog::debug("COORD [{},{}]", coord.x, coord.y);
    if(model.isPointOnBoard(coord)) {
        if (button == 0 && state == 1) {
            bool playNow = true;
            if (model.isGameOver) {
                newGame(model.getBoardSize());
                playNow = false;
            }
            else if(!engine.isRunning() && !model.isGameOver) {
               engine.run();
            }
            spdlog::debug("engine.isRunning() = {}", engine.isRunning());
            if(playNow) {
                bool humanMove = engine.humanToMove();
                if(model.state.holdsStone || !humanMove) {
                    auto move = engine.getLocalMove(coord);
                    //model.playMove(move);
                    engine.playLocalMove(move);
                    view.requestRepaint();
                }
                else {
                    model.state.holdsStone = true;
                    if(model.state.colorToMove == Color::BLACK)
                        model.state.reservoirBlack -= 1;
                    else
                        model.state.reservoirWhite -= 1;
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

    bool checked = false;
    if(cmd == "quit") {
        model.game.saveAs("");
        exit = true;
        AppState::RequestExit();
    }
    else if (cmd == "toggle_fullscreen") {
        fullscreen = AppState::ToggleFullscreen();
        checked = fullscreen;
        UserSettings::instance().setFullscreen(fullscreen);
    }
    else if (cmd == "fps") {
        checked = view.toggleFpsLimit();
    }
    else if (cmd == "animate") {
        view.lastTime = 0.0;
        view.startTime = AppState::GetElapsedTime();
        view.animationRunning = true;
        view.requestRepaint();
    }
    else if (cmd == "toggle_territory") {
        checked = model.board.toggleTerritory();
        view.requestRepaint();
    }
    else if (cmd == "toggle_overlay") {
        checked = view.toggleOverlay();
        view.requestRepaint();

    }
    else if (cmd == "play once") {
        engine.playKibitzMove();
        view.requestRepaint();
    }
    else if(cmd == "resign") {
        if(engine.humanToMove()) {
            auto move = engine.getLocalMove(Move::RESIGN);
            engine.playLocalMove(move);
        }
    }
    else if (cmd == "pass") {
        if(engine.humanToMove()) {
            auto move = engine.getLocalMove(Move::PASS);
            engine.playLocalMove(move);
        }
    }
    else if (cmd == "clear") {
        if (model.isGameOver) {
            newGame(model.getBoardSize());
        }
    }
    else if (cmd == "start") {
        if(!engine.isRunning() && !model.isGameOver) {
            engine.run();
        }
    }
    else if (cmd == "reset camera") {
        view.resetView();
        view.requestRepaint();
    }
    else if (cmd == "save camera") {
        view.saveView();
    }
    else if (cmd == "delete camera") {
        view.clearView();
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
            auto select = dynamic_cast<Rml::ElementFormControlSelect*>(doc->GetElementById("selectShader"));
            if(select) {
                int currentProgram = select->GetSelection();
                select->SetSelection((currentProgram + 1) % select->GetNumOptions());
            }
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
    else if (cmd == "increase eof") {
        view.setEof(view.getEof() + 0.0025f);
        spdlog::debug("new eof = {0}", view.getEof());
    }
    else if (cmd == "decrease eof") {
        view.setEof(view.getEof() - 0.0025f);
        spdlog::debug("new eof = {0}", view.getEof());
    }
    else if (cmd == "increase dof") {
        view.setDof(view.getDof() + 0.0025f);
        spdlog::debug("new dof = {0}", view.getDof());
    }
    else if (cmd == "decrease dof") {
        view.setDof(view.getDof() - 0.0025f);
        spdlog::debug("new dof = {0}", view.getDof());
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
    else if(cmd == "free camera toggle") {
        view.cam.setHorizontalLock(!view.cam.lock);
    }
    else if(cmd == "save") {
        //dynamic_cast<GtpEngine*>(engine.currentCoach())->issueCommand("printsgf ./lastgame.sgf");
        model.game.saveAs("");
    }
    else if(cmd == "load") {
        // Get the file chooser handler and show the dialog
        auto* handler = dynamic_cast<EventHandlerFileChooser*>(EventManager::GetEventHandler("open"));
        if (handler) {
            handler->ShowDialog();
        } else {
            spdlog::warn("File chooser handler not found");
        }
    }
    else if(cmd == "msg") {
        auto msg = parent->GetContext()->GetDocument("game_window")->GetElementById("lblMessage");
        if(msg) {
            msg->SetInnerRML("");
        }
    }
    parent->OnMenuToggle(cmd, checked);
    return true;
}

void GobanControl::keyPress(int key, int x, int y, bool downNotUp){
    (void) x;
    (void) y;

    if (!downNotUp) {
        std::string cmd(config->getCommand(static_cast<Rml::Input::KeyIdentifier>(key)));
        if (!cmd.empty()) {
            command(cmd);
            return;
        }
        if (key == Rml::Input::KI_D) {
            view.endPan();
        } else if (key == Rml::Input::KI_A) {
            view.endRotation();
        } else if (key == Rml::Input::KI_S) {
            view.endZoom();
        }
    }
    else {
        if (key == Rml::Input::KI_D) {
            view.initPan(mouseX, mouseY);
        }
        else if (key == Rml::Input::KI_A) {
            view.initRotation(mouseX, mouseY);
        }
        else if (key == Rml::Input::KI_S) {
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

bool GobanControl::setHandicap(int handicap){
    bool isOver = model.state.reason != GameState::NO_REASON;
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

    // Save shader name to user settings
    auto shaders = config->data.value("shaders", nlohmann::json::array());
    if (newShaderIndex >= 0 && newShaderIndex < static_cast<int>(shaders.size())) {
        std::string shaderName = shaders[newShaderIndex].value("name", "");
        UserSettings::instance().setShaderName(shaderName);
    }
}

void GobanControl::destroy() {
    spdlog::debug("GAME DESTRUCT");
    engine.interrupt();
}
