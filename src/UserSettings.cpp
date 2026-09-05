#include "UserSettings.h"
#include <filesystem>
#include <fstream>
#include <system_error>
#include <spdlog/spdlog.h>

UserSettings& UserSettings::instance() {
    static UserSettings instance;
    return instance;
}

void UserSettings::load() {
    std::lock_guard<std::mutex> lock(mutex);
    std::ifstream fin(settingsFile);
    if (!fin) {
        spdlog::debug("No user settings file found");
        return;
    }

    try {
        nlohmann::json user;
        fin >> user;
        fin.close();

        settingsLoaded = true;

        if (user.contains("last_config")) {
            lastConfig = user["last_config"].get<std::string>();
        }

        if (user.contains("fullscreen")) {
            fullscreen = user["fullscreen"].get<bool>();
        }

        if (user.contains("sound_enabled")) {
            soundEnabled = user["sound_enabled"].get<bool>();
        }

        if (user.contains("evaluation_enabled")) {
            evaluationEnabled = user["evaluation_enabled"].get<bool>();
        }

        if (user.contains("evaluation_moves")) {
            // Was a boolean before the three-state mode: true meant the
            // suggestions were always drawn, false that they never were.
            const auto& v = user["evaluation_moves"];
            if (v.is_boolean()) evaluationMoves = v.get<bool>() ? "always" : "off";
            else if (v.is_string()) evaluationMoves = v.get<std::string>();
        }

        if (user.contains("coordinates")) {
            coordinates = user["coordinates"].get<bool>();
        }

        if (user.contains("evaluation_readout")) {
            evaluationReadout = user["evaluation_readout"].get<bool>();
        }

        if (user.contains("wait_clock")) {
            waitClock = user["wait_clock"].get<bool>();
        }

        if (user.contains("evaluation_align")) {
            evaluationAlign = user["evaluation_align"].get<std::string>();
        }

        if (user.contains("evaluation_color")) {
            evaluationColor = user["evaluation_color"].get<std::string>();
        }

        if (user.contains("anaglyph")) {
            anaglyph = user["anaglyph"].get<std::string>();
        }

        if (user.contains("anaglyph_strength")) {
            anaglyphStrength = user["anaglyph_strength"].get<float>();
        }

        if (user.contains("anaglyph_leak") && user["anaglyph_leak"].is_array()
            && user["anaglyph_leak"].size() == 3) {
            anaglyphLeakR = user["anaglyph_leak"][0].get<float>();
            anaglyphLeakG = user["anaglyph_leak"][1].get<float>();
            anaglyphLeakB = user["anaglyph_leak"][2].get<float>();
            anaglyphLeakSet = true;
        }

        if (user.contains("anaglyph_balance") && user["anaglyph_balance"].is_array()
            && user["anaglyph_balance"].size() == 2) {
            anaglyphBalanceL = user["anaglyph_balance"][0].get<float>();
            anaglyphBalanceR = user["anaglyph_balance"][1].get<float>();
            anaglyphBalanceSet = true;
        }

        if (user.contains("glasses")) {
            glasses = user["glasses"].get<std::string>();
        }

        if (user.contains("anaglyph_green")) {
            anaglyphGreen = user["anaglyph_green"].get<float>();
        }

        if (user.contains("pointer_mode")) {
            pointerMode = user["pointer_mode"].get<std::string>();
        }
        if (user.contains("prisoner_mode") && user["prisoner_mode"].is_string()) {
            prisonerMode = user["prisoner_mode"].get<std::string>();
        }

        if (user.contains("coordinate_color")) {
            coordinateColor = user["coordinate_color"].get<std::string>();
        }

        if (user.contains("last_sgf_path")) {
            lastSgfPath = user["last_sgf_path"].get<std::string>();
        }

        if (user.contains("start_fresh")) {
            startFresh = user["start_fresh"].get<bool>();
        }

        if (user.contains("game")) {
            gameSettingsLoaded = true;
            auto& game = user["game"];
            boardSize = game.value("board_size", boardSize);
            komi = game.value("komi", komi);
            handicap = game.value("handicap", handicap);
            if (game.contains("black_player")) {
                blackPlayer = game["black_player"].get<std::string>();
            }
            if (game.contains("white_player")) {
                whitePlayer = game["white_player"].get<std::string>();
            }
        }

        if (user.contains("shader")) {
            shaderLoaded = true;
            auto& shader = user["shader"];
            if (shader.contains("name")) {
                shaderName = shader["name"].get<std::string>();
            }
            shaderEof = shader.value("eof", shaderEof);
            shaderDof = shader.value("dof", shaderDof);
            shaderGamma = shader.value("gamma", shaderGamma);
            shaderContrast = shader.value("contrast", shaderContrast);
        }

        if (user.contains("shader_params") && user["shader_params"].is_object()) {
            shaderParams = user["shader_params"];
        }

        // Helper to parse camera state from JSON
        auto parseCamera = [](const nlohmann::json& j, CameraState& cam) {
            if (j.contains("rotation")) {
                auto& rot = j["rotation"];
                cam.rotX = rot.value("x", cam.rotX);
                cam.rotY = rot.value("y", cam.rotY);
                cam.rotZ = rot.value("z", cam.rotZ);
                cam.rotW = rot.value("w", cam.rotW);
            }
            if (j.contains("pan")) {
                auto& pan = j["pan"];
                cam.panX = pan.value("x", cam.panX);
                cam.panY = pan.value("y", cam.panY);
            }
            cam.distance = j.value("distance", cam.distance);
        };

        if (user.contains("camera")) {
            savedCameraLoaded = true;
            parseCamera(user["camera"], savedCamera);
        }

        if (user.contains("camera_current")) {
            currentCameraLoaded = true;
            parseCamera(user["camera_current"], currentCamera);
        }

        if (user.contains("session")) {
            auto& session = user["session"];
            if (session.contains("file")) {
                sessionFile = session["file"].get<std::string>();
            }
            sessionGameIndex = session.value("game_index", 0);
            sessionTreePathLength = session.value("tree_path_length", 0);
            if (session.contains("tree_path") && session["tree_path"].is_array()) {
                sessionTreePath.clear();
                for (const auto& idx : session["tree_path"]) {
                    sessionTreePath.push_back(idx.get<int>());
                }
            }
            sessionIsExternal = session.value("is_external", false);
            // One key since the modes became one enum. `tsumego_mode` and
            // `analysis_mode` are the two booleans it replaced — read only when
            // the new key is absent, so an existing user.json still restores,
            // and tsumego first, which is the order the enum resolves them in.
            // An unreadable name falls back rather than throwing: a settings
            // file is not a command line, and Match is the harmless answer.
            sessionGameMode = GameMode::MATCH;
            if (session.contains("game_mode")) {
                const auto parsed = parseGameMode(
                    session.value("game_mode", std::string()));
                if (parsed) {
                    sessionGameMode = *parsed;
                } else {
                    spdlog::warn("Unknown session game_mode '{}' — starting in {}",
                        session.value("game_mode", std::string()),
                        gameModeName(sessionGameMode));
                }
            } else if (session.value("tsumego_mode", false)) {
                sessionGameMode = GameMode::TSUMEGO;
            } else if (session.value("analysis_mode", false)) {
                sessionGameMode = GameMode::EXPLORE;
            }
            sessionBlackPlayer = session.value("black_player", std::string());
            sessionWhitePlayer = session.value("white_player", std::string());
        }

        spdlog::debug("User settings loaded");
    } catch (const std::exception& e) {
        spdlog::warn("Failed to parse user settings: {}", e.what());
    }
}

