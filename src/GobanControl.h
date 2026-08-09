#ifndef GOBAN_GOBANCONTROL_H
#define GOBAN_GOBANCONTROL_H

#include "GameThread.h"
#include "GobanModel.h"
#include "GobanView.h"

#include <nlohmann/json.hpp>

#include <functional>
#include <map>
#include <string>
#include <vector>

class ElementGame;

class GobanControl {
public:
    GobanControl(ElementGame* p, GobanModel& m, GobanView& v, GameThread& e)
            : parent(p), model(m), view(v), engine(e),
            initialized(false), exit(false), mouseX(-1), mouseY(-1), fullscreen(false)
    {
    }

    ~GobanControl() { destroy(); }

    void destroy() const;

    void mouseClick(int button, int state, int x, int y);
    void mouseMove(int x, int y);
    void keyPress(int key, int x, int y, bool downNotUp = false);
    [[nodiscard]] bool isExiting() const {
        return exit;
    }
    bool newGame(unsigned boardSize) const;

    /// UI half of a game-replacing action (new game, load, switch game).
    /// Must run on the UI thread; ElementGame calls it when a deferred action
    /// finishes on the game thread.
    void finishGameReplacement() const;

    void switchPlayer(int which, int idx) const;
    void switchShader(int idx) const;
    bool setHandicap(int) const;
    bool setKomi(float) const;

    /// Board-size and handicap changes replace the game on screen, so they ask
    /// first when there is a game worth keeping — the same gate `clear` has
    /// always had, and the one the dropdowns were missing.
    ///
    /// The answer arrives asynchronously, so the caller cannot revert its
    /// widget at the call site: `onSettled(changed)` runs once the outcome is
    /// known, with `changed == false` when the user declined or the change
    /// failed.
    void requestNewGame(unsigned boardSize, std::function<void(bool)> onSettled) const;
    void requestHandicap(int handicap, std::function<void(bool)> onSettled) const;
    // Parses "name arg arg..." and dispatches through the command registry.
    void command(const std::string& cmd);
    // Dispatches an already split command (scripting channel entry point).
    void command(const std::string& name, const std::vector<std::string>& args);
    void saveCurrentGame() const;

    /// Machine-readable snapshot of the state a scenario can assert on.
    /// Keys are stable names; values are strings, numbers or bools. Used by
    /// ScenarioRunner for `expect <key> <value>` and printed in full when an
    /// expectation fails, so a failure is diagnosable without a debugger.
    [[nodiscard]] nlohmann::json dumpState() const;

    /// True when it is safe for a scenario to issue the next command: engines
    /// are loaded, no engine is thinking, and no programmatic UI sync is in
    /// flight.
    [[nodiscard]] bool isIdle() const;

    /// Whether resigning is meaningful right now. One predicate, two callers:
    /// the `resign` command guards on it, and ElementGame greys `cmdResign` by
    /// it. Keeping the button and the command on the same question is the point
    /// — they used to disagree, so a resign refused by the toolbar still went
    /// through from the keybinding.
    [[nodiscard]] bool canResign() const;

private:
    /// Runs `replace` once the user has agreed to discard the current game, or
    /// straight away when GobanModel::hasGameWorthKeeping() says there is
    /// nothing to lose.
    void confirmGameReplacement(std::function<void()> replace,
                                std::function<void()> onCancelled) const;

    // Per-invocation state handed to a command handler.
    struct CommandContext {
        const std::vector<std::string>& args;
        bool checked;      // menu toggle state, forwarded to ElementGame::OnMenuToggle
        bool notifyMenu;   // clear to skip OnMenuToggle (old early-`return` semantics)
    };

    struct CommandEntry {
        std::function<void(CommandContext&)> handler;
        int minArgs;
        int maxArgs;
        const char* help;  // "<args> — description", or plain description if argument-less
    };

    /// The actual reset. Runs on whichever thread owns the engines at the time:
    /// the UI thread when nothing is thinking, otherwise the game thread via
    /// GameThread::runWhenEngineFree.
    bool newGameNow(unsigned boardSize) const;

    void buildRegistry();
    void listCommands() const;
    // Left click on an on-board intersection; shared by mouseClick and "click".
    void boardClick(const Position& coord);

    ElementGame* parent;
    GobanModel& model;
    GobanView& view;
    GameThread& engine;

    bool initialized;
    bool exit;
    float mouseX, mouseY;
    bool fullscreen;
    bool syncingUI = true;  // Suppress game actions when syncing UI to match state
    std::map<std::string, CommandEntry> registry;  // built lazily on first command()

public:
    void finishInitialization() { syncingUI = false; }
    bool isSyncingUI() const { return syncingUI; }
    void setSyncingUI(bool syncing) { syncingUI = syncing; }
};


#endif //GOBAN_GOBANCONTROL_H
