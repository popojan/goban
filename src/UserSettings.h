/** \file
 *  \brief What survives a restart: `user.json`, loaded once and saved on change.
 *
 * A process-wide singleton holding the preferences the user never explicitly
 * saves — language config, fullscreen, sound, camera preset, board size, komi,
 * handicap — plus the session state that lets startup put the board back where
 * it was: the SGF path, the game index within it, and the tree path (depth plus
 * the branch choices at multi-child nodes only).
 *
 * Distinct from Configuration, which is the read-only application config the
 * user edits by hand. This one the application writes.
 */
#ifndef GOBAN_USERSETTINGS_H
#define GOBAN_USERSETTINGS_H

#include <mutex>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

struct CameraState {
    float rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f, rotW = 1.0f;
    float panX = 0.0f, panY = 0.0f;
    float distance = 3.0f;
};

class UserSettings {
public:
    static UserSettings& instance();

    void load();
    void save();

    // Config
    std::string getLastConfig() const { std::lock_guard<std::mutex> lock(mutex); return lastConfig; }
    void setLastConfig(const std::string& value);

    // Fullscreen
    bool getFullscreen() const { std::lock_guard<std::mutex> lock(mutex); return fullscreen; }
    void setFullscreen(bool value);

    // Sound
    bool getSoundEnabled() const { std::lock_guard<std::mutex> lock(mutex); return soundEnabled; }
    void setSoundEnabled(bool value);

    /// The live evaluation overlay (ADR-0007). Off by default and sticky: a
    /// second engine's network weights are a real cost on the machines issue #45
    /// is about, so nobody pays it without asking — but a user who configured
    /// KataGo asks once, not once per session.
    bool getEvaluationEnabled() const { std::lock_guard<std::mutex> lock(mutex); return evaluationEnabled; }
    void setEvaluationEnabled(bool value);

    /// The engine's move suggestions drawn on the board. Separate from the
    /// panel above, and off by default, because the two are read at different
    /// times: the numbers *after* a move, so a player can invent their own and
    /// judge it; the stars *before*, at which point the choice is no longer
    /// theirs. Turning the panel on must not silently take that away.
    bool getEvaluationMoves() const { std::lock_guard<std::mutex> lock(mutex); return evaluationMoves; }
    void setEvaluationMoves(bool value);

    /// The evaluation as text on the board's near edge instead of in the panel.
    /// Off by default: it replaces a readout that always works with one that is
    /// an open question about how a live element feels in world space.
    bool getEvaluationOnBoard() const { std::lock_guard<std::mutex> lock(mutex); return evaluationOnBoard; }
    void setEvaluationOnBoard(bool value);

    // Last SGF (for resuming after restart)
    std::string getLastSgfPath() const { std::lock_guard<std::mutex> lock(mutex); return lastSgfPath; }
    void setLastSgfPath(const std::string& value);

    // Start fresh (skip auto-loading when user cleared board)
    bool getStartFresh() const { std::lock_guard<std::mutex> lock(mutex); return startFresh; }
    void setStartFresh(bool value);

    // Game settings
    int getBoardSize() const { std::lock_guard<std::mutex> lock(mutex); return boardSize; }
    void setBoardSize(int value);
    float getKomi() const { std::lock_guard<std::mutex> lock(mutex); return komi; }
    void setKomi(float value);
    int getHandicap() const { std::lock_guard<std::mutex> lock(mutex); return handicap; }
    void setHandicap(int value);
    std::string getBlackPlayer() const { std::lock_guard<std::mutex> lock(mutex); return blackPlayer; }
    void setBlackPlayer(const std::string& value);
    std::string getWhitePlayer() const { std::lock_guard<std::mutex> lock(mutex); return whitePlayer; }
    void setWhitePlayer(const std::string& value);
    void setPlayers(const std::string& black, const std::string& white);
    void setGameSettings(int boardSize, float komi, int handicap,
                         const std::string& black, const std::string& white);
    bool hasGameSettings() const { std::lock_guard<std::mutex> lock(mutex); return gameSettingsLoaded; }

    // Shader
    std::string getShaderName() const { std::lock_guard<std::mutex> lock(mutex); return shaderName; }
    void setShaderName(const std::string& value);

    float getShaderEof() const { std::lock_guard<std::mutex> lock(mutex); return shaderEof; }
    void setShaderEof(float value);

    float getShaderDof() const { std::lock_guard<std::mutex> lock(mutex); return shaderDof; }
    void setShaderDof(float value);

    float getShaderGamma() const { std::lock_guard<std::mutex> lock(mutex); return shaderGamma; }
    void setShaderGamma(float value);

    float getShaderContrast() const { std::lock_guard<std::mutex> lock(mutex); return shaderContrast; }
    void setShaderContrast(float value);

    // Camera preset (saved via "save camera", applied via "reset camera")
    CameraState getSavedCamera() const { std::lock_guard<std::mutex> lock(mutex); return savedCamera; }
    void setSavedCamera(const CameraState& state) { std::lock_guard<std::mutex> lock(mutex); savedCamera = state; savedCameraLoaded = true; }
    bool hasSavedCamera() const { std::lock_guard<std::mutex> lock(mutex); return savedCameraLoaded; }

    /// The view a fresh install opens on, from `camera` in the application
    /// config. Seeded by main() once the config is loaded, and never written
    /// back — this file is not where shipped defaults belong.
    ///
    /// It used to live in `user.json`, which is also the runtime scratchpad the
    /// application rewrites on every settings change. That put a tracked file in
    /// permanent conflict with the running program, and it leaked local paths
    /// and language into the repository once already (5fe4d48, "removed leaked
    /// local settings" — whose `.gitignore` entry could not help, because
    /// ignoring does nothing to a file already tracked).
    void setDefaultCamera(const CameraState& state) { std::lock_guard<std::mutex> lock(mutex); defaultCamera = state; defaultCameraLoaded = true; }
    CameraState getDefaultCamera() const { std::lock_guard<std::mutex> lock(mutex); return defaultCamera; }
    bool hasDefaultCamera() const { std::lock_guard<std::mutex> lock(mutex); return defaultCameraLoaded; }