void UserSettings::save() {
    std::lock_guard<std::mutex> lock(mutex);
    saveLocked();
}

/// Serialise and write. The caller holds the lock.
void UserSettings::saveLocked() const {
    const std::string payload = serialize();

    // Write to a sibling temp file and rename over the target. std::ofstream
    // truncates on open, so the previous form left a window in which the file
    // on disk was empty or half-written: a crash or a power cut there lost every
    // setting, silently — load() would find unparseable JSON, warn to a log
    // nobody reads, and fall back to defaults. rename() is atomic on POSIX and
    // on Windows when the target exists (MoveFileEx semantics via std::rename
    // are not, hence the remove-first dance there).
    const std::string tmp = settingsFile + ".tmp";
    {
        std::ofstream fout(tmp, std::ios::binary | std::ios::trunc);
        if (!fout) {
            spdlog::warn("Failed to save user settings: cannot write {}", tmp);
            return;
        }
        fout << payload;
        fout.flush();
        if (!fout) {
            spdlog::warn("Failed to save user settings: write to {} failed", tmp);
            fout.close();
            std::error_code ec;
            std::filesystem::remove(tmp, ec);
            return;
        }
    }

    std::error_code ec;
#ifdef _WIN32
    // std::filesystem::rename replaces an existing file on Windows too, but only
    // via the error_code overload; the throwing one is documented as
    // implementation-defined when the target exists.
    std::filesystem::rename(tmp, settingsFile, ec);
    if (ec) {
        std::filesystem::remove(settingsFile, ec);
        std::filesystem::rename(tmp, settingsFile, ec);
    }
#else
    std::filesystem::rename(tmp, settingsFile, ec);
#endif
    if (ec) {
        spdlog::warn("Failed to save user settings: cannot replace {} ({})",
                     settingsFile, ec.message());
        std::error_code ignored;
        std::filesystem::remove(tmp, ignored);
        return;
    }
    spdlog::debug("User settings saved");
}

