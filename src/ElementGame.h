/** \file
 *  \brief The RmlUi element that owns the whole application object graph.
 *
 * Holds GobanModel, GobanView, GameThread and GobanControl by value, in that
 * order, and registers the two GameObservers. Beyond ownership its jobs are the
 * per-frame `OnUpdate()` (widget sync, action availability, message line), the
 * async engine-loading state machine that lets the board render before any
 * engine has spoken, and the prompt/message system.
 *
 * UI thread only. It is also where a deferred game-replacing action lands:
 * `takeDeferredTaskDone()` in `gameLoop()` is the one place the game thread's
 * completion becomes a widget update, because RmlUi is not thread safe.
 */
#ifndef ROCKETINVADERSELEMENTGAME_H
#define ROCKETINVADERSELEMENTGAME_H

#include "AnalysisService.h"
#include "GobanControl.h"
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/StyleSheet.h>
#include <array>
#include <future>
#include <atomic>
#include <functional>

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

    /// Enter or leave a puzzle. GameThread owns the mode — the model's flag and
    /// the record's session-copy suppression are copies it publishes — so the
    /// authoritative write goes there and only the view's own copy is set here.
    void setTsumegoMode(bool enabled) {
        engine.setGameMode(enabled ? GameMode::TSUMEGO : GameMode::MATCH);
        view.setTsumegoMode(enabled);
        if (enabled) {
            cacheTsumegoHints();
        }
    }

    bool isTsumegoMode() const {
        return view.isTsumegoMode();
    }

    void cacheTsumegoHints();

    bool needsRender() const {
        return view.needsRender();
    }

    /// Non-empty when a scenario asked for a screenshot; the main loop calls
    /// this right before the buffer swap and captures the finished frame.
    std::string takeScreenshotRequest() { return view.takeScreenshotRequest(); }

    // Returns timeout for idle sleep: >0 = seconds to wait, -1 = wait forever
    double getIdleTimeout() const;

    bool isExiting() const { return control.isExiting(); }
    void Reshape();
    void gameLoop();
    void populateUIElements();  // Populate engine-independent UI elements immediately
    void refreshPlayerDropdowns();  // Clear and repopulate player dropdowns from active players
    // Game settings dropdowns (board/komi/handicap) are synced automatically in OnUpdate
    GobanControl& getController() { return control; }
    GameThread& getGameThread() { return engine; }
    /// The live evaluation overlay's engine and thread. Owned here because this
    /// object owns the graph; declared after `engine` so it is destroyed first
    /// and cannot outlive the predicate it reads. See ADR-0007.
    AnalysisService& getAnalysis() { return analysis; }
    /// Async engine loading has finished. Scenario scripts must not issue game
    /// commands before this, since players are not yet assigned.
    [[nodiscard]] bool areEnginesLoaded() const { return enginesLoaded; }
    const GobanModel& getModel() const { return model; }
    void OnMenuToggle(const std::string& cmd, bool checked) const;
    void setElementDisabled(const std::string& elementId, bool disabled) const;

    /// Localised text for a template id, or `fallback` when the interface
    /// language's .rml does not define it.
    ///
    /// The fallback matters: a template that is merely absent used to produce an
    /// *empty* message, so a translated interface silently said nothing where
    /// English said "Save failed". Degrading to English is the lesser failure,
    /// and it means a new string can be added to en/goban.rml alone without the
    /// other four becoming mute until someone translates them.
    [[nodiscard]] std::string templateText(const char* id, const std::string& fallback = "") const;

    // Message system - template-based messages in lblMessage
    void showMessage(const std::string& text);  // Dismissable message
    void showPromptYesNo(const std::string& message, std::function<void(bool)> callback);
    void showPromptYesNoTemplate(const std::string& templateId, std::function<void(bool)> callback);
    void showPromptOkCancel(const std::string& message, std::function<void(bool)> callback);
    void handlePromptResponse(bool affirmative);  // Called by event handlers
    void clearMessage();
    bool hasActivePrompt() const { return pendingPromptCallback != nullptr; }

    // Status indicator and log panel — the user-visible end of MessageLog.
    // Opening the panel is what marks the messages seen, which is why the badge
    // and the panel are one feature rather than two.
    void toggleLogPanel();
    void setLogPanelOpen(bool open);
    void clearLog();
    [[nodiscard]] bool isLogPanelOpen() const { return logPanelOpen; }
    /// What #lblStatus is showing, as last written. Exposed so a scenario can
    /// assert that a wait was *explained* rather than merely survived — the
    /// distinction the thinking and syncing lines exist for.
    [[nodiscard]] const std::string& statusText() const { return statusTextShown; }

protected:
    void OnUpdate() override;
public:
    void OnRender() override;

private:

    int WINDOW_WIDTH = 0, WINDOW_HEIGHT = 0;

    GobanModel model;
    GobanView view;
    GameThread engine;
    AnalysisService analysis;
    GobanControl control;
    std::mutex mutex;

    /// Which player each colour's dropdown is currently showing. The change
    /// detector for the two player dropdowns — an index rather than
    /// `GameState::black`/`white`, which are std::strings the game thread
    /// reassigns while this thread reads them. PlayerManager hands the index out
    /// under its own mutex.
    static constexpr size_t NO_PLAYER_SHOWN = static_cast<size_t>(-1);
    std::array<size_t, 2> shownPlayerIndex{NO_PLAYER_SHOWN, NO_PLAYER_SHOWN};

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
    int sgfGameIndex{-1};   // Game index to load (-1 = last game)

    // Session restoration state
    int sessionTreePathLength{0};
    std::vector<int> sessionTreePath;  // Branch choices only (at multi-child nodes)
    bool sessionIsExternal{false};
    GameMode sessionGameMode{GameMode::MATCH};
    bool sessionRestoreNeeded{false};

    void startAsyncEngineLoading();
    void checkEngineLoadingComplete();
    void performDeferredInitialization();
    void updateLoadingStatus(const std::string& message);

    // Status indicator / log panel state. logVersionShown is what stops the
    // panel being rebuilt on every frame: MessageLog::version() is an atomic, so
    // an open panel over a quiet log costs one relaxed load.
    void syncStatusIndicator();
    void rebuildLogPanel();

    /// The four prisoner labels, written from one place so the two callers
    /// cannot disagree about which colour took which stones — they did.

    bool logPanelOpen{false};
    uint64_t logVersionShown{0};
    std::string statusTextShown;

    // Determine initial board size by peeking at SGF that will be loaded
    static int determineInitialBoardSize();

    // Greys out unavailable actions; rules in availableActions(), inputs from
    // GobanControl. See UiActions.h.
    void syncActionAvailability();

    // Sync a dropdown selection to match a value string
    void syncDropdown(Rml::Element* container, const char* elementId, const std::string& value);

    // Prompt system callback storage
    std::function<void(bool)> pendingPromptCallback;
};

#endif
