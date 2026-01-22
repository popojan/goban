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

        if (user.contains("camera")) {
            cameraLoaded = true;
            auto& camera = user["camera"];
            if (camera.contains("rotation")) {
                auto& rot = camera["rotation"];
                cameraRotX = rot.value("x", cameraRotX);
                cameraRotY = rot.value("y", cameraRotY);
                cameraRotZ = rot.value("z", cameraRotZ);
                cameraRotW = rot.value("w", cameraRotW);
            }
            if (camera.contains("translation")) {
                auto& trans = camera["translation"];
                cameraTransX = trans.value("x", cameraTransX);
                cameraTransY = trans.value("y", cameraTransY);
                cameraTransZ = trans.value("z", cameraTransZ);
            }
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

    user["shader"] = {
        {"name", shaderName},
        {"eof", shaderEof},
        {"dof", shaderDof},
        {"gamma", shaderGamma},
        {"contrast", shaderContrast}
    };

    user["camera"]["rotation"] = {
        {"x", cameraRotX},
        {"y", cameraRotY},
        {"z", cameraRotZ},
        {"w", cameraRotW}
    };

    user["camera"]["translation"] = {
        {"x", cameraTransX},
        {"y", cameraTransY},
        {"z", cameraTransZ}
    };

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

void UserSettings::setCameraRotation(float x, float y, float z, float w) {
    cameraRotX = x;
    cameraRotY = y;
    cameraRotZ = z;
    cameraRotW = w;
}

void UserSettings::setCameraTranslation(float x, float y, float z) {
    cameraTransX = x;
    cameraTransY = y;
    cameraTransZ = z;
}
