#include "UserSettings.h"
#include "Board.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <spdlog/spdlog.h>

UserSettings& UserSettings::instance() {
    static UserSettings instance;
    return instance;
}

namespace {

// One unreadable key costs that key, not every setting after it. The whole file
// used to be read inside a single try block, so the first value of the wrong
// type ended the load, and the next save wrote the defaults back over the rest.
template <typename Body>
void guarded(const char* key, Body&& body) {
    try {
        body();
    } catch (const nlohmann::json::exception& e) {
        spdlog::warn("user.json: ignoring '{}': {}", key, e.what());
    }
}

// Leaves `out` alone when the key is absent or unreadable. Safe on a value that
// is not an object at all: find() then simply finds nothing.
template <typename T>
void read(const nlohmann::json& obj, const char* key, T& out) {
    const auto it = obj.find(key);
    if (it == obj.end()) return;
    guarded(key, [&] { out = it->template get<T>(); });
}

}  // namespace

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

        read(user, "last_config", lastConfig);

        read(user, "fullscreen", fullscreen);

        read(user, "sound_enabled", soundEnabled);

        read(user, "evaluation_enabled", evaluationEnabled);

        if (user.contains("evaluation_moves")) {
            // Was a boolean before the three-state mode: true meant the
            // suggestions were always drawn, false that they never were.
            const auto& v = user["evaluation_moves"];
            if (v.is_boolean()) evaluationMoves = v.get<bool>() ? "always" : "off";
            else if (v.is_string()) evaluationMoves = v.get<std::string>();
        }

        read(user, "coordinates", coordinates);

        read(user, "evaluation_readout", evaluationReadout);

        read(user, "wait_clock", waitClock);

        read(user, "evaluation_align", evaluationAlign);

        read(user, "evaluation_color", evaluationColor);

        read(user, "anaglyph", anaglyph);

        read(user, "anaglyph_strength", anaglyphStrength);

        if (user.contains("anaglyph_leak") && user["anaglyph_leak"].is_array()
            && user["anaglyph_leak"].size() == 3) {
            guarded("anaglyph_leak", [&] {
                const auto leak = user["anaglyph_leak"].get<std::array<float, 3>>();
                anaglyphLeakR = leak[0];
                anaglyphLeakG = leak[1];
                anaglyphLeakB = leak[2];
                anaglyphLeakSet = true;
            });
        }

        if (user.contains("anaglyph_balance") && user["anaglyph_balance"].is_array()
            && user["anaglyph_balance"].size() == 2) {
            guarded("anaglyph_balance", [&] {
                const auto balance = user["anaglyph_balance"].get<std::array<float, 2>>();
                anaglyphBalanceL = balance[0];
                anaglyphBalanceR = balance[1];
                anaglyphBalanceSet = true;
            });
        }

        read(user, "glasses", glasses);

        read(user, "anaglyph_green", anaglyphGreen);

        read(user, "pointer_mode", pointerMode);
        if (user.contains("prisoner_mode") && user["prisoner_mode"].is_string()) {
            prisonerMode = user["prisoner_mode"].get<std::string>();
        }

        read(user, "coordinate_color", coordinateColor);

        read(user, "last_sgf_path", lastSgfPath);

        read(user, "start_fresh", startFresh);

        if (user.contains("game") && user["game"].is_object()) {
            gameSettingsLoaded = true;
            auto& game = user["game"];
            int savedSize = boardSize;
            read(game, "board_size", savedSize);
            if (Board::isSupportedSize(savedSize)) {
                boardSize = savedSize;
            } else {
                spdlog::warn("user.json: board_size {} is not supported; keeping {}",
                             savedSize, boardSize);
            }
            read(game, "komi", komi);
            read(game, "handicap", handicap);
            read(game, "black_player", blackPlayer);
            read(game, "white_player", whitePlayer);
        }

        if (user.contains("shader") && user["shader"].is_object()) {
            shaderLoaded = true;
            auto& shader = user["shader"];
            read(shader, "name", shaderName);
            read(shader, "eof", shaderEof);
            read(shader, "dof", shaderDof);
            read(shader, "gamma", shaderGamma);
            read(shader, "contrast", shaderContrast);
        }

        if (user.contains("shader_params") && user["shader_params"].is_object()) {
            shaderParams = user["shader_params"];
        }

        // Helper to parse camera state from JSON
        auto parseCamera = [](const nlohmann::json& j, CameraState& cam) {
            if (j.contains("rotation")) {
                auto& rot = j["rotation"];
                read(rot, "x", cam.rotX);
                read(rot, "y", cam.rotY);
                read(rot, "z", cam.rotZ);
                read(rot, "w", cam.rotW);
            }
            if (j.contains("pan")) {
                auto& pan = j["pan"];
                read(pan, "x", cam.panX);
                read(pan, "y", cam.panY);
            }
            read(j, "distance", cam.distance);
        };

        if (user.contains("camera") && user["camera"].is_object()) {
            savedCameraLoaded = true;
            parseCamera(user["camera"], savedCamera);
        }

        if (user.contains("camera_current") && user["camera_current"].is_object()) {
            currentCameraLoaded = true;
            parseCamera(user["camera_current"], currentCamera);
        }

        if (user.contains("session") && user["session"].is_object()) {
            auto& session = user["session"];
            read(session, "file", sessionFile);
            sessionGameIndex = 0;
            read(session, "game_index", sessionGameIndex);
            sessionTreePathLength = 0;
            read(session, "tree_path_length", sessionTreePathLength);
            if (session.contains("tree_path") && session["tree_path"].is_array()) {
                sessionTreePath.clear();
                read(session, "tree_path", sessionTreePath);
            }
            sessionIsExternal = false;
            read(session, "is_external", sessionIsExternal);
            // One key since the modes became one enum. `tsumego_mode` and
            // `analysis_mode` are the two booleans it replaced — read only when
            // the new key is absent, so an existing user.json still restores,
            // and tsumego first, which is the order the enum resolves them in.
            // An unreadable name falls back rather than throwing: a settings
            // file is not a command line, and Match is the harmless answer.
            sessionGameMode = GameMode::MATCH;
            std::string modeName;
            bool oldTsumego = false, oldAnalysis = false;
            read(session, "game_mode", modeName);
            read(session, "tsumego_mode", oldTsumego);
            read(session, "analysis_mode", oldAnalysis);
            if (session.contains("game_mode")) {
                const auto parsed = parseGameMode(modeName);
                if (parsed) {
                    sessionGameMode = *parsed;
                } else {
                    spdlog::warn("Unknown session game_mode '{}' — starting in {}",
                        modeName,
                        gameModeName(sessionGameMode));
                }
            } else if (oldTsumego) {
                sessionGameMode = GameMode::TSUMEGO;
            } else if (oldAnalysis) {
                sessionGameMode = GameMode::EXPLORE;
            }
            sessionBlackPlayer.clear();
            sessionWhitePlayer.clear();
            read(session, "black_player", sessionBlackPlayer);
            read(session, "white_player", sessionWhitePlayer);
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
