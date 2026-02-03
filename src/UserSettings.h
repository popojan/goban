#ifndef GOBAN_USERSETTINGS_H
#define GOBAN_USERSETTINGS_H

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
    std::string getLastConfig() const { return lastConfig; }
    void setLastConfig(const std::string& value);

    // Fullscreen
    bool getFullscreen() const { return fullscreen; }
    void setFullscreen(bool value);

    // Sound
    bool getSoundEnabled() const { return soundEnabled; }
    void setSoundEnabled(bool value);

    // Last SGF (for resuming after restart)
    std::string getLastSgfPath() const { return lastSgfPath; }
    void setLastSgfPath(const std::string& value);

    // Start fresh (skip auto-loading when user cleared board)
    bool getStartFresh() const { return startFresh; }
    void setStartFresh(bool value);

    // Game settings
    int getBoardSize() const { return boardSize; }
    void setBoardSize(int value);
    float getKomi() const { return komi; }
    void setKomi(float value);
    int getHandicap() const { return handicap; }
    void setHandicap(int value);
    std::string getBlackPlayer() const { return blackPlayer; }
    void setBlackPlayer(const std::string& value);
    std::string getWhitePlayer() const { return whitePlayer; }
    void setWhitePlayer(const std::string& value);
    void setPlayers(const std::string& black, const std::string& white);
    bool hasGameSettings() const { return gameSettingsLoaded; }

    // Shader
    std::string getShaderName() const { return shaderName; }
    void setShaderName(const std::string& value);

    float getShaderEof() const { return shaderEof; }
    void setShaderEof(float value);

    float getShaderDof() const { return shaderDof; }
    void setShaderDof(float value);

    float getShaderGamma() const { return shaderGamma; }
    void setShaderGamma(float value);

    float getShaderContrast() const { return shaderContrast; }
    void setShaderContrast(float value);

    // Camera preset (saved via "save camera", applied via "reset camera")
    const CameraState& getSavedCamera() const { return savedCamera; }
    void setSavedCamera(const CameraState& state) { savedCamera = state; }
    bool hasSavedCamera() const { return savedCameraLoaded; }

    // Current camera (auto-saved on exit, restored on startup)
    const CameraState& getCurrentCamera() const { return currentCamera; }
    void setCurrentCamera(const CameraState& state) { currentCamera = state; currentCameraLoaded = true; }
    bool hasCurrentCamera() const { return currentCameraLoaded; }

    // Session restoration
    std::string getSessionFile() const { return sessionFile; }
    void setSessionFile(const std::string& value) { sessionFile = value; }
    int getSessionGameIndex() const { return sessionGameIndex; }
    void setSessionGameIndex(int value) { sessionGameIndex = value; }
    const std::vector<int>& getSessionTreePath() const { return sessionTreePath; }
    void setSessionTreePath(const std::vector<int>& value) { sessionTreePath = value; }
    int getSessionTreePathLength() const { return sessionTreePathLength; }
    void setSessionTreePathLength(int value) { sessionTreePathLength = value; }
    bool getSessionIsExternal() const { return sessionIsExternal; }
    void setSessionIsExternal(bool value) { sessionIsExternal = value; }
    bool getSessionTsumegoMode() const { return sessionTsumegoMode; }
    void setSessionTsumegoMode(bool value) { sessionTsumegoMode = value; }
    bool getSessionAnalysisMode() const { return sessionAnalysisMode; }
    void setSessionAnalysisMode(bool value) { sessionAnalysisMode = value; }
    bool hasSessionState() const { return !sessionFile.empty(); }
    void clearSessionState();

    // Check if settings were loaded (file existed)
    bool hasSettings() const { return settingsLoaded; }
    bool hasShaderSettings() const { return shaderLoaded; }

private:
    UserSettings() = default;
    UserSettings(const UserSettings&) = delete;
    UserSettings& operator=(const UserSettings&) = delete;

    static constexpr const char* SETTINGS_FILE = "user.json";

    bool settingsLoaded = false;
    bool savedCameraLoaded = false;
    bool currentCameraLoaded = false;
    bool shaderLoaded = false;
    bool gameSettingsLoaded = false;

    // Config
    std::string lastConfig = "./config/en.json";

    // Fullscreen
    bool fullscreen = false;

    // Sound
    bool soundEnabled = true;

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
