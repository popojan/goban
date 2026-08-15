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
            evaluationMoves = user["evaluation_moves"].get<bool>();
        }

        if (user.contains("coordinates")) {
            coordinates = user["coordinates"].get<bool>();
        }

        if (user.contains("evaluation_on_board")) {
            evaluationOnBoard = user["evaluation_on_board"].get<bool>();
        }

        if (user.contains("evaluation_align")) {
            evaluationAlign = user["evaluation_align"].get<std::string>();
        }

        if (user.contains("evaluation_color")) {
            evaluationColor = user["evaluation_color"].get<std::string>();
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
            sessionTsumegoMode = session.value("tsumego_mode", false);
            sessionAnalysisMode = session.value("analysis_mode", false);
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
    user["evaluation_on_board"] = evaluationOnBoard;
    user["coordinates"] = coordinates;
    user["evaluation_align"] = evaluationAlign;
    if (!evaluationColor.empty()) {
        user["evaluation_color"] = evaluationColor;
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
            {"tsumego_mode", sessionTsumegoMode},
            {"analysis_mode", sessionAnalysisMode}
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

void UserSettings::setEvaluationMoves(bool value) {
    std::lock_guard<std::mutex> lock(mutex);
    evaluationMoves = value;
    saveLocked();
}

void UserSettings::setEvaluationOnBoard(bool value) {
    std::lock_guard<std::mutex> lock(mutex);
    evaluationOnBoard = value;
    saveLocked();
}

void UserSettings::setEvaluationAlign(const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex);
    evaluationAlign = value;
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
    sessionTsumegoMode = false;
    sessionAnalysisMode = false;
}
