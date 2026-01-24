#ifndef GOBAN_GOBANCONTROL_H
#define GOBAN_GOBANCONTROL_H

#include "GameThread.h"
#include "GobanModel.h"
#include "GobanView.h"

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
    void command(const std::string& cmd);
    void saveCurrentGame() const;

private:
    ElementGame* parent;
    GobanModel& model;
    GobanView& view;
    GameThread& engine;

    bool initialized;
    bool exit;
    float mouseX, mouseY;
    bool fullscreen;
    bool syncingUI = true;  // Suppress game actions when syncing UI to match state

public:
    void finishInitialization() { syncingUI = false; }
    bool isSyncingUI() const { return syncingUI; }
    void setSyncingUI(bool syncing) { syncingUI = syncing; }
};


#endif //GOBAN_GOBANCONTROL_H
