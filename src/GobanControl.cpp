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

bool GobanControl::newGame(unsigned boardSize) const {
    engine.interrupt();
    engine.reset();
    engine.removeSgfPlayers();  // Remove temporary SGF players from previous load
    view.setTsumegoMode(false);
    model.tsumegoMode = false;
    model.game.setSuppressSessionCopy(false);
    if(engine.clearGame(boardSize, model.state.komi, model.state.handicap)) {
        model.createNewRecord();
        view.animateIntro();
        parent->refreshPlayerDropdowns();  // Update dropdowns after removing SGF players
        // Save game settings so fresh start uses these values
        auto& settings = UserSettings::instance();
        settings.setBoardSize(static_cast<int>(boardSize));
        settings.setKomi(model.state.komi);
        settings.setHandicap(model.state.handicap);
        // Clear session state - user explicitly started fresh
        settings.clearSessionState();
        return true;
    }
    return false;
}

void GobanControl::mouseClick(int button, int state, int x, int y) {

    mouseX = static_cast<float>(x);
    mouseY = static_cast<float>(y);
    view.mouseMoved(mouseX, mouseY);

    Position coord = view.getBoardCoordinate(static_cast<float>(x), static_cast<float>(y));
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
                // Stone-in-hand: first click picks up, second click places
                bool inputAllowed = engine.humanToMove() || engine.getGameMode() == GameMode::ANALYSIS
                    || view.isTsumegoMode();
                if (!inputAllowed) return;

                if (!model.state.holdsStone) {
                    // First click: pick up stone
                    model.state.holdsStone = true;
                    model.updateReservoirs();
                    view.requestRepaint(GobanView::UPDATE_STONES);
                    return;
                }

                // Second click: place stone
                model.state.holdsStone = false;
                model.updateReservoirs();

                // Check if click matches an existing variation
                auto variations = model.game.getVariations();
                for (const auto& move : variations) {
                    if (move == Move::NORMAL && move.pos == coord) {
                        spdlog::debug("Clicked on existing variation at ({},{})", coord.col(), coord.row());
                        engine.navigateToVariation(move);
                        return;
                    }
                }

                // No matching variation — new move
                if (view.isTsumegoMode()) {
                    // Game thread infers BM marking from context
                    Color colorToMove = model.game.getColorToMove();
                    Move newMove(coord, colorToMove);
                    engine.navigateToVariation(newMove, false);
                    return;
                }

                // Normal mode: create new variation
                Color colorToMove = model.game.getColorToMove();
                spdlog::debug("New variation during navigation (color={})",
                    colorToMove == Color::BLACK ? "B" : "W");
                model.start();
                if (!engine.isRunning()) {
                    engine.run();
                }
                Move newMove(coord, colorToMove);
                engine.navigateToVariation(newMove);
                return;
            }

            // In tsumego mode at end of variation
            if (view.isTsumegoMode() && model.game.isAtEndOfNavigation()) {
                if (!model.game.isOnBadMovePath()) {
                    return;  // Solved — stay blocked
                }
                // Dead branch: allow exploration
                if (!model.state.holdsStone) {
                    model.state.holdsStone = true;
                    model.updateReservoirs();
                    view.requestRepaint(GobanView::UPDATE_STONES);
                    return;
                }
                model.state.holdsStone = false;
                model.updateReservoirs();
                Color colorToMove = model.game.getColorToMove();
                Move newMove(coord, colorToMove);
                engine.navigateToVariation(newMove, false);
                return;
            }

            bool playNow = true;
            if (model.isGameOver) {
                // Clicking on finished game - reuse "clear" which handles save + settings restore
                command("clear");
                playNow = false;
            }
            else if(!model.isGameOver) {
               model.start();
               if (!engine.isRunning()) {
                   engine.run();
               }
            }
            spdlog::debug("engine.isRunning() = {}", engine.isRunning());
            if(playNow) {
                if(!engine.humanToMove() || model.state.holdsStone) {
                    const auto move = engine.getLocalMove(coord);
                    engine.playLocalMove(move);
                    view.requestRepaint();
                }
                else {
                    model.state.holdsStone = true;
                    model.updateReservoirs();  // Stone in hand reduces reservoir
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

void GobanControl::command(const std::string& cmd) {

    bool checked = false;
    if(cmd == "quit") {
        // Show confirmation if game has moves
        if (model.game.moveCount() > 0 && !model.isGameOver) {
            parent->showPromptYesNoTemplate("templateQuitWithoutFinishing", [this](bool confirmed) {
                if (confirmed) {
                    saveCurrentGame();
                    exit = true;
                    AppState::RequestExit();
                }
            });
        } else {
            saveCurrentGame();  // Saves game and stores path for restore on next start
            exit = true;
            AppState::RequestExit();
        }
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
    else if (cmd == "toggle_last_move_overlay") {
        checked = view.toggleLastMoveOverlay();
        view.requestRepaint();
    }
    else if (cmd == "toggle_next_move_overlay") {
        checked = view.toggleNextMoveOverlay();
        view.requestRepaint();
    }
    else if (cmd == "play once") {
        // Don't trigger at end of finished game
        if (model.game.isAtFinishedGame()) {
            return;
        }
        // Activate genmove if needed (for kibitz to work)
        if (!model.isGameOver) {
            model.start();
            if (!engine.isRunning()) {
                engine.run();
            }
        }
        engine.playKibitzMove();
        view.requestRepaint();
    }
    else if (cmd == "toggle_analysis_mode") {
        if (view.isTsumegoMode()) {
            return;  // Tsumego mode exits only via new game or loading ordinary SGF
        }
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
        if(engine.humanToMove() || engine.getGameMode() == GameMode::ANALYSIS) {
            // During navigation, use navigateToVariation (follows existing or creates new)
            if (model.game.isNavigating() && !model.game.isAtEndOfNavigation()) {
                Color colorToMove = model.game.getColorToMove();
                Move passMove(Move::PASS, colorToMove);
                model.start();
                if (!engine.isRunning()) engine.run();
                engine.navigateToVariation(passMove);
            } else {
                model.start();
                if (!engine.isRunning()) engine.run();
                auto move = engine.getLocalMove(Move::PASS);
                engine.playLocalMove(move);
            }
        }
    }
    else if (cmd == "clear") {
        // Use different prompt for game in progress vs finished game
        const char* templateId = engine.isRunning() && !model.isGameOver && !model.tsumegoMode
            ? "templateQuitWithoutFinishing"
            : "templateClearBoard";
        parent->showPromptYesNoTemplate(templateId, [this](bool confirmed) {
            if (confirmed) {
                saveCurrentGame();  // Save before clearing
                // Restore game settings from UserSettings — model values may be from SGF/tsumego
                auto& settings = UserSettings::instance();
                int savedSize = settings.getBoardSize();
                model.state.komi = settings.getKomi();
                model.state.handicap = settings.getHandicap();
                (void) newGame(savedSize > 0 ? savedSize : model.getBoardSize());
            }
        });
    }
    else if (cmd == "start") {
        if (!model.isGameOver) {
            model.start();
            if (!engine.isRunning()) {
                engine.run();
            }
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
    else if (cmd == "zoom stones") {
        view.zoomToStones();
    }
    else if (cmd == "undo move") {
        if (!engine.isThinking()) {
            engine.navigateBack();
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
            engine.navigateToStart();
        }
        else if (cmd == "navigate_end") {
            engine.navigateToEnd();
        }
        else if (cmd == "navigate_back") {
            engine.navigateBack();
        }
        else if (cmd == "navigate_forward") {
            engine.navigateForward();
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
        if(auto doc = parent->GetContext()->GetDocument("game_window")) {
            if(auto select = dynamic_cast<Rml::ElementFormControlSelect*>(doc->GetElementById("selectShader"))) {
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
        parent->showMessage(model.game.getDefaultFileName());
    }
    else if(cmd == "archive") {
        // Only archive if there are games or moves to archive
        if (model.game.getNumGames() == 0 && model.game.moveCount() == 0) {
            parent->showMessage("Nothing to archive");
            return;
        }
        // Save current game and remember the archived filename
        std::string archivedFile = model.game.getDefaultFileName();
        model.game.saveAs("");
        // Clear session doc so new session starts fresh (prevents games from being duplicated)
        model.game.clearSession();
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
        parent->showMessage(archivedFile);
        spdlog::info("Archived to {}, new session: {}", archivedFile, model.game.getDefaultFileName());
    }
    else if(cmd == "load") {
        // Get the file chooser handler and show the dialog
        if (auto* handler = dynamic_cast<EventHandlerFileChooser*>(EventManager::GetEventHandler("open"))) {
            handler->ShowDialog();
        } else {
            spdlog::warn("File chooser handler not found");
        }
    }
    else if (cmd == "prev_game" || cmd == "next_game") {
        if (engine.isThinking()) return;
        size_t gameCount = model.game.getLoadedGameCount();
        if (gameCount <= 1) return;

        int currentIdx = model.game.getLoadedGameIndex();
        int newIdx = (cmd == "prev_game") ? currentIdx - 1 : currentIdx + 1;
        if (newIdx < 0 || newIdx >= static_cast<int>(gameCount)) return;

        bool tsumego = view.isTsumegoMode();
        setSyncingUI(true);
        engine.switchGame(newIdx, tsumego);  // Start at root in tsumego mode
        parent->refreshPlayerDropdowns();
        setSyncingUI(false);

        if (tsumego) {
            view.setTsumegoMode(true);  // Re-apply overlay settings
            model.tsumegoMode = true;
            model.game.setSuppressSessionCopy(true);
            engine.autoPlayTsumegoSetup();
        }

        view.updateLastMoveOverlay();
        view.updateNavigationOverlay();
        view.requestRepaint();
    }
    else if(cmd == "msg") {
        // Only clear if no active prompt (prompts require button click)
        if (!parent->hasActivePrompt()) {
            parent->clearMessage();
        }
    }
    else if(cmd == "prompt_yes" || cmd == "prompt_ok") {
        parent->handlePromptResponse(true);
    }
    else if(cmd == "prompt_no" || cmd == "prompt_cancel") {
        parent->handlePromptResponse(false);
    }
    parent->OnMenuToggle(cmd, checked);
}

void GobanControl::keyPress(int key, int x, int y, bool downNotUp){
    (void) x;
    (void) y;

    // Handle prompt keyboard shortcuts (on key UP)
    if (!downNotUp && parent->hasActivePrompt()) {
        if (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER) {
            parent->handlePromptResponse(true);  // Enter = confirm
            return;
        }
        if (key == Rml::Input::KI_ESCAPE) {
            parent->handlePromptResponse(false);  // Escape = cancel
            return;
        }
    }

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
            if (model.game.hasNextMove()) {
                engine.navigateForward();
                return;  // Handled navigation
            }
            // At end of finished game - nothing more to do
            if (model.game.isAtFinishedGame()) {
                return;
            }
            // At end of unfinished branch - Space falls through to kibitz
            if (key == Rml::Input::KI_RIGHT) {
                return;  // Right key doesn't trigger kibitz
            }
            // Tsumego: request engine move on dead branch via navigation
            if (view.isTsumegoMode()) {
                if (model.game.isOnBadMovePath()) {
                    engine.requestKibitzNav();
                }
                return;  // Don't fall through to "play once" — would break navigation
            }
            spdlog::debug("Navigation: at end of branch, Space falls through to kibitz");
        }
        if (key == Rml::Input::KI_LEFT || key == Rml::Input::KI_BACK) {
            engine.navigateBack();
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

bool GobanControl::setKomi(float komi) const {
    if (!model.started) {
        engine.setKomi(komi);
        model.state.komi = komi;
        model.game.updateKomi(komi);  // Keep game record in sync
        UserSettings::instance().setKomi(komi);
        return true;
    }
    return false;
}

bool GobanControl::setHandicap(int handicap) const {
    bool isOver = model.state.reason != GameState::NO_REASON;
    spdlog::debug("setHandicap: handicap={} started={} isOver={}", handicap, model.started, isOver);
    bool success = false;
    if(!model.started && !isOver) {
        model.state.handicap = handicap;
        success = newGame(model.getBoardSize());
        if (success) {
            UserSettings::instance().setHandicap(handicap);
        }
    }
    view.requestRepaint(GobanView::UPDATE_STONES | GobanView::UPDATE_OVERLAY);
    return success;
}

void GobanControl::switchPlayer(int which, int newPlayerIndex) const {
    engine.activatePlayer(which, static_cast<size_t>(newPlayerIndex));
    model.state.holdsStone = false;
    // Persist player choice — only switchPlayer saves to UserSettings,
    // so SGF player activations (matchSgfPlayers) don't leak into user.json.
    // Save BOTH players to ensure consistency (other player may have been set from SGF
    // without updating UserSettings, and would otherwise revert to stale default).
    auto players = engine.getPlayers();
    size_t blackIdx = engine.getActivePlayer(0);
    size_t whiteIdx = engine.getActivePlayer(1);
    if (blackIdx < players.size() && whiteIdx < players.size()) {
        UserSettings::instance().setPlayers(
            players[blackIdx]->getName(),
            players[whiteIdx]->getName());
    }
}

void GobanControl::switchShader(int newShaderIndex) const {
    view.switchShader(newShaderIndex);

    // Save shader name to user settings
    auto shaders = config->data.value("shaders", nlohmann::json::array());
    if (newShaderIndex >= 0 && newShaderIndex < static_cast<int>(shaders.size())) {
        std::string shaderName = shaders[newShaderIndex].value("name", "");
        UserSettings::instance().setShaderName(shaderName);
    }
}

void GobanControl::destroy() const {
    spdlog::debug("GAME DESTRUCT");
    engine.interrupt();
}

void GobanControl::saveCurrentGame() const {
    auto& settings = UserSettings::instance();

    if (model.game.moveCount() > 0) {
        model.game.saveAs("");
        settings.setLastSgfPath(model.game.getDefaultFileName());
        settings.setStartFresh(false);
    } else {
        // Board was cleared, start fresh on next launch
        settings.setStartFresh(true);
    }

    // Save session state for position restoration on restart
    bool isExternal = model.game.hasLoadedExternalDoc();
    std::string sessionFile = isExternal
        ? model.game.getLoadedFilePath()
        : model.game.getDefaultFileName();

    // Only save session if there's a file to restore from
    if (!sessionFile.empty() && (model.game.moveCount() > 0 || isExternal)) {
        auto treePath = model.game.getTreePath();
        settings.setSessionFile(sessionFile);
        settings.setSessionGameIndex(model.game.getLoadedGameIndex());
        settings.setSessionTreePathLength(treePath.length);
        settings.setSessionTreePath(treePath.branchChoices);
        settings.setSessionIsExternal(isExternal);
        settings.setSessionTsumegoMode(model.tsumegoMode);
        settings.setSessionAnalysisMode(engine.getGameMode() == GameMode::ANALYSIS);
        spdlog::info("Saved session state: file={}, gameIndex={}, pathLen={}, branchChoices={}, tsumego={}, analysis={}",
            sessionFile, model.game.getLoadedGameIndex(), treePath.length, treePath.branchChoices.size(),
            model.tsumegoMode.load(), engine.getGameMode() == GameMode::ANALYSIS);
    } else {
        settings.clearSessionState();
    }

    // Save current camera for session restore (auto-saved, not the preset)
    view.saveCurrentView();

    settings.save();
}
