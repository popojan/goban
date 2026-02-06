#include "ElementGame.h"
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
          model(this, determineInitialBoardSize()),
          view(model), engine(model),
          control(this, model, view, engine)
{
    // Register observers (doesn't require engines)
    engine.addGameObserver(&model);
    engine.addGameObserver(&view);

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
    control.setSyncingUI(true);

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

    control.setSyncingUI(false);

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

    // If we displayed non-zero FPS, we need one more wake-up to show "0 fps"
    if (s_fpsLastDisplayed > 0) {
        float currentTime = static_cast<float>(glfwGetTime());
        double remaining = (s_fpsLastTime + 1.0) - currentTime;
        return (remaining > 0) ? remaining : 0.001;  // Small positive to wake immediately if overdue
    }
    return -1.0;  // Already showing 0 or never displayed - sleep forever
}

ElementGame::~ElementGame() {
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
    updateLoadingStatus("Loading engines...");

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
        // Only clear loading message if no important game message is showing
        // (result messages like "Black won" should not be overwritten)
        auto msg = model.state.msg;
        bool isImportantMessage = msg == GameState::WHITE_WON || msg == GameState::BLACK_WON ||
                                  msg == GameState::WHITE_RESIGNED || msg == GameState::BLACK_RESIGNED ||
                                  msg == GameState::SCORING_FAILED;
        if (!isImportantMessage) {
            updateLoadingStatus("");  // Clear loading message
        }

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

    // Invalidate view state to force OnUpdate to sync all dropdowns
    // (model and view start with identical defaults, so diffs won't fire otherwise)
    view.state.komi = -1.0f;
    view.state.handicap = -1;
    view.state.boardSize = -1;
    view.state.black.clear();
    view.state.white.clear();

    view.requestRepaint();
}

