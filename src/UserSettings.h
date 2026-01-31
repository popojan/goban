#ifndef GOBAN_USERSETTINGS_H
#define GOBAN_USERSETTINGS_H

#include <string>
#include <nlohmann/json.hpp>

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

    // Camera rotation
    float getCameraRotationX() const { return cameraRotX; }
    float getCameraRotationY() const { return cameraRotY; }
    float getCameraRotationZ() const { return cameraRotZ; }
    float getCameraRotationW() const { return cameraRotW; }
    void setCameraRotation(float x, float y, float z, float w);

    // Camera pan and distance
    float getCameraPanX() const { return cameraPanX; }
    float getCameraPanY() const { return cameraPanY; }
    float getCameraDistance() const { return cameraDistVal; }
    void setCameraPan(float x, float y);
    void setCameraDistance(float d);

    // Check if settings were loaded (file existed)
    bool hasSettings() const { return settingsLoaded; }
    bool hasCameraSettings() const { return cameraLoaded; }
    bool hasShaderSettings() const { return shaderLoaded; }

private:
    UserSettings() = default;
    UserSettings(const UserSettings&) = delete;
    UserSettings& operator=(const UserSettings&) = delete;

    static constexpr const char* SETTINGS_FILE = "user.json";

    bool settingsLoaded = false;
    bool cameraLoaded = false;
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

    // Camera rotation (quaternion)
    float cameraRotX = 0.0f;
    float cameraRotY = 0.0f;
    float cameraRotZ = 0.0f;
    float cameraRotW = 1.0f;

    // Camera pan and distance
    float cameraPanX = 0.0f;
    float cameraPanY = 0.0f;
    float cameraDistVal = 3.0f;
};

#endif
