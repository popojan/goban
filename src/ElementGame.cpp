#include "ElementGame.h"
#include "MessageLog.h"
#include "AppState.h"
#include "UserSettings.h"
#include "version.h"
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core.h>
#include <RmlUi/Core/StringUtilities.h>
#include <GLFW/glfw3.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <filesystem>
#include <fstream>
#include <sstream>

// Escape special characters for safe RML display
static std::string escapeRml(const std::string& text) {
    std::string result;
    result.reserve(text.size() * 1.2);  // Slight over-allocation for escapes
    for (char c : text) {
        switch (c) {
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '&': result += "&amp;"; break;
            case '"': result += "&quot;"; break;
            default: result += c; break;
        }
    }
    return result;
}

// Get text from a localized template element
static std::string getTemplateText(Rml::Context* context, const std::string& templateId) {
    auto doc = context->GetDocument("game_window");
    if (!doc) return "";
    auto tpl = doc->GetElementById(templateId.c_str());
    if (!tpl) return "";
    return tpl->GetInnerRML().c_str();
}

ElementGame::ElementGame(const Rml::String& tag)
        : Rml::Element(tag),
          model(determineInitialBoardSize()),
          view(model), engine(model), analysis(model, engine),
          control(this, model, view, engine)
{
    // Register observers (doesn't require engines)
    engine.addGameObserver(&model);
    engine.addGameObserver(&view);

    // The analysis thread publishes from its own thread and must not touch
    // RmlUi; waking the renderer is all it is allowed to ask for, and it only
    // asks when a displayed value actually changed (ADR-0007 decision 14).
    //
    // UPDATE_OVERLAY|UPDATE_STONES rather than a bare repaint: a move suggestion
    // is a board label, and a label sets the annotation material, which has to
    // reach the stone upload or the grid stays drawn under it.
    analysis.setOnUpdate([this] {
        view.requestRepaint(GobanView::UPDATE_OVERLAY | GobanView::UPDATE_STONES);
    });
    view.setAnalysisService(&analysis);

    // Game record creation deferred to loadEnginesParallel (after board size/komi/handicap known)
    // Engine loading is deferred to async thread - board renders immediately
}

