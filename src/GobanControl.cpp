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
#include <ctime>
#include <sstream>
#include <iomanip>

bool GobanControl::newGame(unsigned boardSize) {
    engine.interrupt();
    engine.reset();
    engine.removeSgfPlayers();  // Remove temporary SGF players from previous load
    if(engine.clearGame(boardSize, model.state.komi, model.state.handicap)) {
        model.createNewRecord();
        view.animateIntro();
        // Reset Analysis Mode menu toggle (new game starts in Match mode)
        parent->OnMenuToggle("toggle_analysis_mode", false);
        parent->refreshPlayerDropdowns();  // Update dropdowns after removing SGF players
        // Save game settings so fresh start uses these values
        UserSettings::instance().setBoardSize(static_cast<int>(boardSize));
        UserSettings::instance().setKomi(model.state.komi);
        UserSettings::instance().setHandicap(model.state.handicap);
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
            // During navigation (not at end), handle clicks specially
            if (model.game.isNavigating() && !model.game.isAtEndOfNavigation()) {
                // Block navigation while engine is thinking - would corrupt state
                if (engine.isThinking()) {
                    spdlog::debug("Navigation click blocked - engine is thinking");
                    return;
                }
                auto variations = model.game.getVariations();
                for (const auto& move : variations) {
                    if (move == Move::NORMAL && move.pos == coord) {
                        spdlog::debug("Clicked on existing variation at ({},{})", coord.col(), coord.row());
                        if (engine.navigateToVariation(move)) {
                            view.updateNavigationOverlay();
                        }
                        return;
                    }
                }
                // Clicked on new position - create new variation via navigateToVariation
                // Use SGF tree to determine correct color (model.state may be out of sync)
                Color colorToMove = model.game.getColorToMove();
                spdlog::debug("Clicked on new position during navigation - creating new variation (color={})",
                    colorToMove == Color::BLACK ? "B" : "W");
                Move newMove(coord, colorToMove);
                if (engine.navigateToVariation(newMove)) {
                    view.updateNavigationOverlay();
                    // Sync analysis mode menu if mode changed
                    if (engine.getGameMode() == GameMode::ANALYSIS) {
                        parent->OnMenuToggle("toggle_analysis_mode", true);
                    }
                    // Start game loop for AI response (after navigateToVariation completes)
                    if (!engine.isRunning()) {
                        spdlog::debug("Starting game loop for AI auto-response");
                        model.start();
                        engine.run();
                    }
                }
                return;
            }

            bool playNow = true;
            if (model.isGameOver) {
                newGame(model.getBoardSize());
                playNow = false;
            }
            else if(!engine.isRunning() && !model.isGameOver) {
               model.start();
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
        saveCurrentGame();  // Saves game and stores path for restore on next start
        exit = true;
        AppState::RequestExit();
    }
    else if (cmd == "toggle_fullscreen") {
        fullscreen = AppState::ToggleFullscreen();
        checked = fullscreen;
        UserSettings::instance().setFullscreen(fullscreen);
        view.requestRepaint();
    }
    else if (cmd == "toggle_sound") {
        bool soundEnabled = view.player.isMuted();  // Was muted, now enable
        view.player.setMuted(!soundEnabled);
        checked = soundEnabled;
        UserSettings::instance().setSoundEnabled(soundEnabled);
    }
    else if (cmd == "toggle_fps") {
        checked = view.toggleFpsLimit();
    }
    else if (cmd == "animate") {
        view.lastTime = 0.0;
        view.startTime = AppState::GetElapsedTime();
        view.animationRunning = true;
        view.requestRepaint();
    }
    else if (cmd == "toggle_territory") {
        // Only allow territory toggle at end of a scored game (not resignation)
        if (model.game.shouldShowTerritory()) {
            checked = model.board.toggleTerritory();
            view.requestRepaint(GobanView::UPDATE_STONES | GobanView::UPDATE_OVERLAY);
        }
    }
    else if (cmd == "toggle_overlay") {
        checked = view.toggleOverlay();
        view.requestRepaint();

    }
    else if (cmd == "play once") {
        // Start game loop if not running (needed for kibitz to work)
        if (!engine.isRunning() && !model.isGameOver) {
            model.start();
            engine.run();
        }
        engine.playKibitzMove();
        view.requestRepaint();
    }
    else if (cmd == "toggle_analysis_mode") {
        if (engine.getGameMode() == GameMode::MATCH) {
            if (engine.setGameMode(GameMode::ANALYSIS)) {
                checked = true;
            }
        } else {
            if (engine.setGameMode(GameMode::MATCH)) {
                checked = false;
            }
        }
        view.requestRepaint();
    }
    else if (cmd == "genmove") {
        // Reserved for future "resume match from here" feature
    }
    else if (cmd == "toggle ai vs ai") {
        engine.setAiVsAi(!engine.isAiVsAi());
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
            model.start();
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
        if (!engine.isThinking()) {
            // In match mode vs engine at end of game, double-undo to skip AI's response
            bool doubleUndo = engine.getGameMode() == GameMode::MATCH
                && !model.game.hasNextMove()
                && model.game.moveCount() > 1;
            if (engine.navigateBack()) {
                if (doubleUndo) engine.navigateBack();
                view.updateNavigationOverlay();
            }
        }
    }
    else if (cmd == "navigate_start" || cmd == "navigate_end" ||
             cmd == "navigate_back" || cmd == "navigate_forward") {
        // Block navigation while engine is thinking - would corrupt state
        spdlog::debug("Navigation command '{}': isThinking={}, isRunning={}, isGameOver={}",
            cmd, engine.isThinking(), engine.isRunning(), model.isGameOver.load());
        if (engine.isThinking()) {
            spdlog::debug("Navigation command '{}' blocked - engine is thinking", cmd);
        }
        else if (cmd == "navigate_start") {
            if (engine.navigateToStart()) view.updateNavigationOverlay();
        }
        else if (cmd == "navigate_end") {
            if (engine.navigateToEnd()) view.updateNavigationOverlay();
        }
        else if (cmd == "navigate_back") {
            if (engine.navigateBack()) view.updateNavigationOverlay();
        }
        else if (cmd == "navigate_forward") {
            if (engine.navigateForward()) view.updateNavigationOverlay();
        }
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
        model.game.saveAs("");
        // Show feedback with filename
        auto msg = parent->GetContext()->GetDocument("game_window")->GetElementById("lblMessage");
        if (msg) {
            msg->SetInnerRML(model.game.getDefaultFileName().c_str());
        }
    }
    else if(cmd == "archive") {
        // Only archive if there are games or moves to archive
        if (model.game.getNumGames() == 0 && model.game.moveCount() == 0) {
            auto msg = parent->GetContext()->GetDocument("game_window")->GetElementById("lblMessage");
            if (msg) {
                msg->SetInnerRML("Nothing to archive");
            }
            return true;
        }
        // Save current game and remember the archived filename
        std::string archivedFile = model.game.getDefaultFileName();
        model.game.saveAs("");
        // Create new timestamped filename for next session
        std::time_t t = std::time(nullptr);
        std::tm time {};
        time = *std::localtime(&t);
        std::ostringstream ss;
        std::string gamesPath = "./games";
        if (config && config->data.contains("sgf_dialog")) {
            gamesPath = config->data["sgf_dialog"].value("games_path", "./games");
        }
        ss << gamesPath << "/" << std::put_time(&time, "%Y-%m-%dT%H-%M-%S") << ".sgf";
        model.game.setDefaultFileName(ss.str());
        // Save new session path so it's used after restart
        UserSettings::instance().setLastSgfPath(model.game.getDefaultFileName());
        // Show feedback with archived filename (where games went)
        auto msg = parent->GetContext()->GetDocument("game_window")->GetElementById("lblMessage");
        if (msg) {
            msg->SetInnerRML(archivedFile.c_str());
        }
        spdlog::info("Archived to {}, new session: {}", archivedFile, model.game.getDefaultFileName());
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

    // SGF Navigation keys (on key UP)
    spdlog::debug("keyPress: key={}, downNotUp={}, isNavigating={}, viewPos={}/{}",
        key, downNotUp, model.game.isNavigating(),
        model.game.getViewPosition(), model.game.getLoadedMovesCount());

    if (!downNotUp && model.game.isNavigating()) {
        // Navigation keys - only check isThinking() for these specific keys
        bool isNavKey = (key == Rml::Input::KI_SPACE || key == Rml::Input::KI_RIGHT ||
                         key == Rml::Input::KI_LEFT || key == Rml::Input::KI_BACK);

        if (isNavKey && engine.isThinking()) {
            spdlog::debug("Navigation blocked - engine is thinking");
            return;
        }

        // Space/Right: navigate forward (or trigger kibitz at end of unfinished branch)
        if (key == Rml::Input::KI_SPACE || key == Rml::Input::KI_RIGHT) {
            if (engine.navigateForward()) {
                spdlog::debug("Navigation: forward to move {}/{}",
                    model.game.getViewPosition(), model.game.getLoadedMovesCount());
                view.updateNavigationOverlay();
                return;  // Handled navigation
            }
            // At end of finished game - nothing more to do
            if (!model.game.hasNextMove() && model.game.isAtFinishedGame()) {
                return;
            }
            // At end of unfinished branch - fall through to allow Space for kibitz
            spdlog::debug("Navigation: at end of branch, Space falls through to kibitz");
        }
        if (key == Rml::Input::KI_LEFT || key == Rml::Input::KI_BACK) {
            if (engine.navigateBack()) {
                spdlog::debug("Navigation: back to move {}/{}",
                    model.game.getViewPosition(), model.game.getLoadedMovesCount());
                view.updateNavigationOverlay();
            }
            return;
        }
    }

    std::string cmd(config->getCommand(static_cast<Rml::Input::KeyIdentifier>(key)));
    spdlog::debug("keyPress: key={} mapped to cmd='{}'", key, cmd);

    // Adjustment commands should trigger on key DOWN (enables key repeat)
    if (downNotUp && !cmd.empty()) {
        if (cmd.find("increase") == 0 || cmd.find("decrease") == 0) {
            command(cmd);
            return;
        }
    }

    if (!downNotUp) {
        // Other commands trigger on key UP (except adjustment commands which use key DOWN)
        if (!cmd.empty() && cmd.find("increase") != 0 && cmd.find("decrease") != 0) {
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
        model.game.updateKomi(komi);  // Keep game record in sync
        UserSettings::instance().setKomi(komi);
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
        if (success) {
            UserSettings::instance().setHandicap(handicap);
        }
    }
    view.requestRepaint(GobanView::UPDATE_STONES | GobanView::UPDATE_OVERLAY);
    return success;
}

void GobanControl::togglePlayer(int which, int delta) {
    size_t oldIdx = engine.getActivePlayer(which);
    size_t numPlayers = engine.getPlayers().size();
    size_t newIdx = (oldIdx + numPlayers + delta) % numPlayers;
    engine.activatePlayer(which, newIdx);
    model.state.holdsStone = false;
    // Save player to user settings (skip temporary SGF players)
    auto players = engine.getPlayers();
    if (newIdx < players.size() && !players[newIdx]->isTypeOf(Player::SGF_PLAYER)) {
        if (which == 0) {
            UserSettings::instance().setBlackPlayer(players[newIdx]->getName());
        } else {
            UserSettings::instance().setWhitePlayer(players[newIdx]->getName());
        }
    }
}

void GobanControl::switchPlayer(int which, int newPlayerIndex) {
    engine.activatePlayer(which, static_cast<size_t>(newPlayerIndex));
    model.state.holdsStone = false;
    // Save player to user settings (skip during init and for temporary SGF players)
    if (!syncingUI) {
        auto players = engine.getPlayers();
        size_t idx = engine.getActivePlayer(which);
        if (idx < players.size() && !players[idx]->isTypeOf(Player::SGF_PLAYER)) {
            if (which == 0) {
                UserSettings::instance().setBlackPlayer(players[idx]->getName());
            } else {
                UserSettings::instance().setWhitePlayer(players[idx]->getName());
            }
        }
    }
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

void GobanControl::saveCurrentGame() {
    if (model.game.moveCount() > 0) {
        model.game.saveAs("");
        UserSettings::instance().setLastSgfPath(model.game.getDefaultFileName());
        UserSettings::instance().setStartFresh(false);
    } else {
        // Board was cleared, start fresh on next launch
        UserSettings::instance().setStartFresh(true);
    }
}