std::string UserSettings::serialize() const {
    nlohmann::json user;

    user["last_config"] = lastConfig;
    user["fullscreen"] = fullscreen;
    user["sound_enabled"] = soundEnabled;
    user["evaluation_enabled"] = evaluationEnabled;
    user["evaluation_moves"] = evaluationMoves;
    user["coordinates"] = coordinates;
    user["evaluation_readout"] = evaluationReadout;
    user["wait_clock"] = waitClock;
    user["evaluation_align"] = evaluationAlign;
    if (!evaluationColor.empty()) {
        user["evaluation_color"] = evaluationColor;
    }
    if (!anaglyph.empty()) {
        user["anaglyph"] = anaglyph;
    }
    if (anaglyphStrength >= 0.0f) {
        user["anaglyph_strength"] = anaglyphStrength;
    }
    if (anaglyphLeakSet) {
        user["anaglyph_leak"] = {anaglyphLeakR, anaglyphLeakG, anaglyphLeakB};
    }
    if (anaglyphBalanceSet) {
        user["anaglyph_balance"] = {anaglyphBalanceL, anaglyphBalanceR};
    }
    if (!glasses.empty()) {
        user["glasses"] = glasses;
    }
    if (anaglyphGreen >= 0.0f) {
        user["anaglyph_green"] = anaglyphGreen;
    }
    if (!prisonerMode.empty()) {
        user["prisoner_mode"] = prisonerMode;
    }

    if (!pointerMode.empty()) {
        user["pointer_mode"] = pointerMode;
    }
    if (!coordinateColor.empty()) {
        user["coordinate_color"] = coordinateColor;
    }
    if (!lastSgfPath.empty()) {
        user["last_sgf_path"] = lastSgfPath;
    }
    user["start_fresh"] = startFresh;

    if (gameSettingsLoaded) {
        user["game"] = {
            {"board_size", boardSize},
            {"komi", komi},
            {"handicap", handicap},
            {"black_player", blackPlayer},
            {"white_player", whitePlayer}
        };
    }

    user["shader"] = {
        {"name", shaderName},
        {"eof", shaderEof},
        {"dof", shaderDof},
        {"gamma", shaderGamma},
        {"contrast", shaderContrast}
    };

    // Written only when something is in it, so an untouched install carries no
    // key at all — the same restraint `evaluation_color` shows. Pinning today's
    // shipped defaults into every user.json would quietly defeat any later
    // change to them.
    if (!shaderParams.empty()) {
        user["shader_params"] = shaderParams;
    }

    // Helper to serialize camera state to JSON
    auto serializeCamera = [](const CameraState& cam) {
        return nlohmann::json{
            {"rotation", {{"x", cam.rotX}, {"y", cam.rotY}, {"z", cam.rotZ}, {"w", cam.rotW}}},
            {"pan", {{"x", cam.panX}, {"y", cam.panY}}},
            {"distance", cam.distance}
        };
    };

    // defaultCamera is deliberately absent: it belongs to config/base.json,
    // which ships with the application and is never written here.
    if (savedCameraLoaded) {
        user["camera"] = serializeCamera(savedCamera);
    }

    if (currentCameraLoaded) {
        user["camera_current"] = serializeCamera(currentCamera);
    }

    if (!sessionFile.empty()) {
        user["session"] = {
            {"file", sessionFile},
            {"game_index", sessionGameIndex},
            {"tree_path_length", sessionTreePathLength},
            {"tree_path", sessionTreePath},
            {"is_external", sessionIsExternal},
            // Only the new key is written; the two booleans it replaced are read
            // for migration and then left behind.
            {"game_mode", gameModeName(sessionGameMode)},
            {"black_player", sessionBlackPlayer},
            {"white_player", sessionWhitePlayer}
        };
    }

    return user.dump(2);
}

