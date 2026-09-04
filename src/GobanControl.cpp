#include "ElementGame.h"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include "AppState.h"
#include "GobanControl.h"
#include "MessageLog.h"
#include "EventHandlerFileChooser.h"
#include "EventManager.h"
#include "UserSettings.h"
#include "ScenarioRecorder.h"
#include <algorithm>
#include <cctype>
#include <cmath>
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
        parent->showMessage(Rml::CreateString(
            parent->templateText("tplWaitingFor", "Waiting for %s...").c_str(),
            busyEngine.c_str()).c_str());
    }
    // Accepted either way; a deferred failure is only logged, since the caller
    // (e.g. the board-size dropdown) has already moved on.
    return true;
}

bool GobanControl::newGameNow(unsigned boardSize) const {
    // Engine and model work only. This may run on the game thread (when it was
    // deferred past a genmove), and RmlUi is not thread safe — every widget and
    // view update happens later in finishGameReplacement(), on the UI thread.
    // A no-op when this was deferred and so runs on the game thread — which is
    // the whole point: the loop is between moves there, and stopping it is
    // neither possible nor needed. See ADR-0001.
    engine.interrupt();
    engine.removeSgfPlayers();  // Remove temporary SGF players from previous load
    model.tsumegoMode = false;
    model.game.setSuppressSessionCopy(false);
    if(!engine.clearGame(boardSize, model.state.komi, model.state.handicap)) {
        return false;
    }
    model.createNewRecord();
    // Only now, with the empty record installed: the sync replays whatever the
    // model holds, and starting it inside clearGame() above raced it against
    // this line — the engines could be handed the whole of the game just
    // discarded, and the next move came back "illegal move" on a board that
    // looked empty. See GameThread::startSyncingNewGame().
    engine.startSyncingNewGame();

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
    //
    // The resize first. onBoardSized() only hands the new size over now, and the
    // overlay rebuild below would otherwise be wiped by the next Update() — this
    // runs before context->Update() in the frame.
    view.applyPendingResize();
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

// A move made away from the end of the line — by a click on a fresh point, by a
// click on a point already explored, or by a pass. All three are one act and
// now ask one function what it means, because the tsumego half of the answer
// had been written at one of the three call sites only.
//
// A puzzle is not a game: the answer sits in the main line and defines what
// "correct" is, so nothing the solver tries may promote itself over it, and
// trying does not start a match. `pass` promoted and started (main line 2 → 1
// moves, phase Paused → Playing), and so did a *second* click on a wrong move
// already explored — while the first click on that same point, the one path
// carrying `promote=false`, was right. The rule lives here now, once.
//
// It asks `model.tsumegoMode` rather than the view's copy, though the call
// sites around it ask the view: `promote` has to agree with the flag the *game
// thread* consults when it decides whether to mark the move BM, and that is
// this one. The two are set together but not at the same instant — the deferred
// switchGame() path sets the model's on the game thread and the view's later,
// in finishGameReplacement().
void GobanControl::playVariationAt(const Move& m) const {
    if (!model.tsumegoMode) {
        model.start();
        if (!engine.isRunning()) {
            engine.run();
        }
    }
    engine.navigateToVariation(m, !model.tsumegoMode);
}

// Left click on an on-board intersection. Callers must have validated that the
// point is on the board and that initialization is complete.
void GobanControl::boardClick(const Position& coord) {
    // One snapshot for the whole decision. Reading the SGF tree here would race
    // the game thread exactly as uiInputs() did — a click landing while the
    // engine plays its move is the case, and it is not rare — and taking the
    // fields one at a time would let the position shift underneath the branches
    // below. See ADR-0006 stage 2.
    const auto snap = model.snapshot();

    // One policy question for the whole path (ADR-0005). A click is a move at
    // the cursor and `pass` is the same act, so they read the same answer rather
    // than each carrying their own condition — which is how the review branch
    // below came to test `isThinking()` while the branch that starts a game
    // tested nothing at all.
    //
    // Finished is exempt because a click there is not a play: it means "clear",
    // and it is handled further down.
    if (model.phase() != GamePhase::Finished && !actions().play) {
        // ...but a click that cannot place a stone is not necessarily a click
        // that means nothing. On a game waiting for an engine to move it means
        // Start, which is what it meant for years: boardClick() called
        // model.start() + run() unconditionally, so clicking the board with a
        // bot to move began the match.
        //
        // That was lost as collateral, twice and unremarked. 4c5dcfc moved
        // start() behind "a stone is actually being placed" — rightly, because
        // merely *picking a stone out of the bowl* was flipping the phase to
        // Playing with nothing recorded — and the bot-to-move case went with it;
        // the same commit message still cites "a board click already starts the
        // game" as a reason for something else. Then the guard above refused the
        // click outright, since a.play requires the turn to be the human's.
        //
        // Restored by asking availableActions() a *different* question rather
        // than going round it: a.start is `!finished && !playing &&
        // engineToMove`, which is exactly when the Start button lights up, and
        // dispatching the command reuses the one path that owns model.start()
        // and engine.run(). No second lifecycle route.
        if (actions().start) {
            spdlog::debug("board click with an engine to move: starting the game");
            command("start");
            return;
        }
        spdlog::debug("board click refused by availableActions()");
        return;
    }

    // During navigation (not at end), handle clicks specially
    if (snap->navigating && !snap->atEnd) {
        // Stone-in-hand: first click picks up, second click places
        if (!model.state.holdsStone) {
            // First click: pick up stone
            model.state.holdsStone = true;
            model.updateReservoirs();
            view.requestRepaint(GobanView::UPDATE_STONES);
            return;
        }

        // Second click: place stone.

        // A click on one of the marked moves *follows* it. It is the Right
        // arrow with a mouse: the move is already in the tree, so there is
        // nothing to play, nothing to promote, and nothing to ask the rules —
        // the position it leads to was legal when it was recorded.
        //
        // It used to go through playVariationAt(), which starts the match. The
        // record then grew a copy of itself: following the marked move left the
        // phase Playing at a mid-tree cursor, so the game loop asked the engine
        // for the next one, and GameRecord::move() appends whatever it is
        // handed — even a move the node already has as a child. GNU Go, being
        // deterministic, answers with the recorded move itself, so the node
        // ended up with two identical children drawn on top of each other and
        // the main line acquired a letter it never had: `16a`, then `16b`, then
        // `16c` for the same point, one per visit. Reported as "clicking the
        // main variation plays it instead of following it", which is exactly
        // what it was doing.
        //
        // Resuming play from a reviewed position is Start's job — the same
        // division as `play once`, which asks the engine one question without
        // turning the review back into a game.
        for (const auto& move : snap->variationMoves) {
            if (move == Move::NORMAL && move.pos == coord) {
                spdlog::debug("Clicked on existing variation at ({},{}) — following it",
                              coord.col(), coord.row());
                releaseStone();
                engine.navigateToVariation(move, false);
                view.requestRepaint();
                return;
            }
        }

        // No matching variation — new move. In a tsumego the game thread infers
        // the BM marking from the fact that this is a fresh branch.
        spdlog::debug("New variation during navigation (color={})",
            snap->colorToMove == Color::BLACK ? "B" : "W");
        const Move variation(coord, snap->colorToMove);
        if (!placeStone(variation)) return;
        playVariationAt(variation);
        return;
    }

    // In tsumego mode at end of variation
    if (view.isTsumegoMode() && snap->atEnd) {
        if (!snap->onBadMovePath) {
            return;  // Solved — stay blocked
        }
        // Dead branch: allow exploration
        if (!model.state.holdsStone) {
            model.state.holdsStone = true;
            model.updateReservoirs();
            view.requestRepaint(GobanView::UPDATE_STONES);
            return;
        }
        const Move refutation(coord, snap->colorToMove);
        if (!placeStone(refutation)) return;
        playVariationAt(refutation);
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

    // A stone is actually being placed. The rules are asked *before* the game
    // is started: a point the board will not take is not a move, so it must not
    // flip the phase to Playing either — the same trap as merely picking a
    // stone out of the bowl, above.
    const auto move = engine.getLocalMove(coord);
    if (!placeStone(move)) return;

    model.start();
    if (!engine.isRunning()) {
        engine.run();
    }
    spdlog::debug("engine.isRunning() = {}", engine.isRunning());
    engine.playLocalMove(move);
    view.requestRepaint();
}

// The stone leaves the hand: it is on the board now (or on its way to the game
// thread), so the reservoir gains it back.
void GobanControl::releaseStone() const {
    model.state.holdsStone = false;
    model.updateReservoirs();
}

// Ask the rules before anyone else. Returns false — with the stone still in
// hand — when the point will not take it.
//
// The engine used to be the only referee: every click went out as
// `play <colour> <point>` and whatever GNU Go answered decided it. So a
// misclick onto an occupied point came back as `? illegal move`, which the
// message-log sink takes at error level and puts a red badge in front of the
// user for what is, on a real board, nothing at all — you feel the stone not
// go down and try again. One recorded endgame (2026-08-16, `play B M10` onto a
// White stone from ten moves earlier) left exactly that as the whole contents
// of last_run.log.
//
// The renderer had always known better: GobanView::updateCursor() draws the
// held stone only where the point is empty, so the ghost stone silently
// vanishes over a point this used to send anyway. Both now ask
// GobanModel::isLegalMove(), which is the same rule the board itself is
// rebuilt by — so what the user cannot see placed is exactly what a click will
// not place. An engine refusal after this means the engine has drifted out of
// sync with the board, which is worth an error line.
bool GobanControl::placeStone(const Move& m) const {
    if (!model.isLegalMove(m)) {
        spdlog::debug("board click at {} refused by the rules — the stone stays in hand",
                      m.toString());
        return false;
    }
    releaseStone();
    return true;
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
        const auto snap = model.snapshot();
        size_t gameCount = snap->loadedGameCount;
        if (gameCount <= 1) { ctx.notifyMenu = false; return; }

        int currentIdx = snap->gameIndex;
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
            parent->showMessage(Rml::CreateString(
            parent->templateText("tplWaitingFor", "Waiting for %s...").c_str(),
            busyEngine.c_str()).c_str());
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
        if (model.snapshot()->moveCount > 0 && model.phase() != GamePhase::Finished) {
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
        // Away from the end of the line a kibitz is a *variation*, exactly as a
        // board click and a pass are there — docs/game-modes.md use case 2 is
        // this workflow, and it says the engine simply will not auto-respond at
        // a historical position, which is precisely what asking once is for.
        //
        // It goes through the navigation queue rather than the game loop.
        // `playKibitzMove()` hands the move to whoever is blocked in genmove and
        // otherwise leaves it in `queuedMove` without waking anything, so while
        // reviewing — where nobody is in genmove — it did nothing at all, and
        // the stale request fired later against a position nobody was looking
        // at. KIBITZ_NAV asks the engine for the *cursor's* colour and applies
        // the answer where the cursor is.
        //
        // And no model.start() on this path: reviewing must not flip the game
        // to Playing. That is what left the loop running at a mid-tree cursor.
        //
        // A tsumego takes that path from *anywhere*, end of the line included.
        // The branch below reaches the engine only through playKibitzMove(),
        // which hands the move to a player already blocked in genmove() and
        // otherwise leaves it in `queuedMove` without waking anything — so on a
        // real configuration the menu item asked no engine at all: no `kibitz:
        // asking` line, no move, and the puzzle silently switched to Playing by
        // the start() below. It only appeared to work against the mock, whose
        // timing let the loop collect the queued move. Reported as "nothing
        // happens at Space, Ctrl+Space or Nápověda", and two of those three
        // were true.
        if (!model.snapshot()->atEnd || model.tsumegoMode) {
            engine.requestKibitzNav();
            view.requestRepaint();
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
                parent->showMessage(parent->templateText("tplAnalysisAnswersEveryMove",
                    "Analysis mode answers every move — use Kibitz in match mode instead"));
            }
        } else {
            if (engine.setGameMode(GameMode::MATCH)) {
                ctx.checked = false;
            }
        }
        view.requestRepaint();
    });

    add("toggle_evaluation", 0, 1, "[on|off] — show or hide the live evaluation overlay",
        [this](CommandContext& ctx) {
        auto& analysis = parent->getAnalysis();
        if (!actions().evaluation) {
            // No engine can answer, or this is a tsumego. Leave the menu alone
            // rather than lighting up a toggle that does nothing.
            spdlog::debug("toggle_evaluation refused by availableActions()");
            ctx.notifyMenu = false;
            return;
        }
        // The explicit form exists for the same reason toggle_log's does: a bare
        // toggle asserts nothing about where it started, so a scenario that
        // toggles twice by mistake reads as passing while testing the opposite.
        bool next = !analysis.isEnabled();
        if (!ctx.args.empty()) {
            const std::string arg = toLower(ctx.args[0]);
            if (arg != "on" && arg != "off" && arg != "true" && arg != "false"
                && arg != "1" && arg != "0") {
                spdlog::warn("toggle_evaluation: expected on or off, got '{}'", ctx.args[0]);
                ctx.notifyMenu = false;
                return;
            }
            next = (arg == "on" || arg == "true" || arg == "1");
        }
        analysis.setEnabled(next);
        UserSettings::instance().setEvaluationEnabled(next);
        ctx.checked = next;
        view.requestRepaint();
    });

    add("toggle_evaluation_moves", 0, 1,
        "[on|off] — show or hide the engine's suggested moves on the board",
        [this](CommandContext& ctx) {
        // Same availability as the panel: an analysis engine exists and this is
        // not a tsumego. Deliberately not also gated on the panel being *on* —
        // an inert toggle you can pre-set is less confusing than one greyed for
        // a reason the menu cannot explain.
        if (!actions().evaluation) {
            spdlog::debug("toggle_evaluation_moves refused by availableActions()");
            ctx.notifyMenu = false;
            return;
        }
        bool next = !view.isAnalysisOverlayShown();
        if (!ctx.args.empty()) {
            const std::string arg = toLower(ctx.args[0]);
            if (arg != "on" && arg != "off" && arg != "true" && arg != "false"
                && arg != "1" && arg != "0") {
                spdlog::warn("toggle_evaluation_moves: expected on or off, got '{}'",
                             ctx.args[0]);
                ctx.notifyMenu = false;
                return;
            }
            next = (arg == "on" || arg == "true" || arg == "1");
        }
        view.setAnalysisOverlay(next);
        UserSettings::instance().setEvaluationMoves(next);
        ctx.checked = next;
    });

    add("toggle_evaluation_readout", 0, 1,
        "[on|off] — show or hide the win rate and score estimate on the board's edge",
        [this](CommandContext& ctx) {
        // Same availability as the suggestions: an analysis engine exists and
        // this is not a tsumego.
        if (!actions().evaluation) {
            spdlog::debug("toggle_evaluation_readout refused by availableActions()");
            ctx.notifyMenu = false;
            return;
        }
        bool next = !view.isEvaluationReadoutShown();
        if (!ctx.args.empty()) {
            const std::string arg = toLower(ctx.args[0]);
            if (arg != "on" && arg != "off" && arg != "true" && arg != "false"
                && arg != "1" && arg != "0") {
                spdlog::warn("toggle_evaluation_readout: expected on or off, got '{}'",
                             ctx.args[0]);
                ctx.notifyMenu = false;
                return;
            }
            next = (arg == "on" || arg == "true" || arg == "1");
        }
        view.setEvaluationReadout(next);
        UserSettings::instance().setEvaluationReadout(next);
        ctx.checked = next;
    });

    add("toggle_wait_clock", 0, 1,
        "[on|off] — show or hide the elapsed-seconds clock while the program is busy",
        [this](CommandContext& ctx) {
        // Deliberately not gated on actions().evaluation: this reports on the
        // *program*, not on the analysis, and it is the only thing that shows
        // during an engine's turn or a resync when the evaluation is off.
        bool next = !view.isWaitClockShown();
        if (!ctx.args.empty()) {
            const std::string arg = toLower(ctx.args[0]);
            if (arg != "on" && arg != "off" && arg != "true" && arg != "false"
                && arg != "1" && arg != "0") {
                spdlog::warn("toggle_wait_clock: expected on or off, got '{}'",
                             ctx.args[0]);
                ctx.notifyMenu = false;
                return;
            }
            next = (arg == "on" || arg == "true" || arg == "1");
        }
        view.setWaitClock(next);
        UserSettings::instance().setWaitClock(next);
        ctx.checked = next;
    });

    add("evaluation_align", 0, 1,
        "[left|center|right] — where the board readout sits along the edge; "
        "with no argument, cycles through them",
        [this](CommandContext& ctx) {
        TextAlign next;
        if (ctx.args.empty()) {
            // Cycling is for judging it by eye, which is the only way this
            // choice can be made.
            switch (view.evaluationAlign()) {
                case TextAlign::Center: next = TextAlign::Left;   break;
                case TextAlign::Left:   next = TextAlign::Right;  break;
                case TextAlign::Right:  next = TextAlign::Center; break;
            }
        } else {
            const auto parsed = GobanView::parseAlign(toLower(ctx.args[0]));
            if (!parsed) {
                spdlog::warn("evaluation_align: expected left, center or right, got '{}'",
                             ctx.args[0]);
                return;
            }
            next = *parsed;
        }
        view.setEvaluationAlign(next);
        UserSettings::instance().setEvaluationAlign(GobanView::alignName(next));
    });

    add("anaglyph", 0, 1,
        "[gray|half-color|color|dubois] — how a stereo shader combines the two "
        "eyes; with no argument, cycles through them",
        [this](CommandContext& ctx) {
        Stereo::Anaglyph next;
        if (ctx.args.empty()) {
            // Cycling, for the same reason evaluation_align cycles: the only way
            // to choose between these is to look at the board through the
            // glasses, and more colour is not simply better — it buys retinal
            // rivalry with it.
            switch (view.anaglyph()) {
                case Stereo::Anaglyph::Gray:      next = Stereo::Anaglyph::HalfColor; break;
                case Stereo::Anaglyph::HalfColor: next = Stereo::Anaglyph::Dubois;    break;
                case Stereo::Anaglyph::Dubois:    next = Stereo::Anaglyph::Color;     break;
                case Stereo::Anaglyph::Color:     next = Stereo::Anaglyph::Gray;      break;
            }
        } else {
            const auto parsed = Stereo::parseAnaglyph(ctx.args[0]);
            if (!parsed) {
                spdlog::warn("anaglyph: expected gray, half-color, color or dubois, "
                             "got '{}'", ctx.args[0]);
                return;
            }
            next = *parsed;
        }
        view.setAnaglyph(next);
        UserSettings::instance().setAnaglyph(Stereo::anaglyphName(next));
        // Named on screen because cycling is blind otherwise: half-colour and
        // Dubois differ subtly on a board that is mostly wood.
        parent->showMessage(Stereo::anaglyphName(next));
    });

    add("anaglyph_strength", 0, 1,
        "[0..1] — how much of each eye's colour survives against its brightness; "
        "1 is the mode as published, 0 collapses it to gray",
        [this](CommandContext& ctx) {
        if (ctx.args.empty()) {
            parent->showMessage(std::to_string(view.anaglyphStrength()));
            return;
        }
        try {
            view.setAnaglyphStrength(std::stof(ctx.args[0]));
        } catch (const std::exception&) {
            spdlog::warn("anaglyph_strength: '{}' is not a number", ctx.args[0]);
            return;
        }
        UserSettings::instance().setAnaglyphStrength(view.anaglyphStrength());
    });

    add("anaglyph_leak", 0, 3,
        "[r g b] — crosstalk cancelled per channel, for the glasses you actually "
        "have; try raising g first, since red filters leak green worst",
        [this](CommandContext& ctx) {
        const auto& leak = view.anaglyphLeak();
        if (ctx.args.empty()) {
            parent->showMessage(std::to_string(leak.r) + " " + std::to_string(leak.g)
                                + " " + std::to_string(leak.b));
            return;
        }
        // One argument sets green alone. That is not a shortcut for its own sake:
        // green through the red filter is the leak that actually bites, and it is
        // the one a user will be reaching for.
        Stereo::Crosstalk next = leak;
        try {
            if (ctx.args.size() == 1) {
                next.g = std::stof(ctx.args[0]);
            } else {
                next.r = std::stof(ctx.args[0]);
                next.g = std::stof(ctx.args[1]);
                next.b = ctx.args.size() > 2 ? std::stof(ctx.args[2]) : 0.0f;
            }
        } catch (const std::exception&) {
            spdlog::warn("anaglyph_leak: expected numbers, got '{}'", ctx.args[0]);
            return;
        }
        view.setAnaglyphLeak(next);
        const auto& applied = view.anaglyphLeak();
        UserSettings::instance().setAnaglyphLeak(applied.r, applied.g, applied.b);
    });

    add("glasses", 0, 1,
        "[red-cyan|red-blue] — which channels reach which eye; red/blue lenses "
        "block green, so it belongs to the left eye there instead of the right",
        [this](CommandContext& ctx) {
        if (ctx.args.empty()) {
            parent->showMessage(Stereo::glassesName(view.glasses()));
            return;
        }
        const auto parsed = Stereo::parseGlasses(ctx.args[0]);
        if (!parsed) {
            spdlog::warn("glasses: expected red-cyan or red-blue, got '{}'", ctx.args[0]);
            return;
        }
        view.setGlasses(*parsed);
        UserSettings::instance().setGlasses(Stereo::glassesName(*parsed));
        // Said plainly, because Dubois silently stops being Dubois here and a
        // user who chose it deserves to know why the picture changed.
        if (!Stereo::duboisApplies(*parsed) && view.anaglyph() == Stereo::Anaglyph::Dubois) {
            parent->showMessage("red-blue: Dubois needs cyan, using half-color");
        } else {
            parent->showMessage(Stereo::glassesName(*parsed));
        }
    });

    add("pointer", 0, 1,
        "[auto|always|never] — when the board draws its own pointer instead of "
        "the window system's; auto means under a stereo shader, where the native "
        "one cannot be at the right depth",
        [this](CommandContext& ctx) {
        if (ctx.args.empty()) {
            parent->showMessage(pointerModeName(view.pointerMode_()));
            return;
        }
        const auto parsed = parsePointerMode(ctx.args[0]);
        if (!parsed) {
            spdlog::warn("pointer: expected auto, always or never, got '{}'", ctx.args[0]);
            return;
        }
        view.setPointerMode(*parsed);
        UserSettings::instance().setPointerMode(pointerModeName(*parsed));
        parent->showMessage(pointerModeName(*parsed));
    });

    add("mouse_click", 2, 2,
        "<x> <y> — click at a window pixel, as the mouse would. The point is "
        "whatever the ray finds, which is what makes it usable where a scenario "
        "must act on the point it is hovering without knowing which one that is",
        [this](CommandContext& ctx) {
        try {
            mouseClick(0, 1, std::stoi(ctx.args[0]), std::stoi(ctx.args[1]));
        } catch (const std::exception&) {
            spdlog::warn("mouse_click: expected two integers, got '{}' '{}'",
                         ctx.args[0], ctx.args[1]);
        }
    });

    add("mouse_move", 2, 2,
        "<x> <y> — move the pointer to a window pixel, as the mouse would; for "
        "scenarios and the recorder, which otherwise cannot hover anything",
        [this](CommandContext& ctx) {
        try {
            mouseMove(std::stoi(ctx.args[0]), std::stoi(ctx.args[1]));
        } catch (const std::exception&) {
            spdlog::warn("mouse_move: expected two integers, got '{}' '{}'",
                         ctx.args[0], ctx.args[1]);
        }
    });

    add("anaglyph_green", 0, 1,
        "[0..1] — how much green the colour modes use; the dial for lenses that "
        "pass green through both filters, where no clean split exists",
        [this](CommandContext& ctx) {
        if (ctx.args.empty()) {
            parent->showMessage(std::to_string(view.anaglyphGreen()));
            return;
        }
        try {
            view.setAnaglyphGreen(std::stof(ctx.args[0]));
        } catch (const std::exception&) {
            spdlog::warn("anaglyph_green: '{}' is not a number", ctx.args[0]);
            return;
        }
        UserSettings::instance().setAnaglyphGreen(view.anaglyphGreen());
        // Said plainly rather than left to be discovered: in gray the green
        // channel carries neither eye, so this dial cannot move anything, and a
        // user turning it in the mode most likely to be selected would otherwise
        // conclude the setting does not work.
        if (!Stereo::greenCarriesImage(view.anaglyph())) {
            parent->showMessage("gray uses no green — try half-color");
        }
    });

    add("anaglyph_balance", 0, 2,
        "[left right] — per-eye gain, for glasses whose filters pass different "
        "amounts of light; a blue lens is much darker than a red one",
        [this](CommandContext& ctx) {
        const auto& balance = view.anaglyphBalance();
        if (ctx.args.empty()) {
            parent->showMessage(std::to_string(balance.left) + " "
                                + std::to_string(balance.right));
            return;
        }
        // One argument brightens the *right* eye and leaves the left alone,
        // because that is the asymmetry these glasses actually have: it is the
        // darker filter that needs the help, and raising one eye rather than
        // lowering the other keeps the picture off the floor.
        Stereo::EyeBalance next = balance;
        try {
            if (ctx.args.size() == 1) {
                next.left = 1.0f;
                next.right = std::stof(ctx.args[0]);
            } else {
                next.left = std::stof(ctx.args[0]);
                next.right = std::stof(ctx.args[1]);
            }
        } catch (const std::exception&) {
            spdlog::warn("anaglyph_balance: expected numbers, got '{}'", ctx.args[0]);
            return;
        }
        view.setAnaglyphBalance(next);
        const auto& applied = view.anaglyphBalance();
        UserSettings::instance().setAnaglyphBalance(applied.left, applied.right);
    });

    add("evaluation_color", 0, 1,
        "[#rrggbb|#rrggbbaa|reset] — the ink of the board readout; alpha is what "
        "makes it read as part of the wood",
        [this](CommandContext& ctx) {
        if (ctx.args.empty()) {
            // No argument reports the current ink, so it can be read off before
            // being changed.
            parent->showMessage(hexFromColor(view.readoutColor()));
            return;
        }
        const std::string arg = toLower(ctx.args[0]);
        if (arg == "reset" || arg == "default") {
            // Back to whatever the application config ships, by forgetting the
            // choice rather than by writing the current default down.
            UserSettings::instance().setEvaluationColor("");
            parent->showMessage("Evaluation colour reset — restart to reload the "
                                "configured default");
            return;
        }
        const auto parsed = parseHexColor(arg);
        if (!parsed) {
            spdlog::warn("evaluation_color: '{}' is not #rgb, #rrggbb or #rrggbbaa",
                         ctx.args[0]);
            return;
        }
        view.setReadoutColor(*parsed);
        UserSettings::instance().setEvaluationColor(hexFromColor(*parsed));
    });

    add("coordinate_offset", 0, 1,
        "[spacings] — how far the coordinate labels sit into the margin; 0.425 "
        "centres them, past ~0.7 they hang off the wood",
        [this](CommandContext& ctx) {
        if (ctx.args.empty()) {
            parent->showMessage(std::to_string(view.coordinateOffset()));
            return;
        }
        try {
            const float offset = std::stof(ctx.args[0]);
            if (offset > GobanView::MAX_SAFE_COORD_OFFSET) {
                parent->showMessage("Past the edge of the wood — the margin is "
                                    "0.85 spacings");
            }
            view.setCoordinateOffset(offset);
        } catch (const std::exception&) {
            spdlog::warn("coordinate_offset: '{}' is not a number", ctx.args[0]);
        }
    });

    add("coordinate_color", 0, 1,
        "[#rrggbb|#rrggbbaa|reset] — the ink of the board's coordinate labels",
        [this](CommandContext& ctx) {
        if (ctx.args.empty()) {
            parent->showMessage(hexFromColor(view.coordinateColor()));
            return;
        }
        const std::string arg = toLower(ctx.args[0]);
        if (arg == "reset" || arg == "default") {
            UserSettings::instance().setCoordinateColor("");
            parent->showMessage("Coordinate colour reset — restart to reload the "
                                "configured default");
            return;
        }
        const auto parsed = parseHexColor(arg);
        if (!parsed) {
            spdlog::warn("coordinate_color: '{}' is not #rgb, #rrggbb or #rrggbbaa",
                         ctx.args[0]);
            return;
        }
        view.setCoordinateColor(*parsed);
        UserSettings::instance().setCoordinateColor(hexFromColor(*parsed));
    });

    add("toggle_coordinates", 0, 1,
        "[on|off] — column letters and row numbers on the board's margins",
        [this](CommandContext& ctx) {
        // No availability term: unlike the evaluation toggles this needs no
        // engine and works on any board.
        bool next = !view.areCoordinatesShown();
        if (!ctx.args.empty()) {
            const std::string arg = toLower(ctx.args[0]);
            if (arg != "on" && arg != "off" && arg != "true" && arg != "false"
                && arg != "1" && arg != "0") {
                spdlog::warn("toggle_coordinates: expected on or off, got '{}'",
                             ctx.args[0]);
                ctx.notifyMenu = false;
                return;
            }
            next = (arg == "on" || arg == "true" || arg == "1");
        }
        view.setCoordinates(next);
        UserSettings::instance().setCoordinates(next);
        ctx.checked = next;
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
            if (!model.snapshot()->atEnd) {
                parent->showMessage(parent->templateText("tplResignAtEndOnly",
                    "Navigate to the end of the game to resign"));
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
        // During navigation, pass creates a variation (game is paused — no turn
        // restriction). From the snapshot, as boardClick does: the two must
        // agree about what "mid-tree" means, and neither may read the tree.
        // Through the same function too, not merely the same snapshot — the
        // agreement stopped at the snapshot once, and a pass in a tsumego then
        // promoted itself over the solution.
        const auto snap = model.snapshot();
        if (snap->navigating && !snap->atEnd) {
            playVariationAt(Move(Move::PASS, snap->colorToMove));
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
        // `pass` and a board click already start the game themselves — a click
        // by placing a stone, and, when it is an engine's turn, by dispatching
        // this command from boardClick().
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
        view.saveShaderSettings();
    });

    add("decrease gamma", 0, 0, "lower gamma", [this](CommandContext&) {
        spdlog::debug("new gamma = {0}", view.getGamma() + 0.025f);
        view.setGamma(view.getGamma() - 0.025f);
        view.saveShaderSettings();
    });

    // All six of these persist immediately, like `anaglyph`, `pointer` and the
    // evaluation toggles. They used to be written only by `save camera` and
    // re-read by `reset camera`, which meant tuning an anaglyph and then
    // reframing the board threw the tuning away — they are not camera state.
    add("increase eof", 0, 0, "ask for more stereo deviation", [this](CommandContext&) {
        view.setEof(view.getEof() + 0.0025f);
        view.saveShaderSettings();
        spdlog::debug("new eof = {0}", view.getEof());
    });

    add("decrease eof", 0, 0, "ask for less stereo deviation", [this](CommandContext&) {
        view.setEof(view.getEof() - 0.0025f);
        view.saveShaderSettings();
        spdlog::debug("new eof = {0}", view.getEof());
    });

    // The window rests on the near point at 0. Positive pushes the scene further
    // behind the glass; negative brings it forward through the screen plane,
    // which is more vivid and collides with the interface drawn at that plane.
    add("increase dof", 0, 0, "push the stereoscopic window back", [this](CommandContext&) {
        view.setDof(Stereo::clampWindowOffset(view.getDof() + 0.0025f));
        view.saveShaderSettings();
        spdlog::debug("new dof offset = {0}", view.getDof());
    });

    add("decrease dof", 0, 0, "pull the stereoscopic window forward", [this](CommandContext&) {
        view.setDof(Stereo::clampWindowOffset(view.getDof() - 0.0025f));
        view.saveShaderSettings();
        spdlog::debug("new dof offset = {0}", view.getDof());
    });

    add("increase contrast", 0, 0, "raise contrast", [this](CommandContext&) {
        spdlog::debug("new contrast = {0}", view.getContrast() + 0.025f);
        view.setContrast(view.getContrast() + 0.025f);
        view.saveShaderSettings();
    });

    add("decrease contrast", 0, 0, "lower contrast", [this](CommandContext&) {
        spdlog::debug("new contrast = {0}", view.getContrast() - 0.025f);
        view.setContrast(view.getContrast() - 0.025f);
        view.saveShaderSettings();
    });

    add("reset contrast and gamma", 0, 0, "restore default contrast and gamma", [this](CommandContext&) {
        view.resetAdjustments();
    });

    add("free camera toggle", 0, 0, "toggle the horizontal camera lock", [this](CommandContext&) {
        view.cam.setHorizontalLock(!view.cam.lock);
    });

    // The log panel. No availableActions() term: this is not a game action —
    // it changes nothing about the position and must work in every phase,
    // including while engines load, which is exactly when it is needed.
    add("toggle_log", 0, 1, "[on|off] — show or hide the message log",
        [this](CommandContext& ctx) {
        if (ctx.args.empty()) {
            parent->toggleLogPanel();
            return;
        }
        // Explicit form for scripts: a bare toggle asserts nothing about where
        // it started, so a scenario that toggles twice by mistake reads as
        // passing while testing the opposite state.
        const std::string arg = toLower(ctx.args[0]);
        if (arg != "on" && arg != "off" && arg != "true" && arg != "false"
            && arg != "1" && arg != "0") {
            spdlog::warn("toggle_log: expected on or off, got '{}'", ctx.args[0]);
            return;
        }
        parent->setLogPanelOpen(arg == "on" || arg == "true" || arg == "1");
    });

    add("log_clear", 0, 0, "empty the message log", [this](CommandContext&) {
        parent->clearLog();
    });

    add("report_bug", 0, 0, "write the recent interactions to a replayable bug report",
        [this](CommandContext&) {
        // The recorder is always on, so this works retroactively: notice
        // something odd, then save what led up to it.
        const std::string path = ScenarioRecorder::instance().save("reports");
        if (path.empty()) {
            parent->showMessage(parent->templateText("tplNothingRecorded", "Nothing recorded yet"));
        } else {
            parent->showMessage(path);
        }
    });

    add("save", 0, 0, "save the game record now", [this](CommandContext&) {
        // Nothing changed since the last save means nothing to write; the same
        // answer greys the button. Said out loud rather than swallowed, because
        // a Save that appears to do nothing reads as a failure.
        if (!actions().save) {
            parent->showMessage(parent->templateText("tplNothingToSave", "Nothing to save"));
            return;
        }
        // Report what actually happened. This used to show the filename
        // unconditionally, so a write that failed — a missing games folder was
        // enough — was indistinguishable from one that succeeded.
        if (model.game.saveAs("")) {
            parent->showMessage(model.game.getDefaultFileName());
        } else {
            parent->showMessage(Rml::CreateString(
                parent->templateText("tplSaveFailed", "Save failed: %s").c_str(),
                model.game.getDefaultFileName().c_str()).c_str());
        }
    });

    add("archive", 0, 0, "move the daily record to a timestamped file", [this](CommandContext& ctx) {
        // Save any pending changes to the daily file
        std::string dailyFile = model.game.getDefaultFileName();
        model.game.saveAs("");

        // Check if there's actually a file to archive
        if (!std::filesystem::exists(dailyFile)) {
            parent->showMessage(parent->templateText("tplNothingToArchive", "Nothing to archive"));
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
            parent->showMessage(parent->templateText("tplArchiveFailed", "Archive failed"));
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
            parent->showMessage(Rml::CreateString(
            parent->templateText("tplWaitingFor", "Waiting for %s...").c_str(),
            busyEngine.c_str()).c_str());
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

    // --- File chooser (scripting seam) ---------------------------------------
    // The dialog was reachable only by clicking it, so nothing about it could be
    // tested — and it is where a good share of this project's bugs have lived.
    // These drive the real handler, so a scenario takes the same path a user
    // does, tsumego toggle included.
    auto chooser = []() -> EventHandlerFileChooser* {
        auto* handler = dynamic_cast<EventHandlerFileChooser*>(
            EventManager::GetEventHandler("open"));
        if (!handler) spdlog::warn("chooser: file chooser handler not found");
        return handler;
    };

    add("chooser_open", 0, 0, "open the SGF file chooser", [this, chooser](CommandContext&) {
        if (auto* h = chooser()) {
            std::string currentFile = model.game.hasLoadedExternalDoc()
                ? model.game.getLoadedFilePath()
                : model.game.getDefaultFileName();
            h->ShowDialog(currentFile, model.game.getLoadedGameIndex());
        }
    });

    add("chooser_cancel", 0, 0, "close the file chooser without loading",
        [chooser](CommandContext&) {
        if (auto* h = chooser()) h->HideDialog();
    });

    add("chooser_confirm", 0, 0, "load the selected file and game",
        [chooser](CommandContext&) {
        if (auto* h = chooser()) h->OpenSelected();
    });

    add("chooser_path", 1, 1, "<dir> — browse to a directory",
        [chooser](CommandContext& ctx) {
        if (auto* h = chooser()) h->SetPath(ctx.args[0]);
    });

    add("chooser_up", 0, 0, "browse to the parent directory", [chooser](CommandContext&) {
        if (auto* h = chooser()) h->NavigateUp();
    });

    add("chooser_select_file", 1, 1, "<name> — select a file by name",
        [chooser](CommandContext& ctx) {
        // By name, not index: a scenario cannot know the order the filesystem
        // produced, and pinning one would make the test depend on it.
        if (auto* h = chooser()) h->SelectFileByName(ctx.args[0]);
    });

    add("chooser_select_game", 1, 1, "<index> — select a game within the file",
        [chooser](CommandContext& ctx) {
        int index = 0;
        if (!parseInt(ctx.args[0], index)) {
            spdlog::warn("chooser_select_game: '{}' is not an index", ctx.args[0]);
            return;
        }
        if (auto* h = chooser()) h->SelectGameByIndex(index);
    });

    add("chooser_tsumego", 1, 1, "<on|off> — set the tsumego toggle",
        [chooser](CommandContext& ctx) {
        const std::string arg = toLower(ctx.args[0]);
        if (arg != "on" && arg != "off" && arg != "true" && arg != "false"
            && arg != "1" && arg != "0") {
            spdlog::warn("chooser_tsumego: expected on or off, got '{}'", ctx.args[0]);
            return;
        }
        const bool on = (arg == "on" || arg == "true" || arg == "1");
        if (auto* h = chooser()) h->SetTsumegoSelected(on);
    });

    add("chooser_files_page", 1, 1, "<next|prev> — page the file list",
        [chooser](CommandContext& ctx) {
        const std::string arg = toLower(ctx.args[0]);
        if (arg != "next" && arg != "prev") {
            spdlog::warn("chooser_files_page: expected next or prev, got '{}'", ctx.args[0]);
            return;
        }
        if (auto* h = chooser()) h->StepFilesPage(arg == "next" ? +1 : -1);
    });

    add("chooser_games_page", 1, 1, "<next|prev> — page the game list",
        [chooser](CommandContext& ctx) {
        const std::string arg = toLower(ctx.args[0]);
        if (arg != "next" && arg != "prev") {
            spdlog::warn("chooser_games_page: expected next or prev, got '{}'", ctx.args[0]);
            return;
        }
        if (auto* h = chooser()) h->StepGamesPage(arg == "next" ? +1 : -1);
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
                parent->showMessage(parent->templateText("tplKomiBeforePlayOnly",
                    "Komi can only be changed before play starts"));
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

void GobanControl::keyPress(int key, unsigned mods, bool downNotUp){

    // Handle prompt keyboard shortcuts (on key UP)
    if (!downNotUp && mods == KeyMod::NONE && parent->hasActivePrompt()) {
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
    const auto keySnap = model.snapshot();
    // The log walked the tree on every keystroke — three accessors, on the UI
    // thread, purely to print a line that is usually discarded.
    spdlog::debug("keyPress: key={}, downNotUp={}, isNavigating={}, viewPos={}/{}",
        key, downNotUp, keySnap->navigating,
        keySnap->viewPosition, keySnap->mainLineMoves);

    if (!downNotUp && mods == KeyMod::NONE && keySnap->navigating) {
        // These four keys dispatch through the registry rather than calling the
        // navigator directly. Two things follow from that and neither is
        // incidental: they answer to the same availableActions() rule as the
        // toolbar buttons, and they are recorded, so a keyboard-driven review
        // survives into a bug report. Navigating by key used to be invisible to
        // ScenarioRecorder, which silently dropped it from every replay.
        //
        // Space/Right: navigate forward (or trigger kibitz at end of unfinished branch)
        if (key == Rml::Input::KI_SPACE || key == Rml::Input::KI_RIGHT) {
            if (!keySnap->atEnd) {
                command("navigate_forward");
                return;  // Handled navigation
            }
            // At end of finished game - nothing more to do
            if (keySnap->atFinishedGame) {
                return;
            }
            // At end of unfinished branch - Space falls through to kibitz
            if (key == Rml::Input::KI_RIGHT) {
                return;  // Right key doesn't trigger kibitz
            }
            // Tsumego: ask through the command, which owns the policy — that a
            // solved position is refused, and that a puzzle is never resumed as
            // a match. Doing it here instead is what let the key and the menu
            // item disagree: this branch reached the engine and `play once` did
            // not. ADR-0005's rule, applied to a key handler.
            if (view.isTsumegoMode()) {
                command("play once");
                return;  // Don't fall through — the table's binding plays a move
            }
            spdlog::debug("Navigation: at end of branch, Space falls through to kibitz");
        }
        if (key == Rml::Input::KI_LEFT || key == Rml::Input::KI_BACK) {
            command("navigate_back");
            return;
        }
    }

    std::string cmd(config->getCommand(static_cast<Rml::Input::KeyIdentifier>(key), mods));
    spdlog::debug("keyPress: key={} mods={} mapped to cmd='{}'", key, mods, cmd);

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
    in.uiReady = acceptsUiEvents();
    // Nothing below is looked at until the UI is ready — availableActions()
    // returns an all-false answer on !uiReady — and gathering it anyway meant
    // interrogating GameThread every frame throughout startup, while
    // loadEnginesParallel was still appending to the player list on another
    // thread. humanToMove() reads players[activePlayer[…]], so a push_back that
    // reallocated the vector left it dereferencing freed memory. Observed as a
    // segfault in humanToMove() with loadEnginesParallel live in another thread.
    if (!in.uiReady) return in;

    in.phase             = model.phase();
    in.engineThinking    = engine.isThinking();
    // The same term isIdle() has had all along. It was never gathered here, so
    // the policy could not see the seconds-long resync after a board size
    // change — the toolbar stayed lit and a click went into queuedMove.
    in.enginesSyncing    = engine.isSyncingEngines();
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
    in.onBadMovePath     = snap->onBadMovePath;
    // The predicate toggle_territory used to guard itself with, lifted into the
    // policy so the button and the command cannot answer differently.
    in.scoredEnd         = snap->scoredEnd;
    in.hasMoves          = snap->moveCount > 0
                           || !model.setupBlackStones.empty()
                           || !model.setupWhiteStones.empty()
                           || snap->mainLineMoves > 0;
    in.hasUnsavedChanges = model.game.hasUnsavedChanges();
    // "Is there an engine that could answer", not "is it answering now". The
    // overlay's own state — starting, yielded, unavailable — belongs to the
    // panel, not to whether the toggle may be pressed.
    // Configured *and* not found incapable. isConfigured() alone offered a
    // toggle that could not work — the comment on evaluationAvailable already
    // said "has not been found incapable", which nothing implemented.
    in.evaluationAvailable = parent->getAnalysis().isConfigured()
                          && parent->getAnalysis().state() != AnalysisState::Unavailable;
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
    // A shader still being linked in the background. Sixth term, and the only
    // one that is not about the game: until it lands there is no board to read,
    // so a scripted run that proceeded here would be asking questions of a view
    // that has never drawn. It is bounded — a link finishes — so unlike
    // EngineSync::Unsynced it is safe to wait on.
    if (view.gobanShader.isBuilding()) return false;
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
    // The fifth term, and the one this predicate was missing: a move handed to
    // the loop that it has not taken up yet. playLocalMove() leaves it in
    // `queuedMove` whenever nobody is blocked in genmove() — between two moves,
    // or before the loop reaches its first turn — so a scripted run could ask
    // for the board immediately after a click and be told, truthfully by the
    // other four terms and uselessly, that everything was idle.
    if (engine.hasQueuedMove()) return false;
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
    // What the *renderer* was handed, as against what the board knows. These
    // are the uniforms that decide how many stones sit in the bowls, and they
    // were permanently zero while the two keys above were right — so a scenario
    // asserting only the board's count could not see it. Same distinction as
    // sounds_played and overlay_glyphs.
    s["prisoners_drawn_black"] = view.capturedBlackShown;
    s["prisoners_drawn_white"] = view.capturedWhiteShown;

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
    // The status indicator and the log behind it. `log_badge` is what the user
    // sees without opening anything, so a scenario can assert that a failure was
    // actually surfaced rather than only logged — which is the whole point of
    // the feature and otherwise invisible to the harness.
    {
        const auto& log = MessageLog::instance();
        s["log_count"]  = static_cast<int>(log.size());
        // What the badge actually counts: arrivals since the panel was last
        // opened. log_count saturates at the buffer capacity and so cannot tell
        // "nothing new" from "the buffer is full".
        s["log_unseen"] = static_cast<int>(log.unseenCount());
        s["log_open"]   = parent->isLogPanelOpen();
        s["log_badge"]  = !log.hasUnseen() ? "none"
                        : (log.unseenSeverity() == MessageSeverity::Error ? "error" : "warning");
        s["engine_loading"] = engine.engineLoadingSummary();
        // The line itself, not the condition behind it: a wait the user cannot
        // see is the failure, so what was actually painted is the thing worth
        // asserting.
        s["status_text"] = parent->statusText();
    }

    // The two game waits, which the board reports rather than the status line.
    // Same argument as status_text above and for the same reason it replaced a
    // bare isThinking(): what was *painted* is the thing that was missing, so
    // that is what a scenario asserts. `wait_text` is empty during the grace
    // period, when the wait is real but deliberately not yet mentioned — so the
    // kind and the text are both reported, and they are not the same question.
    s["wait_clock_shown"] = view.isWaitClockShown();
    s["wait_indicator"] = waitKindName(view.waitIndicator());
    s["wait_text"] = view.waitIndicatorText();

    // Sounds mixed all the way to their end, not merely requested. The
    // distinction is the whole point: a request the mixer never saw looked
    // exactly like one it played, which is why a swallowed stone sound had no
    // symptom to assert on.
    s["sounds_played"] = static_cast<double>(view.soundsPlayed());
    // Text the glyph pass actually put on screen, not text that was prepared for
    // it. The two move-marker toggles used to gate the whole pass, so switching
    // both off blanked the coordinates, the markup and the evaluation as well —
    // with every key describing them still reading true. Same reason
    // sounds_played counts what was heard.
    s["overlay_glyphs"] = static_cast<int>(view.overlayGlyphs());
    // The stereo depth budget for the camera as it stands. `stereo_deviation`
    // is the quantity the literature bounds at 1/30 of the image width; it is
    // reported whatever shader is selected, because what it describes is the
    // camera, not the shader.
    s["stereo"]           = view.gobanShader.isStereo();
    s["stereo_base"]      = view.stereoHalfBase();
    s["stereo_near"]      = view.stereoNearPoint();
    s["stereo_deviation"] = view.stereoDeviation();
    // Where the scene meets the glass — what `dof` decides, and the one thing
    // none of the three above can answer. Reported with the board's own depth
    // range beside it, because the question anybody asks of it ("is the screen
    // plane at the back of the board?") is a comparison, not a distance.
    float boardNear = 0.0f, boardFar = 0.0f;
    view.stereoBoardDepth(boardNear, boardFar);
    s["stereo_convergence"] = view.stereoConvergence();
    s["stereo_board_near"]  = boardNear;
    s["stereo_board_far"]   = boardFar;
    // Where the window sits relative to the nearest thing in frame: 1 means it
    // rests exactly on it, which is the default and the whole point of deriving
    // it. Published as a ratio because that is the *invariant* — it holds at
    // every zoom and aspect ratio, where the two distances above are camera
    // specific, and "assert at more than one zoom" is the lesson this file's
    // stereo scenario was written to enforce.
    const float near = view.stereoNearPoint();
    s["stereo_convergence_ratio"] = near > 0.0f ? view.stereoConvergence() / near : 0.0f;
    // Reported whatever shader is selected, for the same reason: it is a
    // property of the viewer, not of the shader, and it outlives a switch to
    // mono and back.
    s["anaglyph"]          = Stereo::anaglyphName(view.anaglyph());
    s["anaglyph_strength"] = view.anaglyphStrength();
    s["anaglyph_leak_r"]   = view.anaglyphLeak().r;
    s["anaglyph_leak_g"]   = view.anaglyphLeak().g;
    s["anaglyph_leak_b"]   = view.anaglyphLeak().b;
    s["anaglyph_balance_l"] = view.anaglyphBalance().left;
    s["anaglyph_balance_r"] = view.anaglyphBalance().right;
    s["glasses"]            = Stereo::glassesName(view.glasses());
    s["anaglyph_green"]     = view.anaglyphGreen();
    // What the shader was actually told to draw, not what policy would like: the
    // stereo gate and the over-the-board gate both fold into this one number, so
    // a scenario asserting it is asserting the thing on screen.
    s["pointer_mark"]       = view.pointerMark();
    s["pointer_mode"]       = pointerModeName(view.pointerMode_());
    // The point under the pointer. A scenario that has to hover a *particular*
    // intersection can only work in window pixels, so it needs to assert what
    // those pixels resolved to — otherwise a camera change would silently move
    // the test to a different point rather than failing.
    s["cursor_col"]         = model.cursor.col();
    s["cursor_row"]         = model.cursor.row();
    s["tsumego"]        = model.tsumegoMode.load();
    s["holds_stone"]    = model.state.holdsStone;
    s["show_territory"] = model.board.showTerritory;
    s["unsaved_changes"] = model.game.hasUnsavedChanges();
    s["msg"]            = messageName(model.state.msg);
    // What the message tail and the board overlay are showing. Asserting these
    // is the only way to cover ElementGame::OnUpdate()'s comment/markup handling,
    // which is the least-tested code in the program and the last thing still
    // reading across the thread boundary unsynchronised (ADR-0006 stage 3).
    s["comment"]        = snap->comment;
    s["markup_count"]   = snap->markup.size();

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
    s["can_play"]      = a.play;
    s["can_resign"]    = a.resign;
    s["can_undo"]      = a.undo;
    s["can_kibitz"]    = a.kibitz;
    s["can_navigate"]  = a.navigate;
    s["can_territory"] = a.territory;
    s["can_clear"]     = a.clear;
    s["can_save"]      = a.save;
    s["can_evaluation"] = a.evaluation;

    // The evaluation overlay. `eval_state` is the one to wait on — `eval_enabled`
    // flips the instant the toggle is pressed, long before a process has started
    // or a number has arrived. The numbers are absent rather than zero when
    // there is no report, for the reason ADR-0007 decision 12 gives.
    {
        const auto& analysis = parent->getAnalysis();
        s["eval_enabled"] = analysis.isEnabled();
        s["eval_state"]   = analysisStateName(analysis.state());
        // The move suggestions are a separate feature from the readout, and off
        // by default — `eval_labels` reading 0 with the evaluation running is
        // the normal case, not a fault.
        s["eval_moves_shown"] = view.isAnalysisOverlayShown();
        s["eval_board_text"] = view.evaluationReadoutText();
        s["eval_readout_shown"] = view.isEvaluationReadoutShown();
        s["eval_align"] = GobanView::alignName(view.evaluationAlign());
        s["coordinates_shown"] = view.areCoordinatesShown();
    s["last_move_shown"] = view.isLastMoveOverlayShown();
    s["next_move_shown"] = view.isNextMoveOverlayShown();
        s["coord_color"] = hexFromColor(view.coordinateColor());
        s["coord_offset"] = view.coordinateOffset();
        s["eval_color"] = hexFromColor(view.readoutColor());
        s["eval_labels"]  = static_cast<int>(view.analysisLabelCount());
        // Suggestions that landed on a move already in the record: the label
        // stays and only takes on colour. The split between these two keys is
        // the combine rule.
        s["eval_tints"]   = static_cast<int>(view.analysisTintCount());
        // A recommended pass has no point to sit on, so it is neither a label
        // nor a tint — it is the word in the margin, and needs a key of its own.
        s["eval_pass"]    = view.analysisPassText();
        if (const auto rep = analysis.report()) {
            s["eval_winrate"] = static_cast<int>(std::lround(rep->winrateBlack * 100.0));
            s["eval_score"]   = rep->scoreLeadBlack ? *rep->scoreLeadBlack : 0.0;
            s["eval_has_score"] = rep->scoreLeadBlack.has_value();
            s["eval_moves"]   = static_cast<int>(rep->moves.size());
            // Whether what is displayed still describes the position on screen.
            s["eval_stale"]   = rep->positionId != snap->positionId;
        } else {
            s["eval_winrate"] = -1;
            s["eval_has_score"] = false;
            s["eval_moves"]   = 0;
            s["eval_stale"]   = true;
        }
    }

    // Where the position came from — needed to reconstruct a starting point
    // when replaying a recorded session.
    s["sgf_file"] = snap->sgfFile;
    s["game_index"] = snap->gameIndex;

    // File chooser, so a scenario can assert what the dialog is offering rather
    // than only what loading it produced.
    if (auto* h = dynamic_cast<EventHandlerFileChooser*>(EventManager::GetEventHandler("open"))) {
        s["chooser_active"]     = h->IsDialogVisible();
        s["chooser_path"]       = h->GetCurrentPath();
        s["chooser_file_count"] = h->GetFileCount();
        s["chooser_game_count"] = h->GetGameCount();
        s["chooser_file"]       = h->GetSelectedFileName();
        s["chooser_game"]       = h->GetSelectedGameIndex();
        s["chooser_tsumego"]    = h->IsTsumegoSelected();
    } else {
        s["chooser_active"]     = false;
        s["chooser_path"]       = std::string();
        s["chooser_file_count"] = 0;
        s["chooser_game_count"] = 0;
        s["chooser_file"]       = std::string();
        s["chooser_game"]       = -1;
        s["chooser_tsumego"]    = false;
    }

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
