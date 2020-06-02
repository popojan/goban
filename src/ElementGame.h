#ifndef ROCKETINVADERSELEMENTGAME_H
#define ROCKETINVADERSELEMENTGAME_H

#include "GobanControl.h"
#include <Rocket/Core/Element.h>
#include <Rocket/Core/EventListener.h>
#include <Rocket/Core/StyleSheet.h>

class ElementGame : public Rocket::Core::Element,
    public Rocket::Core::EventListener
{
public:
    ElementGame(const Rocket::Core::String& tag);
    virtual ~ElementGame();

    void ProcessEvent(Rocket::Core::Event& event);

    void OnChildAdd(Rocket::Core::Element* element);

    bool needsRepaint() {
        return view.needsRepaint();
    }
    void requestRepaint() {
        view.requestRepaint();
    }

    bool isExiting() { return control.isExiting(); }
    void Reshape();
    void gameLoop();
    void populateEngines();
    GobanControl& getController() { return control; }
protected:
    virtual void OnUpdate();
public:
    virtual void OnRender();

private:

    int WINDOW_WIDTH = 0, WINDOW_HEIGHT = 0;

    GobanModel model;
    GobanView view;
    GameThread engine;
    GobanControl control;
    std::mutex mutex;
    bool hasResults, calculatingScore;
};

#endif