void UserSettings::setLastConfig(const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex);
    lastConfig = value;
    saveLocked();
}

void UserSettings::setFullscreen(bool value) {
    std::lock_guard<std::mutex> lock(mutex);
    fullscreen = value;
    saveLocked();
}

void UserSettings::setSoundEnabled(bool value) {
    std::lock_guard<std::mutex> lock(mutex);
    soundEnabled = value;
    saveLocked();
}

void UserSettings::setEvaluationEnabled(bool value) {
    std::lock_guard<std::mutex> lock(mutex);
    evaluationEnabled = value;
    saveLocked();
}

void UserSettings::setEvaluationMoves(const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex);
    evaluationMoves = value;
    saveLocked();
}

void UserSettings::setEvaluationReadout(bool value) {
    std::lock_guard<std::mutex> lock(mutex);
    evaluationReadout = value;
    saveLocked();
}

void UserSettings::setWaitClock(bool value) {
    std::lock_guard<std::mutex> lock(mutex);
    waitClock = value;
    saveLocked();
}

void UserSettings::setEvaluationAlign(const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex);
    evaluationAlign = value;
    saveLocked();
}

void UserSettings::setAnaglyph(const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex);
    anaglyph = value;
    saveLocked();
}

void UserSettings::setAnaglyphStrength(float value) {
    std::lock_guard<std::mutex> lock(mutex);
    anaglyphStrength = value;
    saveLocked();
}

void UserSettings::clearSavedCamera() {
    std::lock_guard<std::mutex> lock(mutex);
    savedCameraLoaded = false;
    saveLocked();
}

void UserSettings::setPointerMode(const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex);
    pointerMode = value;
    saveLocked();
}

void UserSettings::setPrisonerMode(const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex);
    prisonerMode = value;
    saveLocked();
}

void UserSettings::setAnaglyphGreen(float value) {
    std::lock_guard<std::mutex> lock(mutex);
    anaglyphGreen = value;
    saveLocked();
}

void UserSettings::setGlasses(const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex);
    glasses = value;
    saveLocked();
}

void UserSettings::setAnaglyphBalance(float left, float right) {
    std::lock_guard<std::mutex> lock(mutex);
    anaglyphBalanceL = left; anaglyphBalanceR = right;
    anaglyphBalanceSet = true;
    saveLocked();
}

