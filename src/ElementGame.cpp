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

    // Create initial game record (doesn't require engines)
    model.createNewRecord();

    // Engine loading is deferred to async thread - board renders immediately
    // Player initialization moved to "load" event (from SGF or user settings)
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
        std::string currentLang = fs::path(currentGui).filename().string();
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
                                std::string langFile = entry.path().stem().string();  // e.g., "en" from "en.json"
                                selectLanguage->Add(langName.c_str(), langFile.c_str());
                                spdlog::info("Found language: {} ({})", langName, langFile);
                                if (langFile == currentLang) {
                                    currentLangIndex = index;
                                }
                                index++;
                            }
                        } catch (const std::exception& e) {
                            spdlog::warn("Failed to parse {}: {}", entry.path().string(), e.what());
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

void ElementGame::populateEngines() {
    // Populate player dropdowns with all available players (requires engines loaded)
    auto selectBlack = dynamic_cast<Rml::ElementFormControlSelect*>(
            GetContext()->GetDocument("game_window")->GetElementById("selectBlack"));
    auto selectWhite = dynamic_cast<Rml::ElementFormControlSelect*>(
            GetContext()->GetDocument("game_window")->GetElementById("selectWhite"));
    const auto players(engine.getPlayers());

    if(!selectBlack) {
        spdlog::warn("missing GUI element [selectBlack]");
    }
    if(!selectWhite) {
        spdlog::warn("missing GUI element [selectWhite]");
    }

    if(selectBlack && selectWhite) {
        // Clear placeholder items
        while (selectBlack->GetNumOptions() > 0) selectBlack->Remove(0);
        while (selectWhite->GetNumOptions() > 0) selectWhite->Remove(0);

        for (unsigned i = 0; i < players.size(); ++i) {
            std::ostringstream ss;
            ss << i;
            std::string playerName(players[i]->getName());
            std::string playerIndex(ss.str());
            selectBlack->Add(playerName.c_str(), playerIndex.c_str());
            selectWhite->Add(playerName.c_str(), playerIndex.c_str());
        }
        selectBlack->SetSelection(static_cast<int>(players.size()) - 1);
        selectWhite->SetSelection(0);
    }
}

void ElementGame::refreshPlayerDropdowns() {
    auto selectBlack = dynamic_cast<Rml::ElementFormControlSelect*>(
            GetContext()->GetDocument("game_window")->GetElementById("selectBlack"));
    auto selectWhite = dynamic_cast<Rml::ElementFormControlSelect*>(
            GetContext()->GetDocument("game_window")->GetElementById("selectWhite"));

    if (!selectBlack || !selectWhite) {
        spdlog::warn("refreshPlayerDropdowns: missing dropdown elements");
        return;
    }

    // Save target selections BEFORE clearing
    const auto targetBlack = static_cast<int>(engine.getActivePlayer(0));
    const auto targetWhite = static_cast<int>(engine.getActivePlayer(1));

    // Suppress change events during repopulation to prevent transient
    // player switches (e.g. briefly activating an engine during clear)
    control.setSyncingUI(true);

    while (selectBlack->GetNumOptions() > 0) {
        selectBlack->Remove(selectBlack->GetNumOptions() - 1);
    }
    while (selectWhite->GetNumOptions() > 0) {
        selectWhite->Remove(selectWhite->GetNumOptions() - 1);
    }

    const auto players = engine.getPlayers();
    for (unsigned i = 0; i < players.size(); ++i) {
        std::ostringstream ss;
        ss << i;
        std::string playerName(players[i]->getName());
        std::string playerIndex(ss.str());
        selectBlack->Add(playerName.c_str(), playerIndex.c_str());
        selectWhite->Add(playerName.c_str(), playerIndex.c_str());
    }

    selectBlack->SetSelection(targetBlack);
    selectWhite->SetSelection(targetWhite);

    control.setSyncingUI(false);

    spdlog::debug("refreshPlayerDropdowns: {} players, black={}, white={}",
        players.size(), targetBlack, targetWhite);
}

void ElementGame::refreshGameSettingsDropdowns() {
    auto doc = GetContext()->GetDocument("game_window");
    if (!doc) return;

    // Helper to sync dropdown selection by value
    auto setSelectByValue = [doc](const char* elementId, const std::string& value) {
        auto select = dynamic_cast<Rml::ElementFormControlSelect*>(doc->GetElementById(elementId));
        if (!select) return;
        for (int i = 0; i < select->GetNumOptions(); i++) {
            if (select->GetOption(i)->GetAttribute("value", Rml::String()) == value) {
                select->SetSelection(i);
                break;
            }
        }
    };

    control.setSyncingUI(true);
    setSelectByValue("selBoard", std::to_string(model.getBoardSize()));
    std::ostringstream komiStr;
    komiStr << model.state.komi;
    setSelectByValue("selectKomi", komiStr.str());
    setSelectByValue("selectHandicap", std::to_string(model.state.handicap));
    control.setSyncingUI(false);

    spdlog::info("refreshGameSettingsDropdowns: board={}, komi={}, handicap={}",
        model.getBoardSize(), model.state.komi, model.state.handicap);
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

    // Determine which SGF (if any) we'll load - same logic as determineInitialBoardSize()
    auto& settings = UserSettings::instance();
    sgfToLoad.clear();

    if (!settings.getStartFresh()) {
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
    spdlog::info("Initial board size: {}, SGF to load: {}", initialBoardSize,
                 sgfToLoad.empty() ? "(none)" : sgfToLoad);

    // Start intro animation so board is visible and responsive during loading
    view.animateIntro();

    // Set default filename for SGF saving
    if (!sgfToLoad.empty()) {
        model.game.setDefaultFileName(sgfToLoad);
    }

    // Load all engines in parallel - first ready engine loads SGF, rest sync
    engineLoadFuture = std::async(std::launch::async, [this]() {
        engine.loadEnginesParallel(config, sgfToLoad, [this]() {
            // Called when first engine is ready and SGF is loaded
            stonesDisplayed = true;
            view.requestRepaint();
        });
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
                                  msg == GameState::WHITE_RESIGNED || msg == GameState::BLACK_RESIGNED;
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

    // Populate engine dropdowns
    populateEngines();

    // Clear startFresh flag if it was set
    if (UserSettings::instance().getStartFresh()) {
        spdlog::info("Starting fresh (board was cleared last session)");
        UserSettings::instance().setStartFresh(false);
    }

    // SGF was already loaded in loadEnginesParallel()
    if (!sgfToLoad.empty()) {
        refreshPlayerDropdowns();
        refreshGameSettingsDropdowns();
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

        std::string blackName = settings.hasGameSettings() ? settings.getBlackPlayer() : "Human";
        std::string whiteName = settings.hasGameSettings() ? settings.getWhitePlayer() : "Human";

        int blackIdx = findPlayer(blackName);
        int whiteIdx = findPlayer(whiteName);
        if (blackIdx >= 0) control.switchPlayer(0, blackIdx);
        if (whiteIdx >= 0) control.switchPlayer(1, whiteIdx);

        refreshPlayerDropdowns();
        if (settings.hasGameSettings()) {
            refreshGameSettingsDropdowns();
        }
    }

    control.finishInitialization();
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
    // Sync game mode menu toggle with engine state
    {
        std::vector<Rml::Element*> els;
        context->GetDocument("game_window")->GetElementsByClassName(els, "toggle_analysis_mode");
        bool analysisMode = engine.getGameMode() == GameMode::ANALYSIS;
        if (!els.empty() && els[0]->IsClassSet("selected") != analysisMode) {
            OnMenuToggle("toggle_analysis_mode", analysisMode);
        }
    }

    // Update disabled state for context-sensitive menu items
    {
        bool thinking = engine.isThinking();
        bool humanTurn = engine.humanToMove() && !thinking;
        bool hasMoves = model.game.moveCount() > 0
            || !model.setupBlackStones.empty() || !model.setupWhiteStones.empty();
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

        // Clear: disabled when board is empty (no moves made)
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
        Rml::Element* hand = context->GetDocument("game_window")->GetElementById("lblHandicap");
        if (hand != nullptr) {
            hand->SetInnerRML(Rml::CreateString( "Handicap: %d", model.state.handicap).c_str());
            requestRepaint();
            view.state.handicap = model.state.handicap;
        }
    }
    if (view.state.komi != model.state.komi) {
        Rml::Element* elKomi = context->GetDocument("game_window")->GetElementById("lblKomi");
        if (elKomi != nullptr) {
            elKomi->SetInnerRML(Rml::CreateString( "Komi: %.1f", model.state.komi).c_str());
            view.state.komi = model.state.komi;
            requestRepaint();
        }
    }
    // Collect engine errors FIRST (before message display, so we can combine them)
    std::string engineErrors;
    for (auto& p : engine.getPlayers()) {
        if (p->isTypeOf(Player::ENGINE)) {
            std::string err = dynamic_cast<GtpEngine*>(p)->lastError();
            if (!err.empty()) {
                if (!engineErrors.empty()) engineErrors += "\n";
                engineErrors += err;
            }
        }
    }

    // Helper to show message with engine errors prepended
    auto showWithErrors = [this, &engineErrors](const std::string& msg) {
        if (engineErrors.empty()) {
            showMessage(msg);
        } else if (msg.empty()) {
            showMessage(engineErrors);
        } else {
            showMessage(engineErrors + "\n" + msg);
        }
    };

    // Track if we need to refresh due to engine error changes
    bool errorsChanged = (engineErrors != model.state.err);
    if (errorsChanged) {
        model.state.err = engineErrors;
        view.state.err = engineErrors;
    }

    // Check if current message is important (game-related, should override engine messages)
    auto isImportantMessage = [](GameState::Message msg) {
        return msg == GameState::WHITE_WON || msg == GameState::BLACK_WON ||
               msg == GameState::WHITE_RESIGNED || msg == GameState::BLACK_RESIGNED ||
               msg == GameState::BLACK_PASS || msg == GameState::WHITE_PASS ||
               msg == GameState::CALCULATING_SCORE;
    };

    // Update message when msg changes or position changes
    // Don't let engine error changes override important messages
    bool msgChanged = view.state.msg != model.state.msg;
    bool posChanged = view.board.positionNumber.load() != model.board.positionNumber.load();
    bool shouldUpdateForErrors = errorsChanged && !isImportantMessage(model.state.msg);

    if (msgChanged || posChanged || shouldUpdateForErrors) {
        switch (model.state.msg) {
        case GameState::CALCULATING_SCORE:
            showMessage(getTemplateText(context, "templateCalculatingScore"));
            break;
        case GameState::BLACK_RESIGNS:
            showMessage(getTemplateText(context, "templateBlackResigns"));
            break;
        case GameState::WHITE_RESIGNS:
            showMessage(getTemplateText(context, "templateWhiteResigns"));
            break;
        case GameState::BLACK_RESIGNED: {
            std::string msg = getTemplateText(context, "templateResignWhiteWon");
            if (!model.state.comment.empty()) msg += "\n\n" + model.state.comment;
            showMessage(msg);
            break;
        }
        case GameState::WHITE_RESIGNED: {
            std::string msg = getTemplateText(context, "templateResignBlackWon");
            if (!model.state.comment.empty()) msg += "\n\n" + model.state.comment;
            showMessage(msg);
            break;
        }
        case GameState::BLACK_PASS: {
            std::string msg = getTemplateText(context, "templateBlackPasses");
            if (!model.state.passVariationLabel.empty())
                msg = model.state.passVariationLabel + " " + msg;
            showMessage(msg);
            break;
        }
        case GameState::WHITE_PASS: {
            std::string msg = getTemplateText(context, "templateWhitePasses");
            if (!model.state.passVariationLabel.empty())
                msg = model.state.passVariationLabel + " " + msg;
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
            if (!model.state.comment.empty()) {
                resultMsg += "\n\n" + model.state.comment;
            }
            showMessage(resultMsg);
            view.state.reason = model.state.reason;
        }
            break;
        default:
            if (!engineErrors.empty()) {
                showMessage(engineErrors);
            } else {
                clearMessage();
            }
        }
        view.state.msg = model.state.msg;
        // Note: Don't store positionNumber here - let GobanView::Update() handle it
        // to ensure UPDATE_STONES flag is set before positionNumber is consumed
    }
    // Show SGF comment if available, but don't overwrite important game messages
    if (view.state.comment != model.state.comment || errorsChanged) {
        spdlog::debug("Comment changed: '{}' -> '{}'", view.state.comment.substr(0, 30), model.state.comment.substr(0, 30));
        if (!model.state.comment.empty() && !isImportantMessage(model.state.msg)) {
            showWithErrors(model.state.comment);
            // Scroll to bottom to show latest content
            if (auto msg = context->GetDocument("game_window")->GetElementById("lblMessage")) {
                msg->SetScrollTop(msg->GetScrollHeight() - msg->GetClientHeight());
            }
        } else if (!isImportantMessage(model.state.msg)) {
            if (!engineErrors.empty()) {
                showMessage(engineErrors);
            } else {
                clearMessage();
            }
        }
        view.state.comment = model.state.comment;
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
