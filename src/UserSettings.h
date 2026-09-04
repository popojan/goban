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
    /// "off", "on_demand" or "always" (ADR-0014). A string rather than an enum
    /// because this header is shared with goban_core, which knows nothing of the
    /// view; GobanView parses it.
    std::string getEvaluationMoves() const { std::lock_guard<std::mutex> lock(mutex); return evaluationMoves; }
    void setEvaluationMoves(const std::string& value);

    /// The numbers on the wood, and the elapsed-seconds clock. Both default on;
    /// both are visibility settings for parts of one on-board display, not a
    /// choice about where that display lives (see ADR-0012).
    bool getEvaluationReadout() const { std::lock_guard<std::mutex> lock(mutex); return evaluationReadout; }
    void setEvaluationReadout(bool value);
    bool getWaitClock() const { std::lock_guard<std::mutex> lock(mutex); return waitClock; }
    void setWaitClock(bool value);

    /// Where the board readout sits along the edge: "center", "left", "right".
    /// A taste question, so it is stored rather than decided.
    std::string getEvaluationAlign() const { std::lock_guard<std::mutex> lock(mutex); return evaluationAlign; }
    void setEvaluationAlign(const std::string& value);

    /// How the anaglyph combines the eyes: "gray", "half-color", "color",
    /// "dubois". Empty means "whatever the application config ships", on the
    /// `evaluation_color` precedent — writing a default here would pin today's
    /// choice into every user.json and defeat any later change to it.
    std::string getAnaglyph() const { std::lock_guard<std::mutex> lock(mutex); return anaglyph; }
    void setAnaglyph(const std::string& value);

    /// How much of each eye's colour survives; negative means "not set", so the
    /// shipped default stays in charge. Same reasoning as evaluationColor's
    /// empty string — a written-out default pins today's choice forever.
    float getAnaglyphStrength() const { std::lock_guard<std::mutex> lock(mutex); return anaglyphStrength; }
    void setAnaglyphStrength(float value);

    /// Per-channel crosstalk cancellation, measured for the user's own glasses.
    /// `hasAnaglyphLeak()` distinguishes "no correction wanted" from "never
    /// asked", which matters because zero is a legitimate answer.
    bool hasAnaglyphLeak() const { std::lock_guard<std::mutex> lock(mutex); return anaglyphLeakSet; }
    void getAnaglyphLeak(float& r, float& g, float& b) const {
        std::lock_guard<std::mutex> lock(mutex);
        r = anaglyphLeakR; g = anaglyphLeakG; b = anaglyphLeakB;
    }
    void setAnaglyphLeak(float r, float g, float b);

    /// Per-eye gain, for filters that pass different amounts of light. Same
    /// "asked or not" distinction as the leak: unity is a legitimate answer.
    bool hasAnaglyphBalance() const { std::lock_guard<std::mutex> lock(mutex); return anaglyphBalanceSet; }
    void getAnaglyphBalance(float& left, float& right) const {
        std::lock_guard<std::mutex> lock(mutex);
        left = anaglyphBalanceL; right = anaglyphBalanceR;
    }
    void setAnaglyphBalance(float left, float right);

    /// Which glasses are in use: "red-cyan" or "red-blue". Empty means the
    /// config's answer stands.
    std::string getGlasses() const { std::lock_guard<std::mutex> lock(mutex); return glasses; }
    void setGlasses(const std::string& value);

    /// When the board draws its own pointer: "auto", "always", "never". Empty
    /// means the config's answer stands.
    std::string getPointerMode() const { std::lock_guard<std::mutex> lock(mutex); return pointerMode; }
    void setPointerMode(const std::string& value);

    /// How much green the colour modes use; negative means "not set", so the
    /// config's answer stands. Same sentinel as anaglyphStrength.
    float getAnaglyphGreen() const { std::lock_guard<std::mutex> lock(mutex); return anaglyphGreen; }
    void setAnaglyphGreen(float value);

    /// The readout's ink, as a hex string. Empty means "whatever the application
    /// config ships" — the distinction matters, because writing it unconditionally
    /// would pin today's default into every user.json and quietly defeat any
    /// future change to it.
    std::string getEvaluationColor() const { std::lock_guard<std::mutex> lock(mutex); return evaluationColor; }
    void setEvaluationColor(const std::string& value);

    /// The coordinate labels' ink. Empty means "whatever the config ships", for
    /// the same reason the readout's does.
    std::string getCoordinateColor() const { std::lock_guard<std::mutex> lock(mutex); return coordinateColor; }
    void setCoordinateColor(const std::string& value);

    /// Column letters and row numbers printed on the board's margins. Off by
    /// default: this board is unusually pretty and the labels are a study aid,
    /// not part of the object.
    bool getCoordinates() const { std::lock_guard<std::mutex> lock(mutex); return coordinates; }
    void setCoordinates(bool value);

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
    std::string evaluationMoves = "off";
    bool coordinates = false;
    bool evaluationReadout = true;
    bool waitClock = true;
    std::string evaluationAlign = "center";
    std::string anaglyph;
    float anaglyphStrength = -1.0f;
    bool anaglyphLeakSet = false;
    float anaglyphLeakR = 0.0f, anaglyphLeakG = 0.0f, anaglyphLeakB = 0.0f;
    bool anaglyphBalanceSet = false;
    float anaglyphBalanceL = 1.0f, anaglyphBalanceR = 1.0f;
    std::string glasses;
    float anaglyphGreen = -1.0f;
    std::string pointerMode;
    std::string evaluationColor;
    std::string coordinateColor;

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
