#ifndef ROCKETINVADERSELEMENTGAME_H
#define ROCKETINVADERSELEMENTGAME_H

#include "GobanControl.h"
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/StyleSheet.h>
#include <future>
#include <atomic>

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

    bool isExiting() const { return control.isExiting(); }
    void Reshape();
    void gameLoop();
    void populateUIElements();  // Populate engine-independent UI elements immediately
    void populateEngines();     // Populate player dropdowns (requires engines loaded)
    void refreshPlayerDropdowns();  // Clear and repopulate player dropdowns
    void refreshGameSettingsDropdowns();  // Sync board/komi/handicap dropdowns with model state
    GobanControl& getController() { return control; }
    GameThread& getGameThread() { return engine; }
    void OnMenuToggle(const std::string& cmd, bool checked) const;

protected:
    void OnUpdate() override;
public:
    void OnRender() override;

private:

    int WINDOW_WIDTH = 0, WINDOW_HEIGHT = 0;

    GobanModel model;
    GobanView view;
    GameThread engine;
    GobanControl control;
    std::mutex mutex;

    // Async engine loading state
    std::future<void> engineLoadFuture;
    std::atomic<bool> enginesLoaded{false};
    std::atomic<bool> engineLoadingStarted{false};
    std::atomic<bool> stonesDisplayed{false};  // True when first engine loaded SGF
    bool deferredInitNeeded{false};
    bool deferredInitDone{false};

    // Initial state determined before engine loading
    int initialBoardSize{19};
    std::string sgfToLoad;  // Empty if starting fresh or no file exists

    void startAsyncEngineLoading();
    void checkEngineLoadingComplete();
    void performDeferredInitialization();
    void updateLoadingStatus(const std::string& message);

    // Determine initial board size by peeking at SGF that will be loaded
    static int determineInitialBoardSize();
};

#endif
