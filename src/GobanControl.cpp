#include "ElementGame.h"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include "AppState.h"
#include "GobanControl.h"
#include "EventHandlerFileChooser.h"
#include "EventManager.h"
#include "UserSettings.h"
#include "ScenarioRecorder.h"
#include <algorithm>
#include <cctype>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace {

// Splits a command line on whitespace.
std::vector<std::string> splitCommand(const std::string& cmd) {
    std::vector<std::string> tokens;
    std::istringstream ss(cmd);
    std::string token;
    while (ss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

// Strict integer parse — the whole token must be consumed.
bool parseInt(const std::string& text, int& out) {
    try {
        size_t consumed = 0;
        int value = std::stoi(text, &consumed);
        if (consumed != text.size()) {
            return false;
        }
        out = value;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// Strict float parse — the whole token must be consumed.
bool parseFloat(const std::string& text, float& out) {
    try {
        size_t consumed = 0;
        float value = std::stof(text, &consumed);
        if (consumed != text.size()) {
            return false;
        }
        out = value;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::string toLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

} // namespace

bool GobanControl::newGame(unsigned boardSize) const {
    // Starting a new game discards the current one, so a move the engine is
    // still computing is worthless. Rather than block the UI waiting for it
    // (GTP cannot abort a command in flight), hand the work to the game thread,
    // which drops the move and then runs this. See docs/adr/0001.
    std::string busyEngine;
    const bool ran = engine.runWhenEngineFree(
        [this, boardSize]() { newGameNow(boardSize); }, &busyEngine);
    if (ran) {
        finishGameReplacement();
    } else {
        parent->showMessage("Waiting for " + busyEngine + "...");
    }
    // Accepted either way; a deferred failure is only logged, since the caller
    // (e.g. the board-size dropdown) has already moved on.
    return true;
}

bool GobanControl::newGameNow(unsigned boardSize) const {
    // Engine and model work only. This may run on the game thread (when it was
    // deferred past a genmove), and RmlUi is not thread safe — every widget and
    // view update happens later in finishGameReplacement(), on the UI thread.
    engine.interrupt();
    engine.reset();
    engine.removeSgfPlayers();  // Remove temporary SGF players from previous load
    model.tsumegoMode = false;
    model.game.setSuppressSessionCopy(false);
    if(!engine.clearGame(boardSize, model.state.komi, model.state.handicap)) {
        return false;
    }
    model.createNewRecord();

    // Save game settings so fresh start uses these values (single save)
    auto& settings = UserSettings::instance();
    auto players = engine.getPlayers();
    size_t blackIdx = engine.getActivePlayer(0);
    size_t whiteIdx = engine.getActivePlayer(1);
    std::string blackName = (blackIdx < players.size()) ? players[blackIdx]->getName() : "Human";
    std::string whiteName = (whiteIdx < players.size()) ? players[whiteIdx]->getName() : "Human";
    settings.setGameSettings(
        static_cast<int>(boardSize), model.state.komi, model.state.handicap,
        blackName, whiteName);
    // Clear session state - user explicitly started fresh
    settings.clearSessionState();
    return true;
}

void GobanControl::finishGameReplacement() const {
    // UI half of a game-replacing action, always on the UI thread: either called
    // straight after the engine work, or from ElementGame's poll once a deferred
    // action completed on the game thread.
    view.setTsumegoMode(model.tsumegoMode);
    if (!model.tsumegoMode) {
        view.animateIntro();
    }

    // refreshPlayerDropdowns() raises its own WidgetEventGuard already.
    parent->refreshPlayerDropdowns();

    view.updateLastMoveOverlay();
    view.updateNavigationOverlay();
    view.requestRepaint();
}

void GobanControl::mouseClick(int button, int state, int x, int y) {

    mouseX = static_cast<float>(x);
    mouseY = static_cast<float>(y);
    view.mouseMoved(mouseX, mouseY);

    // Click anywhere (except No button, handled by RmlUI) confirms active prompt
    if (button == 0 && state == 1 && parent->hasActivePrompt()) {
        parent->handlePromptResponse(true);
        return;
    }

    Position coord = view.getBoardCoordinate(static_cast<float>(x), static_cast<float>(y));
    spdlog::debug("COORD [{},{}]", coord.x, coord.y);
    if(model.isPointOnBoard(coord)) {
        // Block stone placement until initialization is complete (players set)
        if (!acceptsUiEvents()) return;
        if (button == 0 && state == 1) {
            // Record here rather than in boardClick(), which is shared with the
            // `click` command — that path is already recorded by command().
            ScenarioRecorder::instance().recordAction(
                "click", {std::to_string(coord.col()), std::to_string(coord.row())},
                dumpState());
            {
                ScenarioRecorder::SuppressNested noNestedRecords;
                boardClick(coord);
            }
            ScenarioRecorder::instance().recordState(dumpState());
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

// Left click on an on-board intersection. Callers must have validated that the
// point is on the board and that initialization is complete.
void GobanControl::boardClick(const Position& coord) {
    // During navigation (not at end), handle clicks specially
    if (model.game.isNavigating() && !model.game.isAtEndOfNavigation()) {
        // Block navigation while engine is thinking - would corrupt state
        if (engine.isThinking()) {
            spdlog::debug("Navigation click blocked - engine is thinking");
            return;
        }
        // Stone-in-hand: first click picks up, second click places
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

    if (model.phase() == GamePhase::Finished) {
        // Clicking on finished game - reuse "clear" which handles save + settings restore
        command("clear");
        return;
    }

    // Taking a stone out of the bowl is not yet a move, so it must not start the
    // game. model.start() used to run before this branch, which flipped the
    // phase to Playing with nothing recorded — and the phase is what the rest of
    // the UI reads: komi locked itself (setKomi accepts Setup and Paused only)
    // and the Start button greyed out, both on a board that was still empty.
    if (engine.humanToMove() && !model.state.holdsStone) {
        model.state.holdsStone = true;
        model.updateReservoirs();  // Stone in hand reduces reservoir
        view.requestRepaint(GobanView::UPDATE_STONES);
        return;
    }

    // A stone is actually being placed: this is the act that starts the game.
    model.start();
    if (!engine.isRunning()) {
        engine.run();
    }
    spdlog::debug("engine.isRunning() = {}", engine.isRunning());
    const auto move = engine.getLocalMove(coord);
    engine.playLocalMove(move);
    view.requestRepaint();
}

void GobanControl::buildRegistry() {
    auto add = [this](const char* name, int minArgs, int maxArgs, const char* help,
                      std::function<void(CommandContext&)> handler) {
        registry[name] = CommandEntry{std::move(handler), minArgs, maxArgs, help};
    };

    // Shared guard for the four navigation commands. GTP traffic while an engine
    // is thinking would corrupt its board state, and in a locked bot-versus-bot
    // match the human is a spectator — both terms come from availableActions(),
    // so these keybindings say exactly what the greyed-out buttons say.
    //
    // The second term is why this is no longer a bare isThinking() test: the game
    // loop clears playerToMove *before* its 500 ms inter-move sleep, so
    // isThinking() is false for that window and navigation keys were quietly
    // accepted mid-match — pausing a running match the toolbar had refused to
    // let anyone touch. Blocking here does *not* skip the post-dispatch menu
    // update, matching the original combined branch.
    auto navigate = [this](const char* name, const std::function<void()>& action) {
        spdlog::debug("Navigation command '{}': isThinking={}, isRunning={}, phase={}",
            name, engine.isThinking(), engine.isRunning(), phaseName(model.phase()));
        if (!actions().navigate) {
            spdlog::debug("Navigation command '{}' refused by availableActions()", name);
            return;
        }
        action();
    };

    // Shared body of prev_game/next_game.
    auto stepLoadedGame = [this](CommandContext& ctx, int delta) {
        size_t gameCount = model.game.getLoadedGameCount();
        if (gameCount <= 1) { ctx.notifyMenu = false; return; }

        int currentIdx = model.game.getLoadedGameIndex();
        int newIdx = currentIdx + delta;
        if (newIdx < 0 || newIdx >= static_cast<int>(gameCount)) { ctx.notifyMenu = false; return; }

        // Switching game replaces the one on screen, so a move still being
        // computed is worthless — defer past it rather than refusing.
        const bool tsumego = view.isTsumegoMode();
        std::string busyEngine;
        const bool ran = engine.runWhenEngineFree([this, newIdx, tsumego]() {
            engine.switchGame(newIdx, tsumego);  // Start at root in tsumego mode
            if (tsumego) {
                model.tsumegoMode = true;
                model.game.setSuppressSessionCopy(true);
                engine.autoPlayTsumegoSetup();
            }
        }, &busyEngine);

        if (ran) {
            finishGameReplacement();
        } else {
            parent->showMessage("Waiting for " + busyEngine + "...");
        }
    };

    add("help", 0, 0, "list every registered command", [this](CommandContext&) {
        listCommands();
    });

    add("nop", 0, 0, "does nothing; used by UI elements that only swallow the event",
        [](CommandContext&) {
    });

    add("quit", 0, 0, "save and exit (prompts when a game is in progress)", [this](CommandContext&) {
        // saveCurrentGame() is called by main.cpp cleanup on all exit paths
        if (model.game.moveCount() > 0 && model.phase() != GamePhase::Finished) {
            parent->showPromptYesNoTemplate("templateQuitWithoutFinishing", [this](bool confirmed) {
                if (confirmed) {
                    exit = true;
                    AppState::RequestExit();
                }
            });
        } else {
            exit = true;
            AppState::RequestExit();
        }
    });

    add("toggle_fullscreen", 0, 0, "toggle fullscreen", [this](CommandContext& ctx) {
        fullscreen = AppState::ToggleFullscreen();
        ctx.checked = fullscreen;
        UserSettings::instance().setFullscreen(fullscreen);
        view.requestRepaint();
    });

    add("toggle_sound", 0, 0, "toggle sound effects", [this](CommandContext& ctx) {
        bool soundEnabled = view.player.isMuted();  // Was muted, now enable
        view.player.setMuted(!soundEnabled);
        ctx.checked = soundEnabled;
        UserSettings::instance().setSoundEnabled(soundEnabled);
    });

    add("toggle_fps", 0, 0, "toggle the frame rate limiter", [this](CommandContext& ctx) {
        ctx.checked = view.toggleFpsLimit();
    });

    add("animate", 0, 0, "run the camera intro animation", [this](CommandContext&) {
        view.lastTime = 0.0;
        view.startTime = AppState::GetElapsedTime();
        view.animationRunning = true;
        view.requestRepaint();
    });

    add("toggle_territory", 0, 0, "toggle territory display", [this](CommandContext& ctx) {
        // Only at the end of a scored game — a resignation counted nothing, so
        // there is no territory to show. The same answer greys the button.
        // On refusal the menu is still updated (with checked == false), unselecting it.
        if (actions().territory) {
            ctx.checked = model.board.toggleTerritory();
            view.requestRepaint(GobanView::UPDATE_STONES | GobanView::UPDATE_OVERLAY);
        }
    });

    add("toggle_last_move_overlay", 0, 0, "toggle the last move marker", [this](CommandContext& ctx) {
        ctx.checked = view.toggleLastMoveOverlay();
        view.requestRepaint();
    });

    add("toggle_next_move_overlay", 0, 0, "toggle the next move marker", [this](CommandContext& ctx) {
        ctx.checked = view.toggleNextMoveOverlay();
        view.requestRepaint();
    });

    add("play once", 0, 0, "ask the kibitz engine for one move", [this](CommandContext& ctx) {
        // Refused at the end of a finished game, while an engine is already
        // thinking, and in a locked bot-bot match — the three terms behind the
        // greyed Kibitz button. The command used to test only the first, so the
        // keybinding queued a second request onto an engine mid-genmove.
        if (!actions().kibitz) {
            spdlog::debug("play once refused by availableActions()");
            ctx.notifyMenu = false;
            return;
        }
        // Activate genmove if needed (for kibitz to work)
        if (model.phase() != GamePhase::Finished) {
            model.start();
            if (!engine.isRunning()) {
                engine.run();
            }
        }
        engine.playKibitzMove();
        view.requestRepaint();
    });

    add("toggle_analysis_mode", 0, 0, "switch between match and analysis mode", [this](CommandContext& ctx) {
        if (view.isTsumegoMode()) {
            // Tsumego mode exits only via new game or loading ordinary SGF.
            // Leave the menu toggle alone — OnUpdate keeps it lit for tsumego.
            ctx.notifyMenu = false;
            return;
        }
        if (engine.getGameMode() == GameMode::MATCH) {
            if (engine.setGameMode(GameMode::ANALYSIS)) {
                ctx.checked = true;
            } else {
                // Refused for a human-versus-human game, and silently until now:
                // the menu entry simply failed to light up. Analysis mode answers
                // every move with the kibitz engine regardless of who is assigned
                // to a colour, so entering it here would quietly turn the game
                // into human-versus-engine. Kibitz on demand needs no mode change
                // — it already works in a match. See docs/game-modes.md.
                parent->showMessage("Analysis mode answers every move — use Kibitz in match mode instead");
            }
        } else {
            if (engine.setGameMode(GameMode::MATCH)) {
                ctx.checked = false;
            }
        }
        view.requestRepaint();
    });

    add("genmove", 0, 0, "reserved for a future \"resume match from here\" feature (no-op)",
        [](CommandContext&) {
    });

    // Registered but currently unbound: no keybinding in config/base.json and no
    // menu entry in any config/gui/*/goban.rml (see backlog/issue-48-better-genmove.md).
    add("toggle ai vs ai", 0, 0, "toggle engine-versus-engine play", [this](CommandContext&) {
        engine.setAiVsAi(!engine.isAiVsAi());
        view.requestRepaint();
    });

    add("resign", 0, 0, "resign the game", [this](CommandContext& ctx) {
        if (!acceptsUiEvents()) { ctx.notifyMenu = false; return; }  // Block until initialization complete
        if (!canResign()) {
            ctx.notifyMenu = false;
            // The button is greyed in this case, but the keybinding is not, so
            // say why rather than swallowing the key.
            if (!model.game.isAtEndOfNavigation()) {
                parent->showMessage("Navigate to the end of the game to resign");
            }
            return;
        }
        // Resigning is an act of play, so it implies starting — the same thing
        // `pass` does from a paused position. Without this the move was merely
        // queued and fired later, on whatever start happened next.
        model.start();
        if (!engine.isRunning()) engine.run();
        auto move = engine.getLocalMove(Move::RESIGN);
        engine.playLocalMove(move);
    });

    add("pass", 0, 0, "pass", [this](CommandContext& ctx) {
        if (!actions().pass) {
            spdlog::debug("pass refused by availableActions()");
            ctx.notifyMenu = false;
            return;
        }
        // During navigation, pass creates a variation (game is paused — no turn restriction)
        if (model.game.isNavigating() && !model.game.isAtEndOfNavigation()) {
            Color colorToMove = model.game.getColorToMove();
            Move passMove(Move::PASS, colorToMove);
            model.start();
            if (!engine.isRunning()) engine.run();
            engine.navigateToVariation(passMove);
        } else if (engine.humanToMove() || engine.getGameMode() == GameMode::ANALYSIS) {
            model.start();
            if (!engine.isRunning()) engine.run();
            auto move = engine.getLocalMove(Move::PASS);
            engine.playLocalMove(move);
        }
    });

    add("clear", 0, 0, "clear the board and start over (prompts first)", [this](CommandContext& ctx) {
        // Nothing recorded means nothing to clear, and the prompt would ask
        // about discarding an empty board. Same answer greys the button.
        if (!actions().clear) {
            spdlog::debug("clear refused by availableActions()");
            ctx.notifyMenu = false;
            return;
        }
        // Use different prompt for game in progress vs finished game
        // Same prompt as the board-size and handicap dropdowns: all three
        // replace the game, and all three save it first, so neither "quit" nor
        // "discard" describes what happens. Only `quit` still ends the app.
        const char* templateId = engine.isRunning() && model.phase() != GamePhase::Finished
                                 && !model.tsumegoMode
            ? "templateStartNewGame"
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
    });

    add("start", 0, 0, "start/resume engine play", [this](CommandContext& ctx) {
        spdlog::debug("start command: acceptsUiEvents={}, phase={}, isRunning={}",
            acceptsUiEvents(), phaseName(model.phase()), engine.isRunning());
        // Start hands the turn to an engine, so it needs one waiting to move and
        // a game that is neither already running nor over. The command used to
        // test the phase alone, which meant it accepted a human-versus-human
        // game where the button was greyed and there was nothing to hand over:
        // `pass` and a board click already start the game themselves.
        if (!actions().start) {
            spdlog::debug("start refused by availableActions()");
            ctx.notifyMenu = false;
            return;
        }
        model.start();
        if (!engine.isRunning()) {
            engine.run();
        }
        spdlog::info("Game started from menu");
    });

    add("reset camera", 0, 0, "restore the default camera", [this](CommandContext&) {
        view.resetView();
        view.requestRepaint();
    });

    add("save camera", 0, 0, "store the current camera as the preset", [this](CommandContext&) {
        view.saveView();
    });

    add("delete camera", 0, 0, "forget the stored camera preset", [this](CommandContext&) {
        view.clearView();
    });

    add("zoom stones", 0, 0, "frame the stones on the board", [this](CommandContext&) {
        view.zoomToStones();
    });

    add("undo move", 0, 0, "take back the last move", [this](CommandContext&) {
        // Undo is navigateBack under another name, so it answers to the same
        // rule as the navigation buttons — including the bot-bot lock the bare
        // isThinking() test here used to miss.
        if (!actions().undo) {
            spdlog::debug("undo move refused by availableActions()");
            return;
        }
        engine.navigateBack();
    });

    add("navigate_start", 0, 0, "jump to the start of the game", [this, navigate](CommandContext&) {
        navigate("navigate_start", [this] { engine.navigateToStart(); });
    });

    add("navigate_end", 0, 0, "jump to the end of the current branch", [this, navigate](CommandContext&) {
        navigate("navigate_end", [this] { engine.navigateToEnd(); });
    });

    add("navigate_back", 0, 0, "step one move back", [this, navigate](CommandContext&) {
        navigate("navigate_back", [this] { engine.navigateBack(); });
    });

    add("navigate_forward", 0, 0, "step one move forward", [this, navigate](CommandContext&) {
        navigate("navigate_forward", [this] { engine.navigateForward(); });
    });

    add("pan camera", 0, 0, "end an interactive camera pan", [this](CommandContext&) {
        view.endPan();
    });

    add("rotate camera", 0, 0, "end an interactive camera rotation", [this](CommandContext&) {
        view.endRotation();
    });

    add("zoom camera", 0, 0, "end an interactive camera zoom", [this](CommandContext&) {
        view.endZoom();
    });

    add("cycle shaders", 0, 0, "select the next shader", [this](CommandContext&) {
        if(auto doc = parent->GetContext()->GetDocument("game_window")) {
            if(auto select = dynamic_cast<Rml::ElementFormControlSelect*>(doc->GetElementById("selectShader"))) {
                int currentProgram = select->GetSelection();
                select->SetSelection((currentProgram + 1) % select->GetNumOptions());
            }
        }
    });

    add("increase gamma", 0, 0, "raise gamma", [this](CommandContext&) {
        spdlog::debug("new gamma = {0}", view.getGamma() + 0.025f);
        view.setGamma(view.getGamma() + 0.025f);
    });

    add("decrease gamma", 0, 0, "lower gamma", [this](CommandContext&) {
        spdlog::debug("new gamma = {0}", view.getGamma() + 0.025f);
        view.setGamma(view.getGamma() - 0.025f);
    });

    add("increase eof", 0, 0, "raise the stereo eye offset factor", [this](CommandContext&) {
        view.setEof(view.getEof() + 0.0025f);
        spdlog::debug("new eof = {0}", view.getEof());
    });

    add("decrease eof", 0, 0, "lower the stereo eye offset factor", [this](CommandContext&) {
        view.setEof(view.getEof() - 0.0025f);
        spdlog::debug("new eof = {0}", view.getEof());
    });

    add("increase dof", 0, 0, "raise the depth of field", [this](CommandContext&) {
        view.setDof(view.getDof() + 0.0025f);
        spdlog::debug("new dof = {0}", view.getDof());
    });

    add("decrease dof", 0, 0, "lower the depth of field", [this](CommandContext&) {
        view.setDof(view.getDof() - 0.0025f);
        spdlog::debug("new dof = {0}", view.getDof());
    });

    add("increase contrast", 0, 0, "raise contrast", [this](CommandContext&) {
        spdlog::debug("new contrast = {0}", view.getContrast() + 0.025f);
        view.setContrast(view.getContrast() + 0.025f);
    });

    add("decrease contrast", 0, 0, "lower contrast", [this](CommandContext&) {
        spdlog::debug("new contrast = {0}", view.getContrast() - 0.025f);
        view.setContrast(view.getContrast() - 0.025f);
    });

    add("reset contrast and gamma", 0, 0, "restore default contrast and gamma", [this](CommandContext&) {
        view.resetAdjustments();
    });

    add("free camera toggle", 0, 0, "toggle the horizontal camera lock", [this](CommandContext&) {
        view.cam.setHorizontalLock(!view.cam.lock);
    });

    add("report_bug", 0, 0, "write the recent interactions to a replayable bug report",
        [this](CommandContext&) {
        // The recorder is always on, so this works retroactively: notice
        // something odd, then save what led up to it.
        const std::string path = ScenarioRecorder::instance().save("reports");
        if (path.empty()) {
            parent->showMessage("Nothing recorded yet");
        } else {
            parent->showMessage(path);
        }
    });

    add("save", 0, 0, "save the game record now", [this](CommandContext&) {
        // Nothing changed since the last save means nothing to write; the same
        // answer greys the button. Said out loud rather than swallowed, because
        // a Save that appears to do nothing reads as a failure.
        if (!actions().save) {
            parent->showMessage("Nothing to save");
            return;
        }
        // Report what actually happened. This used to show the filename
        // unconditionally, so a write that failed — a missing games folder was
        // enough — was indistinguishable from one that succeeded.
        if (model.game.saveAs("")) {
            parent->showMessage(model.game.getDefaultFileName());
        } else {
            parent->showMessage("Save failed: " + model.game.getDefaultFileName());
        }
    });

    add("archive", 0, 0, "move the daily record to a timestamped file", [this](CommandContext& ctx) {
        // Save any pending changes to the daily file
        std::string dailyFile = model.game.getDefaultFileName();
        model.game.saveAs("");

        // Check if there's actually a file to archive
        if (!std::filesystem::exists(dailyFile)) {
            parent->showMessage("Nothing to archive");
            ctx.notifyMenu = false;
            return;
        }

        // Rename daily file to timestamped archive name
        std::time_t t = std::time(nullptr);
        std::tm time {};
        time = *std::localtime(&t);
        std::ostringstream ss;
        auto dailyPath = std::filesystem::path(dailyFile);
        ss << dailyPath.parent_path().string() << "/"
           << std::put_time(&time, "%Y-%m-%dT%H-%M-%S") << ".sgf";
        std::string archivedFile = ss.str();

        try {
            std::filesystem::rename(dailyFile, archivedFile);
            spdlog::info("Renamed {} to {}", dailyFile, archivedFile);
        } catch (const std::exception& e) {
            spdlog::error("Failed to rename session file: {}", e.what());
            parent->showMessage("Archive failed");
            ctx.notifyMenu = false;
            return;
        }

        // Clear session doc so new session starts fresh
        model.game.clearSession();
        // defaultFileName stays as daily pattern (e.g., games/2026-02-03.sgf)
        // No need to change it - next save will create fresh daily file

        // Show feedback with archived filename
        parent->showMessage(archivedFile);
        spdlog::info("Archived to {}, daily session continues as {}", archivedFile, dailyFile);
    });

    // Shared body of load_sgf/load_tsumego, mirroring what the file chooser does
    // on "open_file": set the mode on the UI thread first, then defer the load
    // past any genmove in flight. In tsumego mode the cursor stays at the root
    // (the puzzle is to be solved, not replayed) and the opening move is
    // auto-played when it contradicts PL.
    auto loadGame = [this](CommandContext& ctx, bool tsumego) {
        int gameIndex = 0;
        if (ctx.args.size() > 1 && !parseInt(ctx.args[1], gameIndex)) {
            spdlog::warn("load_sgf: '{}' is not a game index", ctx.args[1]);
            return;
        }
        const std::string path = ctx.args[0];
        parent->setTsumegoMode(tsumego);

        std::string busyEngine;
        const bool ran = engine.runWhenEngineFree([this, path, gameIndex, tsumego]() {
            if (!engine.loadSGF(path, gameIndex, tsumego)) {
                spdlog::warn("load_sgf: failed to load '{}'", path);
                return;
            }
            if (tsumego) {
                engine.autoPlayTsumegoSetup();
            }
        }, &busyEngine);

        if (ran) {
            finishGameReplacement();
        } else {
            parent->showMessage("Waiting for " + busyEngine + "...");
        }
    };

    add("load_sgf", 1, 2, "<path> [gameIndex] — load an SGF without the file chooser",
        [loadGame](CommandContext& ctx) {
        // Same entry point the file chooser uses, exposed for scripted runs and
        // for the prologue of recorded bug reports.
        loadGame(ctx, false);
    });

    add("load_tsumego", 1, 2, "<path> [gameIndex] — load an SGF as a tsumego problem",
        [loadGame](CommandContext& ctx) {
        // The file chooser's tsumego toggle, which was the *only* way into the
        // mode — so bad-move paths, Solved/Wrong feedback and the blocked click
        // on a solved position had no reachable test at all.
        loadGame(ctx, true);
    });

    add("load", 0, 0, "open the SGF file chooser", [this](CommandContext&) {
        // Get the file chooser handler and show the dialog
        if (auto* handler = dynamic_cast<EventHandlerFileChooser*>(EventManager::GetEventHandler("open"))) {
            // Pre-select current file and game in dialog
            std::string currentFile = model.game.hasLoadedExternalDoc()
                ? model.game.getLoadedFilePath()
                : model.game.getDefaultFileName();
            int currentGameIdx = model.game.getLoadedGameIndex();
            handler->ShowDialog(currentFile, currentGameIdx);
        } else {
            spdlog::warn("File chooser handler not found");
        }
    });

    add("prev_game", 0, 0, "previous game in the loaded SGF collection",
        [stepLoadedGame](CommandContext& ctx) {
            stepLoadedGame(ctx, -1);
    });

    add("next_game", 0, 0, "next game in the loaded SGF collection",
        [stepLoadedGame](CommandContext& ctx) {
            stepLoadedGame(ctx, +1);
    });

    add("msg", 0, 0, "dismiss the status message", [this](CommandContext&) {
        // Only clear if no active prompt (prompts require button click)
        if (!parent->hasActivePrompt()) {
            parent->clearMessage();
        }
    });

    add("prompt_yes", 0, 0, "confirm the active prompt", [this](CommandContext&) {
        parent->handlePromptResponse(true);
    });

    add("prompt_ok", 0, 0, "confirm the active prompt", [this](CommandContext&) {
        parent->handlePromptResponse(true);
    });

    add("prompt_no", 0, 0, "dismiss the active prompt", [this](CommandContext&) {
        parent->handlePromptResponse(false);
    });

    add("prompt_cancel", 0, 0, "dismiss the active prompt", [this](CommandContext&) {
        parent->handlePromptResponse(false);
    });

    // --- Parameterised commands (scripting channel) --------------------------

    add("new_game", 1, 1, "<size> — start a new game on a size x size board",
        [this](CommandContext& ctx) {
            int boardSize = 0;
            if (!parseInt(ctx.args[0], boardSize) || boardSize < 2 || boardSize > 25) {
                spdlog::warn("new_game: '{}' is not a valid board size (2..25)", ctx.args[0]);
                return;
            }
            if (!newGame(static_cast<unsigned>(boardSize))) {
                spdlog::warn("new_game: failed to start a {}x{} game", boardSize, boardSize);
            }
    });

    add("switch_player", 2, 2, "<black|white> <name-or-index> — assign a player to one colour",
        [this](CommandContext& ctx) {
            const std::string colour = toLower(ctx.args[0]);
            int which = -1;
            if (colour == "black" || colour == "b" || colour == "0") {
                which = 0;
            } else if (colour == "white" || colour == "w" || colour == "1") {
                which = 1;
            } else {
                spdlog::warn("switch_player: '{}' is not a colour (expected black or white)",
                    ctx.args[0]);
                return;
            }

            const auto players = engine.getPlayers();
            int index = -1;
            if (parseInt(ctx.args[1], index)) {
                if (index < 0 || index >= static_cast<int>(players.size())) {
                    spdlog::warn("switch_player: player index {} out of range (0..{})",
                        index, players.empty() ? 0 : players.size() - 1);
                    return;
                }
            } else {
                // Resolve by name: exact match wins, otherwise a unique substring match
                const std::string wanted = toLower(ctx.args[1]);
                int partial = -1;
                int partialCount = 0;
                for (size_t i = 0; i < players.size(); ++i) {
                    const std::string name = toLower(players[i]->getName());
                    if (name == wanted) {
                        index = static_cast<int>(i);
                        break;
                    }
                    if (name.find(wanted) != std::string::npos) {
                        partial = static_cast<int>(i);
                        ++partialCount;
                    }
                }
                if (index < 0 && partialCount == 1) {
                    index = partial;
                }
                if (index < 0) {
                    std::ostringstream known;
                    for (size_t i = 0; i < players.size(); ++i) {
                        known << (i ? ", " : "") << i << ':' << players[i]->getName();
                    }
                    spdlog::warn("switch_player: no unique player matching '{}' (known: {})",
                        ctx.args[1], known.str());
                    return;
                }
            }

            switchPlayer(which, index);
            parent->refreshPlayerDropdowns();  // Keep the dropdowns in step with the switch
            view.requestRepaint();
    });

    add("board_size", 1, 1, "<size> — as the board-size dropdown, confirmation included",
        [this](CommandContext& ctx) {
            int boardSize = 0;
            if (!parseInt(ctx.args[0], boardSize)) {
                spdlog::warn("board_size: '{}' is not a number", ctx.args[0]);
                return;
            }
            // Unlike new_game, this is the dropdown's path: it asks before
            // replacing a game worth keeping. new_game stays unprompted because
            // every scenario opens with it.
            requestNewGame(static_cast<unsigned>(boardSize), {});
    });

    add("handicap", 1, 1, "<stones> — as the handicap dropdown, confirmation included",
        [this](CommandContext& ctx) {
            int handicap = 0;
            if (!parseInt(ctx.args[0], handicap)) {
                spdlog::warn("handicap: '{}' is not a number", ctx.args[0]);
                return;
            }
            requestHandicap(handicap, {});
    });

    add("komi", 1, 1, "<points> — as the komi dropdown, refused once play has begun",
        [this](CommandContext& ctx) {
            float komi = 0.0f;
            if (!parseFloat(ctx.args[0], komi)) {
                spdlog::warn("komi: '{}' is not a number", ctx.args[0]);
                return;
            }
            // Unlike board_size and handicap this never replaces the game, so
            // there is nothing to confirm — it is simply refused outside Setup
            // and Paused. The dropdown reverts its selection on the same answer.
            if (!setKomi(komi)) {
                parent->showMessage("Komi can only be changed before play starts");
            }
    });

    add("click", 2, 2, "<col> <row> — place a stone as if the board was clicked there",
        [this](CommandContext& ctx) {
            int col = 0;
            int row = 0;
            if (!parseInt(ctx.args[0], col) || !parseInt(ctx.args[1], row)) {
                spdlog::warn("click: '{} {}' is not a board coordinate pair",
                    ctx.args[0], ctx.args[1]);
                return;
            }
            const Position coord(col, row);
            if (!model.isPointOnBoard(coord)) {
                spdlog::warn("click: ({},{}) is off a {}x{} board",
                    col, row, model.getBoardSize(), model.getBoardSize());
                return;
            }
            // Same guard as mouseClick: no stone placement until players are set
            if (!acceptsUiEvents()) {
                spdlog::warn("click: ignored, initialization is not complete");
                return;
            }
            boardClick(coord);
    });
}

void GobanControl::listCommands() const {
    spdlog::info("{} commands registered:", registry.size());
    for (const auto& entry : registry) {
        spdlog::info("  {:<26} {}", entry.first, entry.second.help);
    }
}

void GobanControl::command(const std::string& cmd) {
    if (registry.empty()) {
        buildRegistry();
    }
    // Legacy command names contain spaces ("play once", "reset camera"), so an
    // exact match on the whole line wins before falling back to tokenisation.
    if (registry.find(cmd) != registry.end()) {
        command(cmd, std::vector<std::string>());
        return;
    }
    const auto tokens = splitCommand(cmd);
    if (tokens.empty()) {
        return;  // blank line — nothing to dispatch
    }
    command(tokens.front(), std::vector<std::string>(tokens.begin() + 1, tokens.end()));
}

void GobanControl::command(const std::string& name, const std::vector<std::string>& args) {
    if (registry.empty()) {
        buildRegistry();
    }
    const auto entry = registry.find(name);
    if (entry == registry.end()) {
        spdlog::warn("Unknown command '{}' — run 'help' to list the known commands", name);
        return;
    }
    const int argc = static_cast<int>(args.size());
    if (argc < entry->second.minArgs || argc > entry->second.maxArgs) {
        spdlog::warn("Command '{}' takes {}..{} argument(s), got {} — usage: {} {}",
            name, entry->second.minArgs, entry->second.maxArgs, argc, name, entry->second.help);
        return;
    }
    // Record before executing, then sample the resulting state, so a saved
    // report shows what each action produced.
    ScenarioRecorder::instance().recordAction(name, args, dumpState());

    CommandContext ctx{args, false, true};
    {
        ScenarioRecorder::SuppressNested noNestedRecords;
        entry->second.handler(ctx);
    }

    ScenarioRecorder::instance().recordState(dumpState());
    // Feed the toggle state back into the menu, unless the handler bailed out
    // (the old if/else chain reached this line only when no `return` was taken).
    if (ctx.notifyMenu) {
        parent->OnMenuToggle(name, ctx.checked);
    }
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
        // These four keys dispatch through the registry rather than calling the
        // navigator directly. Two things follow from that and neither is
        // incidental: they answer to the same availableActions() rule as the
        // toolbar buttons, and they are recorded, so a keyboard-driven review
        // survives into a bug report. Navigating by key used to be invisible to
        // ScenarioRecorder, which silently dropped it from every replay.
        //
        // Space/Right: navigate forward (or trigger kibitz at end of unfinished branch)
        if (key == Rml::Input::KI_SPACE || key == Rml::Input::KI_RIGHT) {
            if (model.game.hasNextMove()) {
                command("navigate_forward");
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
                if (model.game.isOnBadMovePath() && actions().kibitz) {
                    engine.requestKibitzNav();
                }
                return;  // Don't fall through to "play once" — would break navigation
            }
            spdlog::debug("Navigation: at end of branch, Space falls through to kibitz");
        }
        if (key == Rml::Input::KI_LEFT || key == Rml::Input::KI_BACK) {
            command("navigate_back");
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
    // Setup or Paused only — see GobanModel::onKomiChange() for why a finished
    // game is excluded, and keep the two guards in step.
    const GamePhase phase = model.phase();
    if (phase == GamePhase::Setup || phase == GamePhase::Paused) {
        engine.setKomi(komi);
        model.state.komi = komi;
        model.game.updateKomi(komi);  // Keep game record in sync
        UserSettings::instance().setKomi(komi);
        return true;
    }
    return false;
}

bool GobanControl::setHandicap(int handicap) const {
    spdlog::debug("setHandicap: handicap={} phase={}", handicap, phaseName(model.phase()));
    // No guard here on purpose. This is the "do it" half, holding no policy —
    // the same split as newGame() and requestNewGame(). Its only caller is
    // requestHandicap(), which has already asked the user whether the current
    // game may be replaced; a second condition here could only contradict that
    // answer, which is exactly what it did: the phase test refused precisely
    // when hasGameWorthKeeping() had decided the question was worth asking, so
    // confirming the prompt did nothing at all.
    model.state.handicap = handicap;
    const bool success = newGame(model.getBoardSize());
    view.requestRepaint(GobanView::UPDATE_STONES | GobanView::UPDATE_OVERLAY);
    return success;
}

void GobanControl::switchPlayer(int which, int newPlayerIndex) const {
    // Dropdown-driven switches never reach command(), so record here too.
    auto allPlayers = engine.getPlayers();
    const std::string chosen =
        (newPlayerIndex >= 0 && static_cast<size_t>(newPlayerIndex) < allPlayers.size())
            ? allPlayers[static_cast<size_t>(newPlayerIndex)]->getName()
            : std::to_string(newPlayerIndex);
    ScenarioRecorder::instance().recordAction(
        "switch_player", {which == 0 ? "black" : "white", chosen}, dumpState());

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

namespace {

const char* messageName(GameState::Message m) {
    switch (m) {
        case GameState::NONE:              return "none";
        case GameState::WHITE_PASS:        return "white_pass";
        case GameState::BLACK_PASS:        return "black_pass";
        case GameState::WHITE_RESIGNS:     return "white_resigns";
        case GameState::BLACK_RESIGNS:     return "black_resigns";
        case GameState::BLACK_RESIGNED:    return "black_resigned";
        case GameState::WHITE_RESIGNED:    return "white_resigned";
        case GameState::WHITE_WON:         return "white_won";
        case GameState::BLACK_WON:         return "black_won";
        case GameState::PAUSED:            return "paused";
        case GameState::CALCULATING_SCORE: return "calculating_score";
        case GameState::SCORING_FAILED:    return "scoring_failed";
        case GameState::TSUMEGO_SOLVED:    return "tsumego_solved";
        case GameState::TSUMEGO_WRONG:     return "tsumego_wrong";
    }
    return "unknown";
}

const char* loopStateName(LoopState s) {
    switch (s) {
        case LoopState::Stopped:  return "stopped";
        case LoopState::Running:  return "running";
        case LoopState::Stopping: return "stopping";
    }
    return "unknown";
}

const char* engineSyncName(EngineSync s) {
    switch (s) {
        case EngineSync::Unsynced: return "unsynced";
        case EngineSync::Syncing:  return "syncing";
        case EngineSync::Synced:   return "synced";
    }
    return "unknown";
}

}  // namespace

void GobanControl::confirmGameReplacement(std::function<void()> replace,
                                          std::function<void()> onCancelled) const {
    if (!model.hasGameWorthKeeping()) {
        replace();
        return;
    }
    parent->showPromptYesNoTemplate("templateStartNewGame",
        [replace = std::move(replace), onCancelled = std::move(onCancelled)](bool confirmed) {
            if (confirmed) {
                replace();
            } else if (onCancelled) {
                onCancelled();
            }
        });
}

void GobanControl::requestNewGame(unsigned boardSize, std::function<void(bool)> onSettled) const {
    confirmGameReplacement(
        [this, boardSize, onSettled]() {
            // Board size is about to change, so the game on screen is going
            // regardless — save it first, as `clear` does.
            saveCurrentGame();
            const bool ok = newGame(boardSize);
            if (onSettled) onSettled(ok);
        },
        [onSettled]() { if (onSettled) onSettled(false); });
}

void GobanControl::requestHandicap(int handicap, std::function<void(bool)> onSettled) const {
    confirmGameReplacement(
        [this, handicap, onSettled]() {
            saveCurrentGame();
            const bool ok = setHandicap(handicap);
            if (onSettled) onSettled(ok);
        },
        [onSettled]() { if (onSettled) onSettled(false); });
}

UiInputs GobanControl::uiInputs() const {
    UiInputs in;
    in.phase             = model.phase();
    in.uiReady           = acceptsUiEvents();
    in.engineThinking    = engine.isThinking();
    in.humanToMove       = engine.humanToMove();
    in.engineToMove      = engine.isCurrentPlayerEngine();
    // Bot-bot detection: the explicit toggle, or simply both sides being
    // engines. Analysis mode unlocks it, since that is the mode for stepping in.
    in.aiVsAiLocked      = (engine.isAiVsAi() || engine.areBothPlayersEngines())
                           && engine.getGameMode() != GameMode::ANALYSIS;
    in.tsumego           = model.tsumegoMode;
    // Everything about the record comes from the published snapshot, never from
    // the record itself: uiInputs() runs every frame on the UI thread, and
    // GameRecord's accessors walk an SGF tree the game thread owns. See
    // GameSnapshot and ADR-0006.
    const auto snap      = model.snapshot();
    in.atEndOfNavigation = snap->atEnd;
    // The predicate toggle_territory used to guard itself with, lifted into the
    // policy so the button and the command cannot answer differently.
    in.scoredEnd         = snap->scoredEnd;
    in.hasMoves          = snap->moveCount > 0
                           || !model.setupBlackStones.empty()
                           || !model.setupWhiteStones.empty()
                           || snap->mainLineMoves > 0;
    in.hasUnsavedChanges = model.game.hasUnsavedChanges();
    return in;
}

UiActions GobanControl::actions() const {
    return availableActions(uiInputs());
}

bool GobanControl::canResign() const {
    // Deliberately not a second copy of the rules — see UiActions.cpp for what
    // resignation requires and why. The `resign` command and the cmdResign
    // button now read the same expression.
    return actions().resign;
}

bool GobanControl::isIdle() const {
    if (!acceptsUiEvents()) return false;
    if (engine.isThinking()) return false;
    // Navigation is queued to the game thread, so it can still be outstanding
    // while no engine is thinking. Without this a script would read the board
    // before navigation had applied.
    if (engine.hasPendingNavigation()) return false;
    // Likewise for a game-replacing action waiting on, or running past, a
    // genmove: the board is not final until it has finished.
    if (engine.hasDeferredTask()) return false;
    // And while the game thread is replaying the record into the engines. Only
    // EngineSync::Syncing counts: Unsynced persists with the loop stopped after
    // a new game, so waiting on that would never return.
    if (engine.isSyncingEngines()) return false;
    return true;
}

nlohmann::json GobanControl::dumpState() const {
    nlohmann::json s;

    // Game record / navigation, all from the published snapshot — dumpState()
    // runs on the UI thread, and on every recorded command. See GameSnapshot.
    const auto snap     = model.snapshot();
    s["move_count"]     = snap->moveCount;
    s["view_position"]  = snap->viewPosition;
    s["main_line_moves"] = snap->mainLineMoves;
    s["navigating"]     = snap->navigating;
    s["at_end"]         = snap->atEnd;
    s["variations"]     = snap->variations;
    s["has_result"]     = snap->hasResult;
    // The recorded result itself, read back from the SGF RE property — not
    // model.state.msg, which is transient. Lets a scenario prove that an action
    // did *not* rewrite the outcome.
    s["result"]         = messageName(snap->resultMessage);
    s["board_size"]     = snap->boardSize;

    // Turn and rules state
    s["color_to_move"]  = (model.state.colorToMove == Color::BLACK) ? "B" : "W";
    s["komi"]           = model.state.komi;
    s["handicap"]       = model.state.handicap;

    // Stones actually on the board, and prisoners
    s["black_stones"]   = model.board.stonesOnBoard(Color::BLACK);
    s["white_stones"]   = model.board.stonesOnBoard(Color::WHITE);
    s["captured_black"] = model.board.capturedCount(Color::BLACK);
    s["captured_white"] = model.board.capturedCount(Color::WHITE);

    // Lifecycle flags — the ones the Design Invariants are written about
    s["mode"]           = (engine.getGameMode() == GameMode::ANALYSIS) ? "analysis" : "match";
    s["ai_vs_ai"]       = engine.isAiVsAi();
    s["phase"]          = phaseName(model.phase());
    s["running"]        = engine.isRunning();
    s["thinking"]       = engine.isThinking();
    // Same meaning as before the flag was split, so scenarios still read.
    s["syncing_ui"]     = !acceptsUiEvents();
    s["pending_nav"]    = engine.hasPendingNavigation();
    s["queued_nav"]     = engine.hasQueuedNavigation();
    s["deferred_task"]  = engine.hasDeferredTask();
    s["engine_sync"]    = engineSyncName(engine.engineSyncState());
    s["loop_state"]     = loopStateName(engine.loopState());
    // Lets a scenario tell "a prompt appeared" from "the command silently did
    // nothing" — the distinction the dead handicap prompt turned on.
    s["prompt_active"]  = parent->hasActivePrompt();
    // A screenshot taken while this is true is not reproducible: the camera is
    // between positions and the shader is fed a live clock while animating.
    s["camera_animating"] = view.animationRunning || view.cameraAnim.active;
    s["tsumego"]        = model.tsumegoMode.load();
    s["holds_stone"]    = model.state.holdsStone;
    s["show_territory"] = model.board.showTerritory;
    s["unsaved_changes"] = model.game.hasUnsavedChanges();
    s["msg"]            = messageName(model.state.msg);

    // What the toolbar is offering right now. These are the exact booleans
    // ElementGame::syncActionAvailability() greys the buttons by, and — since
    // ADR-0005 — the exact booleans every command guards itself with. Exposing
    // them lets a scenario assert the *policy* rather than only its
    // consequences: "Undo is greyed here" used to be checkable solely by
    // pressing it and observing that nothing moved, which cannot tell a refusal
    // apart from an action that legitimately had nothing to do.
    const UiActions a = actions();
    s["can_start"]     = a.start;
    s["can_pass"]      = a.pass;
    s["can_resign"]    = a.resign;
    s["can_undo"]      = a.undo;
    s["can_kibitz"]    = a.kibitz;
    s["can_navigate"]  = a.navigate;
    s["can_territory"] = a.territory;
    s["can_clear"]     = a.clear;
    s["can_save"]      = a.save;

    // Where the position came from — needed to reconstruct a starting point
    // when replaying a recorded session.
    s["sgf_file"] = snap->sgfFile;
    s["game_index"] = snap->gameIndex;

    // Players
    auto players = engine.getPlayers();
    const size_t blackIdx = engine.getActivePlayer(0);
    const size_t whiteIdx = engine.getActivePlayer(1);
    s["black_player"] = (blackIdx < players.size()) ? players[blackIdx]->getName() : "";
    s["white_player"] = (whiteIdx < players.size()) ? players[whiteIdx]->getName() : "";

    // A cheap, order-independent fingerprint of the position, so a scenario can
    // assert "the board is the same as before" without spelling out 361 points.
    unsigned long hash = 1469598103934665603UL;
    for (int row = 0; row < snap->boardSize; ++row) {
        for (int col = 0; col < snap->boardSize; ++col) {
            const Color& stone = model.board[Position(col, row)].stone;
            const unsigned long v = (stone == Color::BLACK) ? 2u
                                  : (stone == Color::WHITE) ? 1u : 0u;
            hash = (hash ^ v) * 1099511628211UL;
        }
    }
    s["board_hash"] = hash;

    return s;
}

void GobanControl::saveCurrentGame() const {
    auto& settings = UserSettings::instance();

    bool hasContent = model.game.moveCount() > 0 || model.game.getLoadedMovesCount() > 0;
    if (hasContent) {
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
    if (!sessionFile.empty() && (hasContent || isExternal)) {
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

void GobanControl::requestScreenshot(const std::string& path) {
    view.requestScreenshot(path);
}

bool GobanControl::screenshotPending() const {
    return view.screenshotPending();
}
