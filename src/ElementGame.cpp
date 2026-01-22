#include "ElementGame.h"
#include "AppState.h"
#include "UserSettings.h"
#include "version.h"
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core.h>
#include <RmlUi/Core/StringUtilities.h>
#include <GLFW/glfw3.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <filesystem>
#include <fstream>

ElementGame::ElementGame(const Rml::String& tag)
        : Rml::Element(tag), model(this), view(model), engine(model),
          control(this, model, view, engine)
{
    engine.loadEngines(config);
    engine.addGameObserver(&model);
    engine.addGameObserver(&view);

    engine.clearGame(19, 0.5, 0);
    model.createNewRecord();
    // Player initialization moved to "load" event (from SGF or user settings)
}

void ElementGame::populateEngines() {
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
        for (unsigned i = 0; i < players.size(); ++i) {
            std::ostringstream ss;
            ss << i;
            std::string playerName(players[i]->getName());
            std::string playerIndex(ss.str());
            selectBlack->Add(playerName.c_str(), playerIndex.c_str());
            selectWhite->Add(playerName.c_str(), playerIndex.c_str());
        }
        selectBlack->SetSelection((int)players.size() - 1);
        selectWhite->SetSelection(0);
    }

    auto selectShader = dynamic_cast<Rml::ElementFormControlSelect*>(
            GetContext()->GetDocument("game_window")->GetElementById("selectShader"));

    if(!selectShader) {
        spdlog::warn("missing GUI element [selectShader]");
        return;
    }

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

    // Populate languages from config/*.json files
    auto selectLanguage = dynamic_cast<Rml::ElementFormControlSelect*>(
            GetContext()->GetDocument("game_window")->GetElementById("selectLanguage"));

    if (!selectLanguage) {
        spdlog::warn("missing GUI element [selectLanguage]");
        return;
    }

    namespace fs = std::filesystem;
    std::string configDir = "./config";

    // Get current language from config's gui path (e.g., "./config/gui/zh" -> "zh")
    std::string currentGui = config->data.value("gui", "./config/gui/en");
    std::string currentLang = fs::path(currentGui).filename().string();
    int currentLangIndex = -1;
    int index = 0;

    try {
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

    // Sync fullscreen menu state with restored state
    OnMenuToggle("toggle_fullscreen", AppState::IsFullscreen());

    // Sync sound state with user settings
    bool soundEnabled = UserSettings::instance().getSoundEnabled();
    view.player.setMuted(!soundEnabled);
    OnMenuToggle("toggle_sound", soundEnabled);

    // Sync FPS/VSync toggle state (MAX_FPS defaults to false = event-driven)
    OnMenuToggle("toggle_fps", view.isFpsLimitEnabled());

    // Populate version label (uses its own content as format string)
    auto versionLabel = GetContext()->GetDocument("game_window")->GetElementById("lblVersion");
    if (versionLabel) {
        versionLabel->SetInnerRML(
            Rml::CreateString(versionLabel->GetInnerRML().c_str(), GOBAN_VERSION).c_str()
        );
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

    // Save target selections BEFORE clearing (Remove() triggers change events that modify activePlayer)
    const auto targetBlack = static_cast<int>(engine.getActivePlayer(0));
    const auto targetWhite = static_cast<int>(engine.getActivePlayer(1));

    // Clear existing options (remove from end to avoid index shifting)
    while (selectBlack->GetNumOptions() > 0) {
        selectBlack->Remove(selectBlack->GetNumOptions() - 1);
    }
    while (selectWhite->GetNumOptions() > 0) {
        selectWhite->Remove(selectWhite->GetNumOptions() - 1);
    }

    // Repopulate from current players list
    const auto players = engine.getPlayers();
    for (unsigned i = 0; i < players.size(); ++i) {
        std::ostringstream ss;
        ss << i;
        std::string playerName(players[i]->getName());
        std::string playerIndex(ss.str());
        selectBlack->Add(playerName.c_str(), playerIndex.c_str());
        selectWhite->Add(playerName.c_str(), playerIndex.c_str());
    }

    // Set selection using saved values
    selectBlack->SetSelection(targetBlack);
    selectWhite->SetSelection(targetWhite);

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
    auto context = GetContext();

    float currentTime = static_cast<float>(glfwGetTime());
    if (currentTime - s_fpsLastTime >= 1.0) {
        auto debugElement = context->GetDocument("game_window")->GetElementById("lblFPS");
        auto fpsTemplate = context->GetDocument("game_window")->GetElementById("templateFPS");
        const Rml::String sFps = Rml::CreateString(fpsTemplate->GetInnerRML().c_str(), (float)s_fpsFrames / (currentTime - s_fpsLastTime));
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

        // Release audio resources if stream finished playing
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
    // If we displayed non-zero FPS, we need one more wake-up to show "0 fps"
    if (s_fpsLastDisplayed > 0) {
        float currentTime = static_cast<float>(glfwGetTime());
        double remaining = (s_fpsLastTime + 1.0) - currentTime;
        return (remaining > 0) ? remaining : 0.001;  // Small positive to wake immediately if overdue
    }
    return -1.0;  // Already showing 0 or never displayed - sleep forever
}

ElementGame::~ElementGame() = default;

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
        Rml::Input::KeyIdentifier key_identifier = (Rml::Input::KeyIdentifier) event.GetParameter< int >("key_identifier", 0);
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
        //control.Initialise();
        spdlog::debug("Load");
        populateEngines();

        // Resume last session (e.g., after language switch or archive)
        // Skip auto-loading if user cleared board before quitting
        bool sgfLoaded = false;
        if (UserSettings::instance().getStartFresh()) {
            spdlog::info("Starting fresh (board was cleared last session)");
            UserSettings::instance().setStartFresh(false);  // Clear flag after processing
        } else {
            std::string lastSgf = UserSettings::instance().getLastSgfPath();
            if (!lastSgf.empty()) {
                // Set defaultFileName BEFORE loading so path comparison in loadFromSGF succeeds
                // This ensures doc is preserved (not lost) when loading daily session across restarts
                model.game.setDefaultFileName(lastSgf);
                UserSettings::instance().setLastSgfPath("");  // Clear after processing

                if (std::filesystem::exists(lastSgf)) {
                    // File exists - load the game (SGF settings take precedence)
                    spdlog::info("Resuming last game from: {}", lastSgf);
                    engine.loadSGF(lastSgf, -1);  // Load last game in file
                    sgfLoaded = true;
                } else {
                    // File doesn't exist yet (new session after archive) - just set as active
                    spdlog::info("Starting new session: {}", lastSgf);
                }
            } else {
                // No explicit last SGF - check for today's daily session file
                std::string dailyFile = model.game.getDefaultFileName();
                if (std::filesystem::exists(dailyFile)) {
                    spdlog::info("Loading today's session file: {}", dailyFile);
                    engine.loadSGF(dailyFile, -1);  // Load last game in file
                    sgfLoaded = true;
                }
            }
        }

        if (sgfLoaded) {
            // Sync dropdowns with SGF game settings
            refreshPlayerDropdowns();
            refreshGameSettingsDropdowns();
        } else {
            // Apply user settings if no SGF was loaded
            auto& settings = UserSettings::instance();

            if (settings.hasGameSettings()) {
                spdlog::info("Applying saved game settings: {}x{}, komi={}, handicap={}",
                    settings.getBoardSize(), settings.getBoardSize(), settings.getKomi(), settings.getHandicap());

                // Apply board size, komi, handicap
                model.state.komi = settings.getKomi();
                model.state.handicap = settings.getHandicap();
                if (settings.getBoardSize() != 19) {
                    control.newGame(settings.getBoardSize());
                }
            }

            // Find and activate players by name (or defaults if no settings)
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
            spdlog::debug("Restoring players: black='{}' (idx={}), white='{}' (idx={})",
                blackName, blackIdx, whiteName, whiteIdx);
            if (blackIdx >= 0) {
                control.switchPlayer(0, blackIdx);
            }
            if (whiteIdx >= 0) {
                control.switchPlayer(1, whiteIdx);
            }

            // Sync dropdown UI with engine state after player settings applied
            refreshPlayerDropdowns();
            if (settings.hasGameSettings()) {
                refreshGameSettingsDropdowns();
            }
        }

        // Initialization complete - enable player settings persistence
        control.finishInitialization();
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
        auto cmdStart = context->GetDocument("game_window")->GetElementById("cmdStart");
        auto cmdClear = context->GetDocument("game_window")->GetElementById("cmdClear");
        auto grpMoves = context->GetDocument("game_window")->GetElementById("grpMoves");
        auto grpGame  = context->GetDocument("game_window")->GetElementById("grpGame");
        const Rml::String DISPLAY("display");
        bool gameInProgress = !isOver && isRunning;
        grpMoves->SetProperty(DISPLAY, gameInProgress ? "block" : "none");
        grpGame->SetProperty(DISPLAY, gameInProgress ? "none" : "block");
        if (!gameInProgress) {
            if (!isOver) {
                cmdClear->SetProperty(DISPLAY, "none");
                cmdStart->SetProperty(DISPLAY, "block");
            } else {
                cmdClear->SetProperty(DISPLAY, "block");
                cmdStart->SetProperty(DISPLAY, "none");
            }
        }
        requestRepaint();
        view.state.cmd = gameState;
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
    Rml::Element* msg = context->GetDocument("game_window")->GetElementById("lblMessage");
    // Update message when msg changes OR when a new game is loaded (detected by positionNumber change)
    if (view.state.msg != model.state.msg
        || view.board.positionNumber.load() != model.board.positionNumber.load()) {
        switch (model.state.msg) {
        case GameState::CALCULATING_SCORE:
            msg->SetInnerRML(
                context->GetDocument("game_window")
                    ->GetElementById("templateCalculatingScore")
                    ->GetInnerRML()
            );
            break;
        case GameState::BLACK_RESIGNS:
            msg->SetInnerRML(
                context->GetDocument("game_window")
                    ->GetElementById("templateBlackResigns")
                    ->GetInnerRML()
            );
            break;
        case GameState::WHITE_RESIGNS:
            msg->SetInnerRML(
                context->GetDocument("game_window")
                    ->GetElementById("templateWhiteReigns")
                    ->GetInnerRML()
            );
            break;
        case GameState::BLACK_RESIGNED:
            msg->SetInnerRML(
                      context->GetDocument("game_window")
                        ->GetElementById("templateResignWhiteWon")
                        ->GetInnerRML()
            );
            break;
        case GameState::WHITE_RESIGNED:
            msg->SetInnerRML(
                      context->GetDocument("game_window")
                        ->GetElementById("templateResignBlackWon")
                        ->GetInnerRML());
            break;
        case GameState::BLACK_PASS:
            msg->SetInnerRML(
                context->GetDocument("game_window")
                    ->GetElementById("templateBlackPasses")
                    ->GetInnerRML()
            );
            break;
        case GameState::WHITE_PASS:
            msg->SetInnerRML(
                context->GetDocument("game_window")
                    ->GetElementById("templateWhitePasses")
                    ->GetInnerRML()
            );
            break;
        case GameState::BLACK_WON:
        case GameState::WHITE_WON: {
            Rml::Element *elWhiteCnt = context->GetDocument("game_window")->GetElementById("cntWhite");
            Rml::Element *elBlackCnt = context->GetDocument("game_window")->GetElementById("cntBlack");
            // Show simplified captured stone counts (no detailed scoring breakdown)
            elWhiteCnt->SetInnerRML(
                    Rml::CreateString( "White captured: %d", model.state.capturedWhite).c_str());
            elBlackCnt->SetInnerRML(
                    Rml::CreateString( "Black captured: %d", model.state.capturedBlack).c_str());
            if (model.state.winner == Color::WHITE)
                msg->SetInnerRML(
                    Rml::CreateString(
                        context->GetDocument("game_window")
                        ->GetElementById("templateWhiteWon")
                        ->GetInnerRML().c_str(),
                    std::abs(model.state.scoreDelta)).c_str()
                );
            else
                msg->SetInnerRML(
                    Rml::CreateString(
                        context->GetDocument("game_window")
                        ->GetElementById("templateBlackWon")
                        ->GetInnerRML().c_str(),
                    std::abs(model.state.scoreDelta)).c_str()
                );
            view.state.reason = model.state.reason;
        }
            break;
        default:
            msg->SetInnerRML("");
        }
        requestRepaint();
        view.state.msg = model.state.msg;
        view.board.positionNumber.store(model.board.positionNumber.load());
    }
    // Show SGF comment if available (takes priority over other NONE-state content)
    if (view.state.comment != model.state.comment) {
        spdlog::debug("Comment changed: '{}' -> '{}'", view.state.comment.substr(0, 30), model.state.comment.substr(0, 30));
        if (!model.state.comment.empty()) {
            msg->SetInnerRML(model.state.comment.c_str());
            // Scroll to bottom to show latest content
            msg->SetScrollTop(msg->GetScrollHeight() - msg->GetClientHeight());
        } else if (view.state.msg == GameState::NONE) {
            msg->SetInnerRML("");
        }
        view.state.comment = model.state.comment;
        requestRepaint();
    }
    // Only show engine errors if no comment is displayed and msg is NONE
    else if(view.state.msg == GameState::NONE && model.state.comment.empty()) {
        for(auto &p: engine.getPlayers()) {
            if(p->isTypeOf(Player::ENGINE)) {
                std::string newMsg(dynamic_cast<GtpEngine*>(p)->lastError());
                if(!newMsg.empty() && newMsg != model.state.err) {
                    model.state.err = newMsg;
                }
            }
        }
        if(view.state.err != model.state.err) {
            msg->SetInnerRML(model.state.err.c_str());
            view.state.err = model.state.err;
            requestRepaint();
        }
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

void ElementGame::OnMenuToggle(const std::string &cmd, bool checked) {
    if(cmd.substr(0, 7) == "toggle_") {
        std::vector<Rml::Element*> elements;
        GetContext()->GetDocument("game_window")->GetElementsByClassName(elements, cmd.c_str());
        for(auto el: elements) {
            el->SetClass("selected", checked);
            el->SetClass("unselected", !checked);
        }
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
