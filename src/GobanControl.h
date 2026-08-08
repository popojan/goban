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
    void switchPlayer(int which, int idx) const;
    void switchShader(int idx) const;
    bool setHandicap(int) const;
    bool setKomi(float) const;
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

private:
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