void UserSettings::setAnaglyphLeak(float r, float g, float b) {
    std::lock_guard<std::mutex> lock(mutex);
    anaglyphLeakR = r; anaglyphLeakG = g; anaglyphLeakB = b;
    anaglyphLeakSet = true;
    saveLocked();
}

void UserSettings::setCoordinateColor(const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex);
    coordinateColor = value;
    saveLocked();
}

void UserSettings::setCoordinates(bool value) {
    std::lock_guard<std::mutex> lock(mutex);
    coordinates = value;
    saveLocked();
}

void UserSettings::setEvaluationColor(const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex);
    evaluationColor = value;
    saveLocked();
}

void UserSettings::setLastSgfPath(const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex);
    lastSgfPath = value;
}

void UserSettings::setStartFresh(bool value) {
    std::lock_guard<std::mutex> lock(mutex);
    startFresh = value;
}

void UserSettings::setShaderName(const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex);
    shaderName = value;
    saveLocked();
}

void UserSettings::setShaderEof(float value) {
    std::lock_guard<std::mutex> lock(mutex);
    shaderEof = value;
}

void UserSettings::setShaderDof(float value) {
    std::lock_guard<std::mutex> lock(mutex);
    shaderDof = value;
}

void UserSettings::setShaderGamma(float value) {
    std::lock_guard<std::mutex> lock(mutex);
    shaderGamma = value;
}

void UserSettings::setShaderContrast(float value) {
    std::lock_guard<std::mutex> lock(mutex);
    shaderContrast = value;
}

nlohmann::json UserSettings::getShaderParams(const std::string& shader) const {
    std::lock_guard<std::mutex> lock(mutex);
    const auto it = shaderParams.find(shader);
    if (it == shaderParams.end() || !it->is_object()) return nlohmann::json::object();
    return *it;   // by value; see the header
}

void UserSettings::setShaderParam(const std::string& shader, const std::string& name,
                                  bool value) {
    std::lock_guard<std::mutex> lock(mutex);
    shaderParams[shader][name] = value;
}

void UserSettings::setBoardSize(int value) {
    std::lock_guard<std::mutex> lock(mutex);
    boardSize = value;
    gameSettingsLoaded = true;
    saveLocked();
}

void UserSettings::setKomi(float value) {
    std::lock_guard<std::mutex> lock(mutex);
    komi = value;
    gameSettingsLoaded = true;
    saveLocked();
}

void UserSettings::setHandicap(int value) {
    std::lock_guard<std::mutex> lock(mutex);
    handicap = value;
    gameSettingsLoaded = true;
    saveLocked();
}

void UserSettings::setBlackPlayer(const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex);
    blackPlayer = value;
    saveLocked();
}

void UserSettings::setWhitePlayer(const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex);
    whitePlayer = value;
    saveLocked();
}

void UserSettings::setPlayers(const std::string& black, const std::string& white) {
    std::lock_guard<std::mutex> lock(mutex);
    blackPlayer = black;
    whitePlayer = white;
    saveLocked();
}

void UserSettings::setGameSettings(int newBoardSize, float newKomi, int newHandicap,
                                   const std::string& black, const std::string& white) {
    std::lock_guard<std::mutex> lock(mutex);
    boardSize = newBoardSize;
    komi = newKomi;
    handicap = newHandicap;
    blackPlayer = black;
    whitePlayer = white;
    gameSettingsLoaded = true;
    saveLocked();
}

void UserSettings::clearSessionState() {
    std::lock_guard<std::mutex> lock(mutex);
    sessionFile.clear();
    sessionGameIndex = 0;
    sessionTreePathLength = 0;
    sessionTreePath.clear();
    sessionIsExternal = false;
    sessionGameMode = GameMode::MATCH;
    sessionBlackPlayer.clear();
    sessionWhitePlayer.clear();
}