    // Current camera (auto-saved on exit, restored on startup)
    CameraState getCurrentCamera() const { std::lock_guard<std::mutex> lock(mutex); return currentCamera; }
    void setCurrentCamera(const CameraState& state) { std::lock_guard<std::mutex> lock(mutex); currentCamera = state; currentCameraLoaded = true; }
    bool hasCurrentCamera() const { std::lock_guard<std::mutex> lock(mutex); return currentCameraLoaded; }

    // Session restoration
    std::string getSessionFile() const { std::lock_guard<std::mutex> lock(mutex); return sessionFile; }
    void setSessionFile(const std::string& value) { std::lock_guard<std::mutex> lock(mutex); sessionFile = value; }
    int getSessionGameIndex() const { std::lock_guard<std::mutex> lock(mutex); return sessionGameIndex; }
    void setSessionGameIndex(int value) { std::lock_guard<std::mutex> lock(mutex); sessionGameIndex = value; }
    std::vector<int> getSessionTreePath() const { std::lock_guard<std::mutex> lock(mutex); return sessionTreePath; }
    void setSessionTreePath(const std::vector<int>& value) { std::lock_guard<std::mutex> lock(mutex); sessionTreePath = value; }
    int getSessionTreePathLength() const { std::lock_guard<std::mutex> lock(mutex); return sessionTreePathLength; }
    void setSessionTreePathLength(int value) { std::lock_guard<std::mutex> lock(mutex); sessionTreePathLength = value; }
    bool getSessionIsExternal() const { std::lock_guard<std::mutex> lock(mutex); return sessionIsExternal; }
    void setSessionIsExternal(bool value) { std::lock_guard<std::mutex> lock(mutex); sessionIsExternal = value; }
    bool getSessionTsumegoMode() const { std::lock_guard<std::mutex> lock(mutex); return sessionTsumegoMode; }
    void setSessionTsumegoMode(bool value) { std::lock_guard<std::mutex> lock(mutex); sessionTsumegoMode = value; }
    bool getSessionAnalysisMode() const { std::lock_guard<std::mutex> lock(mutex); return sessionAnalysisMode; }
    void setSessionAnalysisMode(bool value) { std::lock_guard<std::mutex> lock(mutex); sessionAnalysisMode = value; }
    bool hasSessionState() const { std::lock_guard<std::mutex> lock(mutex); return !sessionFile.empty(); }
    void clearSessionState();

    // Check if settings were loaded (file existed)
    bool hasSettings() const { std::lock_guard<std::mutex> lock(mutex); return settingsLoaded; }
    bool hasShaderSettings() const { std::lock_guard<std::mutex> lock(mutex); return shaderLoaded; }

    /// Redirect persistence to another file. Must be called before load().
    /// Scenario runs use this so that driving the app from a script cannot
    /// overwrite the developer's real session, camera and game settings.
    void setSettingsFile(const std::string& path) { std::lock_guard<std::mutex> lock(mutex); settingsFile = path; }
    std::string getSettingsFile() const { std::lock_guard<std::mutex> lock(mutex); return settingsFile; }

private:
    UserSettings() = default;
    UserSettings(const UserSettings&) = delete;
    UserSettings& operator=(const UserSettings&) = delete;

    /// Serialise and write atomically. Both assume the caller holds the lock,
    /// which every public setter does — they lock, mutate, then call through.
    void saveLocked() const;
    std::string serialize() const;

    /// Every setter calls save(), and the setters are not all on one thread:
    /// GameThread::setFixedHandicap() calls setKomi() from the game thread while
    /// GobanView saves the camera from the UI thread, each rewriting the entire
    /// file. Without this they interleave in the same std::ofstream and in the
    /// members feeding it.
    mutable std::mutex mutex;

    std::string settingsFile = "user.json";

    bool settingsLoaded = false;
    bool savedCameraLoaded = false;
    bool defaultCameraLoaded = false;
    bool currentCameraLoaded = false;
    bool shaderLoaded = false;
    bool gameSettingsLoaded = false;

    // Config
    std::string lastConfig = "./config/en.json";

    // Fullscreen
    bool fullscreen = false;

    // Sound
    bool soundEnabled = true;

    // Live evaluation overlay
    bool evaluationEnabled = false;
    bool evaluationMoves = false;
    bool evaluationOnBoard = false;

    // Last SGF
    std::string lastSgfPath;
    bool startFresh = false;

    // Game settings
    int boardSize = 19;
    float komi = 6.5f;
    int handicap = 0;
    std::string blackPlayer = "Human";
    std::string whitePlayer = "Human";

    // Shader
    std::string shaderName;
    float shaderEof = 0.0725f;
    float shaderDof = 0.0925f;
    float shaderGamma = 1.0f;
    float shaderContrast = 0.0f;

    // Camera states
    CameraState savedCamera;    // Preset saved via menu
    CameraState defaultCamera;  // Shipped default, from the application config
    CameraState currentCamera;  // Auto-saved on exit

    // Session restoration
    std::string sessionFile;
    int sessionGameIndex = 0;
    int sessionTreePathLength = 0;
    std::vector<int> sessionTreePath;  // Branch choices only (consumed at multi-child nodes)
    bool sessionIsExternal = false;
    bool sessionTsumegoMode = false;
    bool sessionAnalysisMode = false;
};

#endif
