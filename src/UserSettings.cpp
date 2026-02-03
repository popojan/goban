#include "UserSettings.h"
#include <fstream>
#include <spdlog/spdlog.h>

UserSettings& UserSettings::instance() {
    static UserSettings instance;
    return instance;
}

void UserSettings::load() {
    std::ifstream fin(SETTINGS_FILE);
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
    nlohmann::json user;

    user["last_config"] = lastConfig;
    user["fullscreen"] = fullscreen;
    user["sound_enabled"] = soundEnabled;
    if (!lastSgfPath.empty()) {
        user["last_sgf_path"] = lastSgfPath;
    }
    user["start_fresh"] = startFresh;

    user["game"] = {
        {"board_size", boardSize},
        {"komi", komi},
        {"handicap", handicap},
        {"black_player", blackPlayer},
        {"white_player", whitePlayer}
    };

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

    std::ofstream fout(SETTINGS_FILE);
    if (fout) {
        fout << user.dump(2);
        fout.close();
        spdlog::debug("User settings saved");
    } else {
        spdlog::warn("Failed to save user settings");
    }
}

void UserSettings::setLastConfig(const std::string& value) {
    lastConfig = value;
    save();
}

void UserSettings::setFullscreen(bool value) {
    fullscreen = value;
    save();
}

void UserSettings::setSoundEnabled(bool value) {
    soundEnabled = value;
    save();
}

void UserSettings::setLastSgfPath(const std::string& value) {
    lastSgfPath = value;
    save();
}

void UserSettings::setStartFresh(bool value) {
    startFresh = value;
    save();
}

void UserSettings::setShaderName(const std::string& value) {
    shaderName = value;
    save();
}

void UserSettings::setShaderEof(float value) {
    shaderEof = value;
}

void UserSettings::setShaderDof(float value) {
    shaderDof = value;
}

void UserSettings::setShaderGamma(float value) {
    shaderGamma = value;
}

void UserSettings::setShaderContrast(float value) {
    shaderContrast = value;
}

void UserSettings::setBoardSize(int value) {
    boardSize = value;
    save();
}

void UserSettings::setKomi(float value) {
    komi = value;
    save();
}

void UserSettings::setHandicap(int value) {
    handicap = value;
    save();
}

void UserSettings::setBlackPlayer(const std::string& value) {
    blackPlayer = value;
    save();
}

void UserSettings::setWhitePlayer(const std::string& value) {
    whitePlayer = value;
    save();
}

void UserSettings::setPlayers(const std::string& black, const std::string& white) {
    blackPlayer = black;
    whitePlayer = white;
    save();
}

void UserSettings::clearSessionState() {
    sessionFile.clear();
    sessionGameIndex = 0;
    sessionTreePathLength = 0;
    sessionTreePath.clear();
    sessionIsExternal = false;
    sessionTsumegoMode = false;
    sessionAnalysisMode = false;
}
