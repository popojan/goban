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

    // Camera translation
    float getCameraTranslationX() const { return cameraTransX; }
    float getCameraTranslationY() const { return cameraTransY; }
    float getCameraTranslationZ() const { return cameraTransZ; }
    void setCameraTranslation(float x, float y, float z);

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

    // Config
    std::string lastConfig = "./config/en.json";

    // Fullscreen
    bool fullscreen = false;

    // Sound
    bool soundEnabled = true;

    // Last SGF
    std::string lastSgfPath;

    // Shader
    std::string shaderName;
    float shaderEof = 0.0075f;
    float shaderDof = 0.01f;
    float shaderGamma = 1.0f;
    float shaderContrast = 0.0f;

    // Camera rotation (quaternion)
    float cameraRotX = 0.0f;
    float cameraRotY = 0.0f;
    float cameraRotZ = 0.0f;
    float cameraRotW = 1.0f;

    // Camera translation
    float cameraTransX = 0.0f;
    float cameraTransY = 0.0f;
    float cameraTransZ = 0.0f;
};

#endif
