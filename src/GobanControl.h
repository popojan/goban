#ifndef GOBAN_GOBANCONTROL_H
#define GOBAN_GOBANCONTROL_H

#include "GameThread.h"
#include "GobanModel.h"
#include "GobanView.h"
#include "Configuration.h"

class ElementGame;

class GobanControl {
public:
    GobanControl(ElementGame* p, GobanModel& m, GobanView& v, GameThread& e)
            : parent(p), model(m), view(v), engine(e),
            initialized(false), exit(false), mouseX(-1), mouseY(-1), fullscreen(false)
    {
    }

    ~GobanControl() { destroy(); }

    void destroy();

    void mouseClick(int button, int state, int x, int y);
    void mouseMove(int x, int y);
    void keyPress(int key, int x, int y, bool downNotUp = false);
    bool isExiting() {
        return exit;
    }
    bool requestExit() {
        exit = true;
        Shell::RequestExit();
        return true;
    }
    bool newGame(int boardSize);
    void togglePlayer(int which, int delta = 1);
    void switchPlayer(int which, int idx);
    void switchShader(int idx);
    void increaseHandicap();
    bool setHandicap(int);
    bool setKomi(float);
    bool command(const std::string& cmd);

private:
    ElementGame* parent;
    GobanModel& model;
    GobanView& view;
    GameThread& engine;

    bool initialized;
    bool exit;
    float mouseX, mouseY;
    bool fullscreen;
};


#endif //GOBAN_GOBANCONTROL_H