void ElementGame::updateLoadingStatus(const std::string& message) {
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
        spdlog::debug("ElementGame received {} key={}", event.GetType().c_str(), static_cast<int>(key_identifier));
        control.keyPress(key_identifier, 0, 0, event == "keydown");
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

void ElementGame::OnUpdate()
{
    if(!view.gobanShader.isReady())
        return;

    //view.board.setStoneRadius(2.0f * model.metrics.stoneRadius / model.metrics.squareSizeX);
    view.board.updateMetrics(model.metrics);

    bool isOver = model.state.reason != GameState::NO_REASON;
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

    // Update disabled state for context-sensitive menu items
    {
        bool thinking = engine.isThinking();
        bool humanTurn = engine.humanToMove() && !thinking;
        bool hasMoves = model.game.moveCount() > 0
            || !model.setupBlackStones.empty() || !model.setupWhiteStones.empty()
            || model.game.getLoadedMovesCount() > 0;
        bool analysisMode = engine.getGameMode() == GameMode::ANALYSIS;
        // Bot-bot match detection: either explicit AI vs AI mode OR both players are engines
        bool botVsBot = engine.isAiVsAi() || engine.areBothPlayersEngines();
        // In bot-bot mode without analysis, most play actions are locked
        bool aiVsAiLocked = botVsBot && !analysisMode;

        // Start: enabled when current player is an engine (to trigger genmove)
        // Disabled when game over, already started, or current player is human
        bool engineToMove = engine.isCurrentPlayerEngine();
        setElementDisabled("cmdStart", isOver || model.started || !engineToMove);

        // Pass/Resign/Undo: disabled when not human's turn, game over, or locked in AI vs AI
        setElementDisabled("cmdPass", !humanTurn || isOver || aiVsAiLocked);
        setElementDisabled("cmdResign", !humanTurn || isOver || aiVsAiLocked);
        setElementDisabled("cmdUndo", !humanTurn || isOver || aiVsAiLocked);
        setElementDisabled("cmdKibitz", thinking || isOver || aiVsAiLocked);

        // Navigation: disabled when engine thinking or locked in AI vs AI
        bool navDisabled = thinking || aiVsAiLocked;
        setElementDisabled("cmdNavStart", navDisabled);
        setElementDisabled("cmdNavBack", navDisabled);
        setElementDisabled("cmdNavForward", navDisabled);
        setElementDisabled("cmdNavEnd", navDisabled);

        // Territory: disabled when game not over
        setElementDisabled("cmdTerritory", !model.isGameOver);

        // Clear: disabled when board is empty (no moves or loaded content)
        setElementDisabled("cmdClear", !hasMoves);

        // Save: disabled when no unsaved changes
        setElementDisabled("cmdSave", !model.game.hasUnsavedChanges());
    }

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
        Rml::Element* elWhiteCnt = context->GetDocument("game_window")->GetElementById("cntWhite");
        Rml::Element* elBlackCnt = context->GetDocument("game_window")->GetElementById("cntBlack");
        if (elWhiteCnt != nullptr) {
            elWhiteCnt->SetInnerRML(Rml::CreateString( "White: %d", model.state.capturedBlack).c_str());
            requestRepaint();
        }
        if (elBlackCnt != nullptr) {
            elBlackCnt->SetInnerRML(Rml::CreateString( "Black: %d", model.state.capturedWhite).c_str());
            requestRepaint();
        }

        // Update prisoner counts in Analysis menu
        Rml::Element* elPrisonersWhite = context->GetDocument("game_window")->GetElementById("lblPrisonersWhite");
        Rml::Element* elPrisonersBlack = context->GetDocument("game_window")->GetElementById("lblPrisonersBlack");
        if (elPrisonersWhite != nullptr) {
            auto templateWhite = context->GetDocument("game_window")->GetElementById("templatePrisonersWhite");
            if (templateWhite != nullptr) {
                elPrisonersWhite->SetInnerRML(
                    Rml::CreateString( templateWhite->GetInnerRML().c_str(), model.state.capturedBlack).c_str()
                );
            }
        }
        if (elPrisonersBlack != nullptr) {
            auto templateBlack = context->GetDocument("game_window")->GetElementById("templatePrisonersBlack");
            if (templateBlack != nullptr) {
                elPrisonersBlack->SetInnerRML(
                    Rml::CreateString( templateBlack->GetInnerRML().c_str(), model.state.capturedWhite).c_str()
                );
            }
        }

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
            hand->SetInnerRML(Rml::CreateString( "Handicap: %d", model.state.handicap).c_str());
            requestRepaint();
        }
        syncDropdown(doc, "selectHandicap", std::to_string(model.state.handicap));
        view.state.handicap = model.state.handicap;
    }
    if (view.state.komi != model.state.komi) {
        auto doc = context->GetDocument("game_window");
        Rml::Element* elKomi = doc->GetElementById("lblKomi");
        if (elKomi != nullptr) {
            elKomi->SetInnerRML(Rml::CreateString( "Komi: %.1f", model.state.komi).c_str());
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

    // Only read comment when position changed — atomic positionNumber ordering
    // guarantees the game thread's comment write is complete
    std::string commentSnapshot = view.state.comment;
    if (posChanged) {
        commentSnapshot = model.state.comment;
    }
    if (msgChanged || posChanged) {
        switch (model.state.msg) {
        case GameState::CALCULATING_SCORE:
            showMessage(getTemplateText(context, "templateCalculatingScore"));
            break;
        case GameState::SCORING_FAILED:
            showMessage("Scoring failed: " + model.state.scoringError);
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
            Rml::Element *elWhiteCnt = context->GetDocument("game_window")->GetElementById("cntWhite");
            Rml::Element *elBlackCnt = context->GetDocument("game_window")->GetElementById("cntBlack");
            // Show simplified captured stone counts (no detailed scoring breakdown)
            elWhiteCnt->SetInnerRML(
                    Rml::CreateString("White captured: %d", model.state.capturedWhite).c_str());
            elBlackCnt->SetInnerRML(
                    Rml::CreateString("Black captured: %d", model.state.capturedBlack).c_str());
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
