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
    explicit ElementGame(const Rocket::Core::String& tag);
    ~ElementGame() override;

    void ProcessEvent(Rocket::Core::Event& event);

    void OnChildAdd(Rocket::Core::Element* element);

    void requestRepaint() {
        view.requestRepaint();
    }

    bool isExiting() { return control.isExiting(); }
    void Reshape();
    void gameLoop();
    void populateEngines();
    GobanControl& getController() { return control; }
    void OnMenuToggle(const std::string& cmd, bool checked);

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
};

#endif
