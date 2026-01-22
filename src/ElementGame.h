#ifndef ROCKETINVADERSELEMENTGAME_H
#define ROCKETINVADERSELEMENTGAME_H

#include "GobanControl.h"
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/StyleSheet.h>

class ElementGame : public Rml::Element,
    public Rml::EventListener
{
public:
    explicit ElementGame(const Rml::String& tag);
    ~ElementGame() override;

    void ProcessEvent(Rml::Event& event) override;

    void OnChildAdd(Rml::Element* element) override;

    void requestRepaint() {
        view.requestRepaint();
    }

    void updateNavigationOverlay() {
        view.updateNavigationOverlay();
    }

    bool needsRender() const {
        return view.needsRender();
    }

    // Returns timeout for idle sleep: >0 = seconds to wait, -1 = wait forever
    double getIdleTimeout() const;

    bool isExiting() { return control.isExiting(); }
    void Reshape();
    void gameLoop();
    void populateEngines();
    void refreshPlayerDropdowns();  // Clear and repopulate player dropdowns
    GobanControl& getController() { return control; }
    GameThread& getGameThread() { return engine; }
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