void ElementGame::populateUIElements() {
    // Populate shaders dropdown (doesn't require engines)
    auto selectShader = dynamic_cast<Rml::ElementFormControlSelect*>(
            GetContext()->GetDocument("game_window")->GetElementById("selectShader"));

    if(!selectShader) {
        spdlog::warn("missing GUI element [selectShader]");
    } else {
        using nlohmann::json;
        const auto shaders(config->data.value("shaders", json::array()));

        int i = 0;
        for(json::const_iterator it = shaders.begin(); it != shaders.end(); ++it, ++i){
            std::ostringstream ss;
            ss << i;
            std::string shaderName(it->value("name", ss.str()));
            std::string shaderIndex(ss.str());
            selectShader->Add(shaderName.c_str(),shaderIndex.c_str());
        }
        // Sync shader menu to restored shader state
        int currentShader = view.gobanShader.getCurrentProgram();
        if (currentShader >= 0 && currentShader < static_cast<int>(shaders.size())) {
            selectShader->SetSelection(currentShader);
        } else {
            selectShader->SetSelection(0);
        }
    }

    // Populate languages from config/*.json files
    auto selectLanguage = dynamic_cast<Rml::ElementFormControlSelect*>(
            GetContext()->GetDocument("game_window")->GetElementById("selectLanguage"));

    if (!selectLanguage) {
        spdlog::warn("missing GUI element [selectLanguage]");
    } else {
        namespace fs = std::filesystem;
        std::string configDir = "./config";

        // Get current language from config's gui path (e.g., "./config/gui/zh" -> "zh")
        std::string currentGui = config->data.value("gui", "./config/gui/en");
        std::string currentLang = fs::path(currentGui).filename().u8string();
        int currentLangIndex = -1;

        try {
            int index = 0;
            for (const auto& entry : fs::directory_iterator(configDir)) {
                if (entry.path().extension() == ".json") {
                    std::ifstream file(entry.path());
                    if (file) {
                        try {
                            nlohmann::json cfg;
                            file >> cfg;
                            if (cfg.contains("language_name")) {
                                std::string langName = cfg["language_name"].get<std::string>();
                                std::string langFile = entry.path().stem().u8string();  // e.g., "en" from "en.json"
                                selectLanguage->Add(langName.c_str(), langFile.c_str());
                                spdlog::info("Found language: {} ({})", langName, langFile);
                                if (langFile == currentLang) {
                                    currentLangIndex = index;
                                }
                                index++;
                            }
                        } catch (const std::exception& e) {
                            spdlog::warn("Failed to parse {}: {}", entry.path().u8string(), e.what());
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            spdlog::warn("Failed to scan config directory: {}", e.what());
        }

        // Set selection to current language to avoid triggering onchange
        if (currentLangIndex >= 0) {
            selectLanguage->SetSelection(currentLangIndex);
        }
    }

    // Sync fullscreen menu state with restored state
    OnMenuToggle("toggle_fullscreen", AppState::IsFullscreen());

    // Sync sound state with user settings
    bool soundEnabled = UserSettings::instance().getSoundEnabled();
    view.player.setMuted(!soundEnabled);
    OnMenuToggle("toggle_sound", soundEnabled);

    // Sync FPS/VSync toggle state (MAX_FPS defaults to false = event-driven)
    OnMenuToggle("toggle_fps", view.isFpsLimitEnabled());

    // Populate version label (uses its own content as format string)
    if (auto versionLabel = GetContext()->GetDocument("game_window")->GetElementById("lblVersion")) {
        versionLabel->SetInnerRML(
            Rml::CreateString(versionLabel->GetInnerRML().c_str(), GOBAN_VERSION).c_str()
        );
    }

    // Add "Human" placeholder to player dropdowns during loading
    auto selectBlack = dynamic_cast<Rml::ElementFormControlSelect*>(
            GetContext()->GetDocument("game_window")->GetElementById("selectBlack"));
    auto selectWhite = dynamic_cast<Rml::ElementFormControlSelect*>(
            GetContext()->GetDocument("game_window")->GetElementById("selectWhite"));
    if (selectBlack && selectWhite) {
        selectBlack->Add("Human", "0");
        selectWhite->Add("Human", "0");
        selectBlack->SetSelection(0);
        selectWhite->SetSelection(0);
    }
}

void ElementGame::refreshPlayerDropdowns() {
    auto doc = GetContext()->GetDocument("game_window");
    auto selectBlack = dynamic_cast<Rml::ElementFormControlSelect*>(doc->GetElementById("selectBlack"));
    auto selectWhite = dynamic_cast<Rml::ElementFormControlSelect*>(doc->GetElementById("selectWhite"));

    if (!selectBlack || !selectWhite) {
        spdlog::warn("refreshPlayerDropdowns: missing dropdown elements");
        return;
    }

    // Suppress change events during repopulation to prevent transient
    // player switches (e.g. briefly activating an engine during clear)
    GobanControl::WidgetEventGuard suppressEvents(control);

    while (selectBlack->GetNumOptions() > 0)
        selectBlack->Remove(selectBlack->GetNumOptions() - 1);
    while (selectWhite->GetNumOptions() > 0)
        selectWhite->Remove(selectWhite->GetNumOptions() - 1);

    const auto players = engine.getPlayers();
    for (unsigned i = 0; i < players.size(); ++i) {
        std::string idx = std::to_string(i);
        std::string name(players[i]->getName());
        selectBlack->Add(name.c_str(), idx.c_str());
        selectWhite->Add(name.c_str(), idx.c_str());
    }

    // Set selection immediately to avoid single-frame glitch after repopulation
    selectBlack->SetSelection(static_cast<int>(engine.getActivePlayer(0)));
    selectWhite->SetSelection(static_cast<int>(engine.getActivePlayer(1)));

    spdlog::debug("refreshPlayerDropdowns: {} players", players.size());
}

void ElementGame::syncDropdown(Rml::Element* container, const char* elementId, const std::string& value) {
    auto select = dynamic_cast<Rml::ElementFormControlSelect*>(container->GetElementById(elementId));
    if (!select) return;
    int current = select->GetSelection();
    for (int i = 0; i < select->GetNumOptions(); i++) {
        if (select->GetOption(i)->GetAttribute("value", Rml::String()) == value) {
            if (i != current) {
                select->SetSelection(i);
                requestRepaint();
            }
            return;
        }
    }
}

// File-scope statics for FPS tracking (shared between gameLoop and getIdleTimeout)
static int s_fpsFrames = 0;
static int s_fpsLastDisplayed = -1;
static float s_fpsLastTime = -1;
static bool s_fpsSkipFrameCount = false;

void ElementGame::gameLoop() {
    // Check if async engine loading has completed
    checkEngineLoadingComplete();

    // A game-replacing action (new game, load, switch game) that had to wait for
    // an engine finishes on the game thread; the widget half must happen here,
    // on the UI thread, because RmlUi is not thread safe.
    if (engine.takeDeferredTaskDone()) {
        control.finishGameReplacement();
        clearMessage();
    }

    auto context = GetContext();

    float currentTime = static_cast<float>(glfwGetTime());
    if (currentTime - s_fpsLastTime >= 1.0) {
        auto debugElement = context->GetDocument("game_window")->GetElementById("lblFPS");
        auto fpsTemplate = context->GetDocument("game_window")->GetElementById("templateFPS");
        const Rml::String sFps = Rml::CreateString(fpsTemplate->GetInnerRML().c_str(),
            static_cast<float>(s_fpsFrames) / (currentTime - s_fpsLastTime));
        if (debugElement != nullptr && s_fpsLastDisplayed != s_fpsFrames) {
            debugElement->SetInnerRML(sFps.c_str());
            if (s_fpsFrames < 1) {
                s_fpsSkipFrameCount = true;  // Don't count FPS update render when idle
            }
            view.requestRepaint();
            s_fpsLastDisplayed = s_fpsFrames;
        }
        s_fpsFrames = 0;
        spdlog::debug(sFps.c_str());
        s_fpsLastTime = currentTime;

        // Release audio resources after extended idle (3 min) to avoid lag from frequent restart
        view.stopAudioIfInactive();
    }
    ElementGame* game = dynamic_cast<ElementGame*>(context->GetDocument("game_window")->GetElementById("game"));
    if (game != nullptr && game->isExiting()) {
        return;
    }
    // Always update RmlUi for event processing (hover states, etc.)
    context->Update();
    if (view.animationRunning || view.MAX_FPS) {
        view.requestRepaint();
    }
    if (view.updateFlag) {
        // Rendering is managed in the main loop - just count frames here
        if (s_fpsSkipFrameCount) {
            s_fpsSkipFrameCount = false;
        } else {
            s_fpsFrames++;
        }
    }
    //if (!view.MAX_FPS){
    //    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    //}
}

double ElementGame::getIdleTimeout() const {
    // During async engine loading, poll periodically to check completion
    if (!enginesLoaded && engineLoadingStarted) {
        return 0.1;  // 100ms polling interval during loading
    }

    // A deferred action produces no input events, so without a timeout the main
    // loop would block in glfwWaitEvents() and never notice it completed.
    if (engine.hasDeferredTask()) {
        return 0.05;
    }

    // If we displayed non-zero FPS, we need one more wake-up to show "0 fps"
    if (s_fpsLastDisplayed > 0) {
        float currentTime = static_cast<float>(glfwGetTime());
        double remaining = (s_fpsLastTime + 1.0) - currentTime;
        return (remaining > 0) ? remaining : 0.001;  // Small positive to wake immediately if overdue
    }
    return -1.0;  // Already showing 0 or never displayed - sleep forever
}

ElementGame::~ElementGame() {
    // Before anything else: the analysis thread reads `model` and `engine` on
    // every tick, so it has to be joined while both still exist. Member
    // destruction order would do it, but only by accident of declaration order.
    analysis.stop();

    // Wait for async engine loading to complete before destruction
    if (engineLoadFuture.valid()) {
        spdlog::debug("Waiting for engine loading to complete before destruction");
        engineLoadFuture.wait();
    }
}

void ElementGame::startAsyncEngineLoading() {
    if (engineLoadingStarted.exchange(true)) {
        return;  // Already started
    }

    spdlog::info("Starting parallel engine loading");
    // The indicator names the engines itself, from PlayerManager's pending list,
    // and clears when they have all answered. See syncStatusIndicator().

    // Determine which SGF (if any) we'll load - check session state first
    auto& settings = UserSettings::instance();
    sgfToLoad.clear();
    sgfGameIndex = -1;
    sessionTreePathLength = 0;
    sessionTreePath.clear();
    sessionIsExternal = false;
    sessionTsumegoMode = false;
    sessionAnalysisMode = false;
    sessionRestoreNeeded = false;

    if (!settings.getStartFresh() && settings.hasSessionState()) {
        // Try session restoration
        std::string sessionFile = settings.getSessionFile();
        if (!sessionFile.empty() && std::filesystem::exists(sessionFile)) {
            sgfToLoad = sessionFile;
            sgfGameIndex = settings.getSessionGameIndex();
            sessionTreePathLength = settings.getSessionTreePathLength();
            sessionTreePath = settings.getSessionTreePath();
            sessionIsExternal = settings.getSessionIsExternal();
            sessionTsumegoMode = settings.getSessionTsumegoMode();
            sessionAnalysisMode = settings.getSessionAnalysisMode();
            sessionRestoreNeeded = true;
            spdlog::info("Session restoration: file={}, gameIndex={}, pathLen={}, branchChoices={}, tsumego={}, analysis={}",
                sessionFile, sgfGameIndex, sessionTreePathLength, sessionTreePath.size(), sessionTsumegoMode, sessionAnalysisMode);
        } else {
            spdlog::warn("Session file not found: {}, falling back to default loading", sessionFile);
            settings.clearSessionState();
        }
    }

    if (sgfToLoad.empty() && !settings.getStartFresh()) {
        // Fallback to old behavior
        std::string lastSgf = settings.getLastSgfPath();
        if (!lastSgf.empty() && std::filesystem::exists(lastSgf)) {
            sgfToLoad = lastSgf;
        } else {
            GameRecord tempRecord;
            std::string dailyFile = tempRecord.getDefaultFileName();
            if (std::filesystem::exists(dailyFile)) {
                sgfToLoad = dailyFile;
            }
        }
    }

    // Store initial board size from model (already set by determineInitialBoardSize())
    initialBoardSize = model.getBoardSize();
    spdlog::info("Initial board size: {}, SGF to load: {} (gameIndex={})", initialBoardSize,
                 sgfToLoad.empty() ? "(none)" : sgfToLoad, sgfGameIndex);

    // Start intro animation so board is visible and responsive during loading
    view.animateIntro();

    // Set default filename for SGF saving (only for daily session, not external files)
    if (!sgfToLoad.empty() && !sessionIsExternal) {
        model.game.setDefaultFileName(sgfToLoad);
    }

    // Load all engines in parallel - first ready engine loads SGF, rest sync
    // Start at root if: tsumego mode OR we have a session tree path to navigate to
    int gameIdx = sgfGameIndex;  // Capture for lambda
    bool loadAtRoot = sessionRestoreNeeded && (sessionTsumegoMode || sessionTreePathLength > 0);

    // Queue tree path navigation before starting engines. The command sits in the
    // queue until the game thread starts (inside loadEnginesParallel, as soon as
    // the coach engine is ready). Single unified path — always on the game thread.
    if (sessionRestoreNeeded && sessionTreePathLength > 0 && !sessionTsumegoMode) {
        engine.navigateToTreePath(sessionTreePathLength, sessionTreePath);
        spdlog::info("Session restore: queued tree path navigation ({} steps, {} branch choices)",
            sessionTreePathLength, sessionTreePath.size());
    }

    engineLoadFuture = std::async(std::launch::async, [this, gameIdx, loadAtRoot]() {
        engine.loadEnginesParallel(config, sgfToLoad, [this]() {
            // Called when first engine is ready and SGF is loaded
            stonesDisplayed = true;
            view.requestRepaint();
        }, gameIdx, loadAtRoot);
        spdlog::info("All engines loaded");
    });
}

void ElementGame::checkEngineLoadingComplete() {
    if (enginesLoaded) {
        return;
    }
    if (!engineLoadFuture.valid()) {
        return;  // Not started yet
    }

    // Check if future is ready (non-blocking)
    auto status = engineLoadFuture.wait_for(std::chrono::milliseconds(0));
    if (status == std::future_status::ready) {
        engineLoadFuture.get();  // void return
        enginesLoaded = true;

        spdlog::info("All engines ready, updating UI");
        // Nothing to clear: the loading text lives in #lblStatus, which
        // syncStatusIndicator() empties as soon as the pending list does, and it
        // never occupied the message box in the first place. That box is for
        // results, comments and prompts.

        // Perform deferred initialization if needed
        if (deferredInitNeeded && !deferredInitDone) {
            performDeferredInitialization();
        }

        view.requestRepaint();
    }
}

void ElementGame::performDeferredInitialization() {
    if (deferredInitDone) return;
    deferredInitDone = true;

    spdlog::info("Performing deferred initialization");

    // Clear startFresh flag if it was set
    if (UserSettings::instance().getStartFresh()) {
        spdlog::info("Starting fresh (board was cleared last session)");
        UserSettings::instance().setStartFresh(false);
    }

    // SGF was already loaded in loadEnginesParallel()
    if (!sgfToLoad.empty()) {
        // Session restoration: navigate to saved tree path
        // Verify current file matches session file (user may have loaded different file via dialog)
        bool fileMatches = model.game.getLoadedFilePath() == sgfToLoad;
        if (sessionRestoreNeeded && !fileMatches) {
            spdlog::warn("Session restore: skipping - file changed from {} to {}",
                sgfToLoad, model.game.getLoadedFilePath());
            sessionRestoreNeeded = false;
        }

        // Tree path navigation was queued in startAsyncEngineLoading() and already
        // processed by the game thread (started early, as soon as coach was ready).

        // Restore tsumego mode
        if (sessionRestoreNeeded && sessionTsumegoMode) {
            setTsumegoMode(true);
            spdlog::info("Session restore: tsumego mode enabled");
        }

        // Restore analysis mode
        if (sessionRestoreNeeded && sessionAnalysisMode) {
            engine.setGameMode(GameMode::ANALYSIS);
            spdlog::info("Session restore: analysis mode enabled");
        }

        // Bootstrap UserSettings from daily session if no game settings exist yet
        // This ensures "Nová hra" uses consistent settings instead of mixing defaults with session
        auto& settings = UserSettings::instance();
        if (sessionRestoreNeeded && !sessionIsExternal && !settings.hasGameSettings()) {
            auto players = engine.getPlayers();
            size_t blackIdx = engine.getActivePlayer(0);
            size_t whiteIdx = engine.getActivePlayer(1);
            std::string blackName = (blackIdx < players.size()) ? players[blackIdx]->getName() : "Human";
            std::string whiteName = (whiteIdx < players.size()) ? players[whiteIdx]->getName() : "Human";

            settings.setGameSettings(
                model.getBoardSize(),
                model.state.komi,
                model.state.handicap,
                blackName,
                whiteName);
            spdlog::info("Bootstrapped UserSettings from daily session: {}x{}, komi={}, handicap={}, players={}/{}",
                model.getBoardSize(), model.getBoardSize(), model.state.komi, model.state.handicap,
                blackName, whiteName);
        }

        refreshPlayerDropdowns();
        sessionRestoreNeeded = false;  // Consumed
    } else {
        // Apply user settings if no SGF was loaded
        auto& settings = UserSettings::instance();
        auto players = engine.getPlayers();

        auto findPlayer = [&players](const std::string& name) -> int {
            for (size_t i = 0; i < players.size(); i++) {
                if (players[i]->getName() == name) {
                    return static_cast<int>(i);
                }
            }
            return -1;
        };

        // When no game settings saved, keep PlayerManager defaults (human vs coach)
        std::string blackName = settings.hasGameSettings()
            ? settings.getBlackPlayer()
            : players[engine.getActivePlayer(0)]->getName();
        std::string whiteName = settings.hasGameSettings()
            ? settings.getWhitePlayer()
            : players[engine.getActivePlayer(1)]->getName();

        int blackIdx = findPlayer(blackName);
        int whiteIdx = findPlayer(whiteName);

        // Fallback if saved player not found (engine removed from config, or language changed)
        auto findHuman = [&players]() -> int {
            for (size_t i = 0; i < players.size(); i++) {
                if (players[i]->isTypeOf(Player::HUMAN)) return static_cast<int>(i);
            }
            return -1;
        };
        if (blackIdx < 0) {
            blackIdx = findHuman();
            spdlog::info("Saved black player '{}' not found, using '{}'",
                blackName, blackIdx >= 0 ? players[blackIdx]->getName() : "none");
        }
        if (whiteIdx < 0) {
            whiteIdx = findHuman();
            spdlog::info("Saved white player '{}' not found, using '{}'",
                whiteName, whiteIdx >= 0 ? players[whiteIdx]->getName() : "none");
        }

        if (blackIdx >= 0) control.switchPlayer(0, blackIdx);
        if (whiteIdx >= 0) control.switchPlayer(1, whiteIdx);

        refreshPlayerDropdowns();
    }

    control.finishInitialization();

    // Only now: the analysis role is resolved from the bot list, and nothing has
    // claimed it until the loader threads have finished. The thread starts here;
    // the *process* does not, and will not until the user asks for it.
    analysis.start();
    analysis.setEnabled(UserSettings::instance().getEvaluationEnabled());
    // Restored separately from the panel: it is a separate feature, and its
    // default is off so that turning the panel on never silently starts
    // pointing at the board.
    view.setAnalysisOverlay(UserSettings::instance().getEvaluationMoves());
    view.setEvaluationOnBoard(UserSettings::instance().getEvaluationOnBoard());
    if (auto align = GobanView::parseAlign(UserSettings::instance().getEvaluationAlign())) {
        view.setEvaluationAlign(*align);
    }
    // Over the config default the view already read, and only if chosen.
    const std::string ink = UserSettings::instance().getEvaluationColor();
    if (!ink.empty()) {
        if (auto parsed = parseHexColor(ink)) {
            view.setReadoutColor(*parsed);
        }
    }

    // Invalidate view state to force OnUpdate to sync all dropdowns
    // (model and view start with identical defaults, so diffs won't fire otherwise)
    view.state.komi = -1.0f;
    view.state.handicap = -1;
    view.state.boardSize = -1;
    view.state.black.clear();
    view.state.white.clear();

    view.requestRepaint();
}

/// The status indicator: what is loading, or how many messages want attention.
///
/// This used to write "Loading engines..." into lblMessage — unnamed, in English
/// regardless of the interface language, and in the box that also carries game
/// results, SGF comments and confirmation prompts. It now drives #lblStatus,
/// which is nobody else's, and names the engine: with two engines configured,
/// "still loading" says nothing about which one is wedged, and a CPU KataGo can
/// hold that state for a minute while every action is correctly greyed out and
/// indistinguishable from a broken program.
void ElementGame::syncStatusIndicator() {
    auto context = GetContext();
    if (!context) return;
    auto doc = context->GetDocument("game_window");
    if (!doc) return;
    auto status = doc->GetElementById("lblStatus");
    if (!status) return;   // a translated .rml without the element degrades to silence

    auto& log = MessageLog::instance();
    std::string loading = enginesLoaded ? std::string() : engine.engineLoadingSummary();
    // The analysis engine cannot use the branch above: it starts lazily, long
    // after `enginesLoaded` has flipped, and loading a second set of network
    // weights takes just as long. Same template — "Loading <engine>…" is exactly
    // what is happening, and it needs no new string in five languages.
    if (loading.empty()) {
        loading = analysis.startingEngineName();
    }

    std::string text;
    const char* severityClass = nullptr;
    if (logPanelOpen) {
        // While the panel is open this line is the only way to close it again,
        // so it must always be present. It was not: opening marked the messages
        // seen, which emptied the badge, which hid the element — leaving the
        // panel up with nothing to click but a menu nobody thinks to look in.
        text = getTemplateText(context, "tplStatusMessages");
        severityClass = "open";
    } else if (!loading.empty()) {
        text = Rml::CreateString(getTemplateText(context, "tplStatusLoading").c_str(),
                                 loading.c_str()).c_str();
        severityClass = "loading";
    } else if (log.hasUnseen()) {
        // Messages since the panel was last opened, not the size of the buffer.
        // The count is parenthesised rather than agreeing with a noun: Czech
        // needs three plural forms (1 zpráva / 2 zprávy / 5 zpráv), Japanese,
        // Korean and Chinese have none, and "Zprávy (3)" is correct in all five
        // without a plural-rules library.
        const bool isError = log.unseenSeverity() == MessageSeverity::Error;
        text = Rml::CreateString(
            getTemplateText(context, isError ? "tplStatusError" : "tplStatusWarning").c_str(),
            static_cast<int>(log.unseenCount())).c_str();
        severityClass = isError ? "error" : "warning";
    }

    // Open beats loading beats the badge. Loading over the badge is deliberate:
    // during startup the warnings that raise it are usually about the very
    // engines still loading, and it waits to be shown the moment loading ends.
    const bool changed = (text != statusTextShown);
    if (changed) {
        status->SetInnerRML(text.c_str());
        statusTextShown = text;
    }
    for (const char* c : {"loading", "warning", "error", "open"}) {
        const bool want = severityClass && std::string(c) == severityClass;
        if (status->IsClassSet(c) != want) {
            status->SetClass(c, want);
        }
    }
    if (changed) {
        view.requestRepaint();
    }

    if (logPanelOpen && log.version() != logVersionShown) {
        rebuildLogPanel();
    }
}

/// Rebuilds #lstLog from the buffer. Only called when the panel is open and the
/// log has actually changed — the version counter is an atomic, so the common
/// case of an open panel and a quiet log costs one relaxed load per frame.
void ElementGame::rebuildLogPanel() {
    auto context = GetContext();
    if (!context) return;
    auto doc = context->GetDocument("game_window");
    if (!doc) return;
    auto list = doc->GetElementById("lstLog");
    if (!list) return;

    auto& log = MessageLog::instance();
    const auto entries = log.entries();
    logVersionShown = log.version();
    // The panel is on screen showing these, so they are seen by definition.
    // Without this, messages arriving while it is open would still be counted as
    // new, and closing it would raise a badge for entries already read.
    log.markSeen();

    // Newest first. The alternative — oldest first, scrolled to the bottom —
    // needs a layout pass before SetScrollTop() means anything, so the panel
    // opened showing the OpenGL and font-loading chatter from startup while the
    // error the user opened it for sat off-screen. Reversing costs nothing and
    // needs no scrolling at all for the common case.
    Rml::String rml;
    for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
        const auto& e = *it;
        const char* cls = e.severity == MessageSeverity::Error   ? "log-error"
                        : e.severity == MessageSeverity::Warning ? "log-warning"
                                                                 : "log-info";
        // The text comes from spdlog, so it can contain anything an engine put
        // on stderr. Escape it rather than letting a stray '<' eat the panel.
        rml += Rml::CreateString("<div class=\"%s\">%s  %s</div>",
                                 cls, e.timestamp.c_str(), escapeRml(e.text).c_str());
    }
    list->SetInnerRML(rml);
    view.requestRepaint();
}

void ElementGame::toggleLogPanel() {
    setLogPanelOpen(!logPanelOpen);
}

void ElementGame::setLogPanelOpen(bool open) {
    auto context = GetContext();
    if (!context) return;
    auto doc = context->GetDocument("game_window");
    if (!doc) return;
    auto panel = doc->GetElementById("pnlLog");
    if (!panel) return;

    logPanelOpen = open;
    panel->SetClass("hide", !open);
    panel->SetClass("show", open);
    if (open) {
        // Opening is what "seen" means. The entries stay; only the badge clears.
        MessageLog::instance().markSeen();
        logVersionShown = 0;   // force a rebuild, the buffer may have rolled
        rebuildLogPanel();
    }
    view.requestRepaint();
}

void ElementGame::clearLog() {
    MessageLog::instance().clear();
    logVersionShown = 0;
    if (logPanelOpen) rebuildLogPanel();
    view.requestRepaint();
}

void ElementGame::updateLoadingStatus(const std::string& message) {
    // Kept for the one caller that still wants a plain string in the message
    // box; engine progress goes through syncStatusIndicator() now.
    auto context = GetContext();
    if (!context) return;

    auto doc = context->GetDocument("game_window");
    if (!doc) return;

    auto msgLabel = doc->GetElementById("lblMessage");
    if (msgLabel) {
        msgLabel->SetInnerRML(message.c_str());
    }

    // Request repaint so the loading message is visible
    view.requestRepaint();
}

void ElementGame::cacheTsumegoHints() {
    auto context = GetContext();
    if (!context) return;
    model.tsumegoHintBlack = getTemplateText(context, "tplBlackToMove");
    model.tsumegoHintWhite = getTemplateText(context, "tplWhiteToMove");
}

/// Writes the four prisoner labels — the two board-corner counters and the two
/// in the Analysis menu — from one place.
///
/// It is one function because it was two, and they disagreed. `capturedBlack`
/// counts *black stones removed from the board* (`Board::updateCaptures`
/// increments it when the captured colour is black), so it is what **White** has
/// taken. The in-game branch had that right and the game-over branch had it
/// backwards, so every game's final position showed both counts swapped. Same
/// shape as the buttons that disagreed with their commands before ADR-0005: two
/// copies of a rule, one of them wrong.
void ElementGame::syncPrisonerLabels() {
    auto context = GetContext();
    if (!context) return;
    auto doc = context->GetDocument("game_window");
    if (!doc) return;

    const std::string whiteTpl = templateText("templatePrisonersWhite", "White: %d");
    const std::string blackTpl = templateText("templatePrisonersBlack", "Black: %d");

    // White's prisoners are the black stones taken, and vice versa.
    const int whiteHasTaken = model.state.capturedBlack;
    const int blackHasTaken = model.state.capturedWhite;

    for (const char* id : {"cntWhite", "lblPrisonersWhite"}) {
        if (auto* el = doc->GetElementById(id)) {
            el->SetInnerRML(Rml::CreateString(whiteTpl.c_str(), whiteHasTaken).c_str());
        }
    }
    for (const char* id : {"cntBlack", "lblPrisonersBlack"}) {
        if (auto* el = doc->GetElementById(id)) {
            el->SetInnerRML(Rml::CreateString(blackTpl.c_str(), blackHasTaken).c_str());
        }
    }
}

/// Writes the evaluation panel from the analysis thread's last report.
///
/// Two rules from ADR-0007 live here. **Never a placeholder** (decision 12): no
/// report means the panel is hidden outright, because a bar resting in the
/// middle because nothing has been computed cannot be told from a genuine even
/// game — the same mistake as reading a failed score as zero. And the one case
/// that is *stale* rather than absent — the numbers were true for a position
/// that has since been left, or the search has yielded to a playing engine — is
/// dimmed rather than blanked, since blanking would flicker once per move.
void ElementGame::syncEvaluationPanel() {
    auto context = GetContext();
    if (!context) return;
    auto doc = context->GetDocument("game_window");
    if (!doc) return;
    auto* panel = doc->GetElementById("grpAnalysis");
    if (!panel) return;   // a translated .rml without the element degrades to silence

    // One readout, in one place. With the board version on, the panel steps
    // aside rather than saying the same thing twice — which is the whole point
    // of the experiment: to see the diegetic one *instead of* the panel, not
    // beside it.
    const auto report = analysis.report();
    const bool show = report != nullptr && !view.isEvaluationOnBoard();
    if (panel->IsClassSet("show") != show) {
        panel->SetClass("show", show);
        panel->SetClass("hide", !show);
    }
    if (!show) return;

    const bool stale = analysis.state() != AnalysisState::Running
                       || report->positionId != model.snapshot()->positionId;
    if (panel->IsClassSet("stale") != stale) {
        panel->SetClass("stale", stale);
    }

    const int percent = static_cast<int>(std::lround(report->winrateBlack * 100.0));
    if (auto* fill = doc->GetElementById("barEvalFill")) {
        fill->SetProperty("width", Rml::CreateString("%d%%", percent).c_str());
    }
    if (auto* label = doc->GetElementById("lblEvalWinrate")) {
        label->SetInnerRML(Rml::CreateString(
            templateText("tplEvalWinrate", "B %d%%").c_str(), percent).c_str());
    }
    if (auto* label = doc->GetElementById("lblEvalScore")) {
        // Absent rather than zero: `lz-analyze` has no score at all, and 0.0 is
        // a legitimate result. Same distinction Engine::final_score() draws.
        if (report->scoreLeadBlack) {
            const double lead = *report->scoreLeadBlack;
            const char* id = lead >= 0.0 ? "tplEvalScoreBlack" : "tplEvalScoreWhite";
            const char* fallback = lead >= 0.0 ? "B+%.1f" : "W+%.1f";
            label->SetInnerRML(Rml::CreateString(
                templateText(id, fallback).c_str(), std::fabs(lead)).c_str());
        } else {
            label->SetInnerRML("");
        }
    }
}

std::string ElementGame::templateText(const char* id, const std::string& fallback) const {
    auto context = GetContext();
    if (!context) return fallback;
    const std::string text = getTemplateText(context, id);
    return text.empty() ? fallback : text;
}

void ElementGame::showMessage(const std::string& text) {
    // Don't overwrite active prompts (quit confirmation, clear board, etc.)
    if (hasActivePrompt()) return;

    auto context = GetContext();
    if (!context) return;

    auto doc = context->GetDocument("game_window");
    if (!doc) return;

    // Get template
    auto tpl = doc->GetElementById("tplMessage");
    if (!tpl) {
        spdlog::warn("Message template tplMessage not found");
        return;
    }

    // Build message from template
    std::string msgHtml = tpl->GetInnerRML().c_str();
    size_t pos = msgHtml.find("%MSG%");
    if (pos != std::string::npos) {
        msgHtml.replace(pos, 5, escapeRml(text));
    }

    // Set in lblMessage
    if (auto msgLabel = doc->GetElementById("lblMessage")) {
        msgLabel->SetInnerRML(msgHtml.c_str());
    }

    view.requestRepaint();
}

void ElementGame::showPromptYesNo(const std::string& message, std::function<void(bool)> callback) {
    auto context = GetContext();
    if (!context) return;

    auto doc = context->GetDocument("game_window");
    if (!doc) return;

    // Get template
    auto tpl = doc->GetElementById("tplPromptYesNo");
    if (!tpl) {
        spdlog::warn("Prompt template tplPromptYesNo not found");
        return;
    }

    // Build prompt from template
    std::string promptHtml = tpl->GetInnerRML().c_str();
    size_t pos = promptHtml.find("%MSG%");
    if (pos != std::string::npos) {
        promptHtml.replace(pos, 5, escapeRml(message));
    }

    // Set in lblMessage
    if (auto msgLabel = doc->GetElementById("lblMessage")) {
        msgLabel->SetInnerRML(promptHtml.c_str());
    }

    pendingPromptCallback = std::move(callback);
    view.requestRepaint();
}

void ElementGame::showPromptYesNoTemplate(const std::string& templateId, std::function<void(bool)> callback) {
    auto context = GetContext();
    if (!context) {
        // Fallback to template ID as message
        showPromptYesNo(templateId, std::move(callback));
        return;
    }
    std::string message = getTemplateText(context, templateId);
    if (message.empty()) {
        // Fallback to template ID if not found
        message = templateId;
    }
    showPromptYesNo(message, std::move(callback));
}

void ElementGame::showPromptOkCancel(const std::string& message, std::function<void(bool)> callback) {
    auto context = GetContext();
    if (!context) return;

    auto doc = context->GetDocument("game_window");
    if (!doc) return;

    // Get template
    auto tpl = doc->GetElementById("tplPromptOkCancel");
    if (!tpl) {
        spdlog::warn("Prompt template tplPromptOkCancel not found");
        return;
    }

    // Build prompt from template
    std::string promptHtml = tpl->GetInnerRML().c_str();
    size_t pos = promptHtml.find("%MSG%");
    if (pos != std::string::npos) {
        promptHtml.replace(pos, 5, escapeRml(message));
    }

    // Set in lblMessage
    if (auto msgLabel = doc->GetElementById("lblMessage")) {
        msgLabel->SetInnerRML(promptHtml.c_str());
    }

    pendingPromptCallback = std::move(callback);
    view.requestRepaint();
}

void ElementGame::handlePromptResponse(bool affirmative) {
    if (pendingPromptCallback) {
        auto callback = std::move(pendingPromptCallback);
        pendingPromptCallback = nullptr;
        callback(affirmative);
    }
    clearMessage();
}

void ElementGame::clearMessage() {
    pendingPromptCallback = nullptr;
    auto context = GetContext();
    if (!context) return;

    auto doc = context->GetDocument("game_window");
    if (!doc) return;

    if (auto msgLabel = doc->GetElementById("lblMessage")) {
        std::string current = msgLabel->GetInnerRML();
        if (!current.empty()) {
            spdlog::debug("clearMessage: clearing '{}'", current.substr(0, 40));
        }
        msgLabel->SetInnerRML("");
    }
    view.requestRepaint();
}

int ElementGame::determineInitialBoardSize() {
    auto& settings = UserSettings::instance();

    // If starting fresh, use settings board size
    if (settings.getStartFresh()) {
        return settings.getBoardSize();
    }

    // Check if there's a last SGF to resume
    std::string lastSgf = settings.getLastSgfPath();
    if (!lastSgf.empty() && std::filesystem::exists(lastSgf)) {
        int size = GameRecord::peekBoardSize(lastSgf);
        if (size > 0) {
            spdlog::debug("Peeked board size {} from last SGF: {}", size, lastSgf);
            return size;
        }
    }

    // Check for daily session file
    GameRecord tempRecord;
    std::string dailyFile = tempRecord.getDefaultFileName();
    if (std::filesystem::exists(dailyFile)) {
        int size = GameRecord::peekBoardSize(dailyFile);
        if (size > 0) {
            spdlog::debug("Peeked board size {} from daily file: {}", size, dailyFile);
            return size;
        }
    }

    // Fall back to user settings
    return settings.getBoardSize();
}

void ElementGame::ProcessEvent(Rml::Event& event)
{
    if (event == "mousemove") {
        spdlog::trace("ElementGame processes event: {}", event.GetType().c_str());
    } else {
        spdlog::debug("ElementGame processes event: {}", event.GetType().c_str());
    }

    // Repaint for non-mousemove events on UI elements (not game board)
    // Note: mouseover/mouseout are handled by global HoverRepaintListener in main.cpp
    if (event.GetTargetElement() != this && !(event == "mousemove")) {
        view.requestRepaint();
    }

    if (event == "keydown" || event == "keyup") {
        Rml::Input::KeyIdentifier key_identifier = static_cast<Rml::Input::KeyIdentifier>(event.GetParameter<int>("key_identifier", 0));
        // RmlUi reports the modifier state on the event itself. Without it the
        // keybinding table could only express bare keys, and every unmodified
        // letter was already taken by the camera and shader controls.
        unsigned mods = KeyMod::NONE;
        if (event.GetParameter<int>("ctrl_key", 0))  mods |= KeyMod::CTRL;
        if (event.GetParameter<int>("shift_key", 0)) mods |= KeyMod::SHIFT;
        if (event.GetParameter<int>("alt_key", 0))   mods |= KeyMod::ALT;
        spdlog::debug("ElementGame received {} key={} mods={}",
                      event.GetType().c_str(), static_cast<int>(key_identifier), mods);
        control.keyPress(key_identifier, mods, event == "keydown");
    }
    else if (event == "mousemove") {
        int x = event.GetParameter<int>("mouse_x", -1);
        int y = event.GetParameter<int>("mouse_y", -1);
        control.mouseMove(x, y);
    }
    else if (event == "mousescroll") {
        int x = event.GetParameter<int>("mouse_x", -1);
        int y = event.GetParameter<int>("mouse_y", -1);
        // RmlUi uses wheel_delta_y for vertical scroll
        float deltaY = event.GetParameter<float>("wheel_delta_y", 0.0f);
        control.mouseClick((deltaY < 0 ? 3 : 4), 1, x, y);
    }
    else if (event == "resize") {
        view.requestRepaint();
    }
    if (event == "load")
    {
        spdlog::debug("Load event - initializing UI elements");

        // Populate engine-independent UI elements immediately (shaders, languages, toggles)
        populateUIElements();

        // Mark that we need to do initialization once engines are ready
        deferredInitNeeded = true;

        // Start async engine loading (will show "Loading engines..." status)
        startAsyncEngineLoading();
    }
}

/// Greys out what cannot be done right now. The rules live in
/// availableActions() (goban_core, unit-tested); GobanControl gathers the
/// inputs. Both this and the command guards read the same answer, which is what
/// stops a button and its keybinding disagreeing — they did, twice, before
/// ADR-0002 step 5 made it one expression.
void ElementGame::syncActionAvailability() {
    const UiActions a = control.actions();
    setElementDisabled("cmdStart",      !a.start);
    setElementDisabled("cmdPass",       !a.pass);
    setElementDisabled("cmdResign",     !a.resign);
    setElementDisabled("cmdUndo",       !a.undo);
    setElementDisabled("cmdKibitz",     !a.kibitz);
    setElementDisabled("cmdNavStart",   !a.navigate);
    setElementDisabled("cmdNavBack",    !a.navigate);
    setElementDisabled("cmdNavForward", !a.navigate);
    setElementDisabled("cmdNavEnd",     !a.navigate);
    setElementDisabled("cmdTerritory",  !a.territory);
    setElementDisabled("cmdClear",      !a.clear);
    setElementDisabled("cmdSave",       !a.save);
    setElementDisabled("cmdEvaluation", !a.evaluation);
    // Same answer, two buttons: the board annotations need exactly what the
    // panel needs — an engine that can analyse, and not a tsumego.
    setElementDisabled("cmdEvaluationMoves", !a.evaluation);
    setElementDisabled("cmdEvaluationBoard", !a.evaluation);
}

void ElementGame::OnUpdate()
{
    if(!view.gobanShader.isReady())
        return;

    //view.board.setStoneRadius(2.0f * model.metrics.stoneRadius / model.metrics.squareSizeX);
    view.board.updateMetrics(model.metrics);

    // The lifecycle phase, not state.reason. The two diverge after navigating
    // back from a finished game: the phase returns to Paused but the reason
    // stays set, which used to grey out Start, Pass, Undo and Kibitz on a
    // position that is no longer the end of the game — while the keybindings
    // for those same actions kept working.
    bool isOver = model.phase() == GamePhase::Finished;
    bool isRunning = engine.isRunning();

    Rml::Context* context = GetContext();

    std::string gameState(!isOver && isRunning ? "1" : (isOver ? "2" : "4"));
    model.state.cmd = gameState;
    if(model.state.cmd != view.state.cmd) {
        // Both grpGame and grpMoves are always visible; individual items are disabled as needed
        requestRepaint();
        view.state.cmd = gameState;
    }

    // Sync territory menu toggle with model state (auto-territory, T key, navigation)
    {
        std::vector<Rml::Element*> els;
        context->GetDocument("game_window")->GetElementsByClassName(els, "toggle_territory");
        if (!els.empty() && els[0]->IsClassSet("selected") != model.board.showTerritory) {
            OnMenuToggle("toggle_territory", model.board.showTerritory);
        }
    }
    // Sync the evaluation toggle. It can move without the menu being touched:
    // the setting is restored at startup, and the service switches itself off
    // when an engine turns out to be incapable.
    {
        auto* cmdEl = context->GetDocument("game_window")->GetElementById("cmdEvaluation");
        const bool checked = analysis.isEnabled();
        if (cmdEl && cmdEl->IsClassSet("selected") != checked) {
            OnMenuToggle("toggle_evaluation", checked);
        }
        auto* movesEl = context->GetDocument("game_window")->GetElementById("cmdEvaluationMoves");
        const bool movesChecked = view.isAnalysisOverlayShown();
        if (movesEl && movesEl->IsClassSet("selected") != movesChecked) {
            OnMenuToggle("toggle_evaluation_moves", movesChecked);
        }
        auto* boardEl = context->GetDocument("game_window")->GetElementById("cmdEvaluationBoard");
        const bool boardChecked = view.isEvaluationOnBoard();
        if (boardEl && boardEl->IsClassSet("selected") != boardChecked) {
            OnMenuToggle("toggle_evaluation_board", boardChecked);
        }
    }
    syncEvaluationPanel();

    // Sync game mode menu toggle with engine state (analysis or tsumego)
    {
        auto doc = context->GetDocument("game_window");
        auto* cmdEl = doc->GetElementById("cmdAnalysisMode");
        bool tsumego = view.isTsumegoMode();
        bool analysisMode = engine.getGameMode() == GameMode::ANALYSIS;
        bool checked = tsumego || analysisMode;
        if (cmdEl && cmdEl->IsClassSet("selected") != checked) {
            OnMenuToggle("toggle_analysis_mode", checked);
        }
        // Swap label between analysis and tsumego mode
        if (cmdEl) {
            const char* tplId = tsumego ? "tplTsumegoMode" : "tplAnalysisMode";
            if (auto* tpl = doc->GetElementById(tplId)) {
                Rml::String current = cmdEl->GetInnerRML();
                Rml::String target = tpl->GetInnerRML();
                if (current != target) {
                    cmdEl->SetInnerRML(target);
                }
            }
        }
    }
    // Sync overlay menu toggles with view state
    {
        std::vector<Rml::Element*> els;
        context->GetDocument("game_window")->GetElementsByClassName(els, "toggle_last_move_overlay");
        if (!els.empty() && els[0]->IsClassSet("selected") != view.showLastMoveOverlay) {
            OnMenuToggle("toggle_last_move_overlay", view.showLastMoveOverlay);
        }
    }
    {
        std::vector<Rml::Element*> els;
        context->GetDocument("game_window")->GetElementsByClassName(els, "toggle_next_move_overlay");
        if (!els.empty() && els[0]->IsClassSet("selected") != view.showNextMoveOverlay) {
            OnMenuToggle("toggle_next_move_overlay", view.showNextMoveOverlay);
        }
    }

    syncActionAvailability();
    syncStatusIndicator();

    if (view.state.colorToMove != model.state.colorToMove) {
        bool blackMove = model.state.colorToMove == Color::BLACK;
        // Update player select dropdown toggle indicators
        OnMenuToggle("toggle_black_player", blackMove);
        OnMenuToggle("toggle_white_player", !blackMove);
        view.state.colorToMove = model.state.colorToMove;
        requestRepaint();
    }
    if ((view.state.capturedBlack != model.state.capturedBlack)
        || (view.state.capturedWhite != model.state.capturedWhite) /*stones captured */
        || (view.state.reason != GameState::NO_REASON && model.state.reason == GameState::NO_REASON) /* new game */)
    {
        syncPrisonerLabels();
        requestRepaint();

        view.state.capturedBlack = model.state.capturedBlack;
        view.state.capturedWhite = model.state.capturedWhite;
        view.state.reason = model.state.reason;
    }
    if(view.state.reservoirBlack != model.state.reservoirBlack
        || view.state.reservoirWhite != model.state.reservoirWhite) {
        view.state.reservoirBlack = model.state.reservoirBlack;
        view.state.reservoirWhite = model.state.reservoirWhite;
        requestRepaint();
    }
    if (view.state.handicap != model.state.handicap) {
        auto doc = context->GetDocument("game_window");
        Rml::Element* hand = doc->GetElementById("lblHandicap");
        if (hand != nullptr) {
            hand->SetInnerRML(Rml::CreateString(
                templateText("templateHandicap", "Handicap: %d").c_str(),
                model.state.handicap).c_str());
            requestRepaint();
        }
        syncDropdown(doc, "selectHandicap", std::to_string(model.state.handicap));
        view.state.handicap = model.state.handicap;
    }
    if (view.state.komi != model.state.komi) {
        auto doc = context->GetDocument("game_window");
        Rml::Element* elKomi = doc->GetElementById("lblKomi");
        if (elKomi != nullptr) {
            elKomi->SetInnerRML(Rml::CreateString(
                templateText("templateKomi", "Komi: %.1f").c_str(),
                model.state.komi).c_str());
            requestRepaint();
        }
        std::ostringstream komiStr;
        komiStr << model.state.komi;
        syncDropdown(doc, "selectKomi", komiStr.str());
        view.state.komi = model.state.komi;
    }
    if (view.state.boardSize != model.state.boardSize) {
        auto doc = context->GetDocument("game_window");
        syncDropdown(doc, "selBoard", std::to_string(model.state.boardSize));
        view.state.boardSize = model.state.boardSize;
    }
    if (view.state.black != model.state.black) {
        auto doc = context->GetDocument("game_window");
        syncDropdown(doc, "selectBlack", std::to_string(engine.getActivePlayer(0)));
        view.state.black = model.state.black;
    }
    if (view.state.white != model.state.white) {
        auto doc = context->GetDocument("game_window");
        syncDropdown(doc, "selectWhite", std::to_string(engine.getActivePlayer(1)));
        view.state.white = model.state.white;
    }
    // Check if current message is important (game-related, should override engine messages)
    auto isImportantMessage = [](GameState::Message msg) {
        return msg == GameState::WHITE_WON || msg == GameState::BLACK_WON ||
               msg == GameState::WHITE_RESIGNED || msg == GameState::BLACK_RESIGNED ||
               msg == GameState::BLACK_PASS || msg == GameState::WHITE_PASS ||
               msg == GameState::CALCULATING_SCORE || msg == GameState::SCORING_FAILED ||
               msg == GameState::TSUMEGO_SOLVED || msg == GameState::TSUMEGO_WRONG;
    };

    bool msgChanged = view.state.msg != model.state.msg;
    bool posChanged = view.board.positionNumber.load() != model.board.positionNumber.load();

    // Only read the comment when the position changed. The atomic positionNumber
    // makes the game thread's write visible, but visibility was never the whole
    // problem: a second navigation while this copies the string is a race on the
    // string itself. The published snapshot is immutable, so the copy is safe
    // whatever the game thread does next. See ADR-0006 stage 3.
    std::string commentSnapshot = view.state.comment;
    if (posChanged) {
        commentSnapshot = model.snapshot()->comment;
    }
    if (msgChanged || posChanged) {
        switch (model.state.msg) {
        case GameState::CALCULATING_SCORE:
            showMessage(getTemplateText(context, "templateCalculatingScore"));
            break;
        case GameState::SCORING_FAILED:
            showMessage(Rml::CreateString(
                templateText("tplScoringFailed", "Scoring failed: %s").c_str(),
                model.state.scoringError.c_str()).c_str());
            break;
        case GameState::BLACK_RESIGNS:
            showMessage(getTemplateText(context, "templateBlackResigns"));
            break;
        case GameState::WHITE_RESIGNS:
            showMessage(getTemplateText(context, "templateWhiteResigns"));
            break;
        case GameState::BLACK_RESIGNED: {
            std::string msg = getTemplateText(context, "templateResignWhiteWon");
            if (!commentSnapshot.empty()) msg += "\n\n" + commentSnapshot;
            showMessage(msg);
            break;
        }
        case GameState::WHITE_RESIGNED: {
            std::string msg = getTemplateText(context, "templateResignBlackWon");
            if (!commentSnapshot.empty()) msg += "\n\n" + commentSnapshot;
            showMessage(msg);
            break;
        }
        case GameState::BLACK_PASS: {
            std::string msg = getTemplateText(context, "templateBlackPasses");
            auto pos = msg.find("{0}");
            if (pos != std::string::npos)
                msg.replace(pos, 3, model.state.passVariationLabel);
            showMessage(msg);
            break;
        }
        case GameState::WHITE_PASS: {
            std::string msg = getTemplateText(context, "templateWhitePasses");
            auto pos = msg.find("{0}");
            if (pos != std::string::npos)
                msg.replace(pos, 3, model.state.passVariationLabel);
            showMessage(msg);
            break;
        }
        case GameState::BLACK_WON:
        case GameState::WHITE_WON: {
            // The same call as the in-game branch, which is the point: this
            // branch had its own copy and gave cntWhite capturedWhite, so both
            // prisoner counts silently swapped on the last move of every game.
            syncPrisonerLabels();
            // Build result message, combining with SGF comment if present
            std::string resultMsg;
            if (model.state.winner == Color::WHITE)
                resultMsg = Rml::CreateString(
                    getTemplateText(context, "templateWhiteWon").c_str(),
                    std::abs(model.state.scoreDelta)).c_str();
            else
                resultMsg = Rml::CreateString(
                    getTemplateText(context, "templateBlackWon").c_str(),
                    std::abs(model.state.scoreDelta)).c_str();
            // Append SGF comment if present (user-authored content takes priority)
            if (!commentSnapshot.empty()) {
                resultMsg += "\n\n" + commentSnapshot;
            }
            showMessage(resultMsg);
            view.state.reason = model.state.reason;
        }
            break;
        case GameState::TSUMEGO_SOLVED:
            showMessage(getTemplateText(context, "tplTsumegoSolved"));
            if (msgChanged) view.playSound("correct", 1.0);
            break;
        case GameState::TSUMEGO_WRONG:
            showMessage(getTemplateText(context, "tplTsumegoWrong"));
            if (msgChanged) view.playSound("error", 0.5);
            break;
        default:
            clearMessage();
        }
        view.state.msg = model.state.msg;
        // Note: Don't store positionNumber here - let GobanView::Update() handle it
        // to ensure UPDATE_STONES flag is set before positionNumber is consumed
    }
    // Show SGF comment if available, but don't overwrite important game messages
    if (view.state.comment != commentSnapshot || posChanged) {
        spdlog::debug("Comment changed: '{}' -> '{}'", view.state.comment.substr(0, 30), commentSnapshot.substr(0, 30));
        if (!commentSnapshot.empty() && !isImportantMessage(model.state.msg)) {
            showMessage(commentSnapshot);
            // Scroll to bottom to show latest content
            if (auto msg = context->GetDocument("game_window")->GetElementById("lblMessage")) {
                msg->SetScrollTop(msg->GetScrollHeight() - msg->GetClientHeight());
            }
        } else if (!isImportantMessage(model.state.msg)) {
            clearMessage();
        }
        view.state.comment = commentSnapshot;
    }
    view.Update();
}

void ElementGame::Reshape() {
    Rml::Context* context = GetContext();
    Rml::Vector2i d = context->GetDimensions();
    if (WINDOW_HEIGHT != d.y || WINDOW_WIDTH != d.x) {
        // Note: RmlUi's StyleSheet API has changed significantly.
        // Dynamic font-size adjustment is disabled for now.
        // The font size could be adjusted via CSS calc() or data bindings in RmlUi.
        WINDOW_WIDTH = d.x;
        WINDOW_HEIGHT = d.y;
        spdlog::debug("ElementGame::Reshape - context: {}x{}", d.x, d.y);
    }
}

void ElementGame::OnMenuToggle(const std::string &cmd, bool checked) const {
    if(cmd.substr(0, 7) == "toggle_") {
        std::vector<Rml::Element*> elements;
        GetContext()->GetDocument("game_window")->GetElementsByClassName(elements, cmd.c_str());
        for(auto el: elements) {
            el->SetClass("selected", checked);
            el->SetClass("unselected", !checked);
        }
    }
}

void ElementGame::setElementDisabled(const std::string& elementId, bool disabled) const {
    auto* el = GetContext()->GetDocument("game_window")->GetElementById(elementId);
    if (el && el->IsClassSet("disabled") != disabled) {
        el->SetClass("disabled", disabled);
    }
}

void ElementGame::OnRender()
{
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    // Enable depth test for 3D rendering (main loop disables it for RmlUi)
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    Reshape();
    view.Render(WINDOW_WIDTH, WINDOW_HEIGHT);
    glPopAttrib();
}

void ElementGame::OnChildAdd(Rml::Element* element)
{
    Rml::Element::OnChildAdd(element);

    if (element == this) {
        GetOwnerDocument()->AddEventListener("load", this);
        GetOwnerDocument()->AddEventListener("mousemove", this);
        GetOwnerDocument()->AddEventListener("mousescroll", this);
        GetOwnerDocument()->AddEventListener("keydown", this);
        GetOwnerDocument()->AddEventListener("keyup", this);
        // Note: mouseover/mouseout handled by global HoverRepaintListener in main.cpp
    }
}
