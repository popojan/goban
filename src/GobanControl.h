//
// Created by jan on 7.5.17.
//

#ifndef GOBAN_GOBANCONTROL_H
#define GOBAN_GOBANCONTROL_H

#include "GameThread.h"
#include "GobanModel.h"
#include "GobanView.h"
#include "spdlog/spdlog.h"

class ElementGame;

class GobanControl {
public:
    GobanControl(ElementGame& p, GobanModel& m, GobanView& v, GameThread& e)
            : parent(p), model(m), view(v), engine(e), rButtonDown(false), mButtonDown(false), startX(-1),
              startY(-1), initialized(false), exit(false), mouseX(-1), mouseY(-1), firstGame(true)
    {
        console = spdlog::get("console");
    };
    ~GobanControl() { destroy(); }

    void destroy();

    void mouseClick(int button, int state, int x, int y);
    void mouseMove(int x, int y);
    void keyPress(int key, int x, int y, bool downNotUp = false);
    bool isExiting() { return exit; }
    void newGame(int boardSize);
    void togglePlayer(int which, int delta = 1);
    void switchPlayer(int which, int idx);
    void increaseHandicap();
    bool setKomi(float);

private:
    ElementGame& parent;
    GobanModel& model;
    GobanView& view;
    GameThread& engine;
private:
    bool rButtonDown, mButtonDown;
    //float mouse[2];
    int startX, startY;
    bool initialized;
    bool exit;
    float mouseX, mouseY;
    bool firstGame;
    std::shared_ptr<spdlog::logger> console;
};


#endif //GOBAN_GOBANCONTROL_H
