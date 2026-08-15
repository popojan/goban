#include "Configuration.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <spdlog/spdlog.h>

bool Configuration::load(const std::string& fileName) {
    std::ifstream fin(std::filesystem::u8path(fileName));
    if (!fin.is_open()) {
        spdlog::error("Failed to open config file: {}", fileName);
        return false;
    }

    nlohmann::json current;
    try {
        fin >> current;
    } catch (const nlohmann::json::parse_error& e) {
        spdlog::error("JSON parse error in config file '{}': {}", fileName, e.what());
        spdlog::error("  at byte position: {}", e.byte);
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Error reading config file '{}': {}", fileName, e.what());
        return false;
    }

    // Handle $include - load base config first, then override with current values
    if (current.contains("$include")) {
        std::string includePath = current["$include"];
        // Make path relative to current file's directory
        size_t lastSlash = fileName.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            includePath = fileName.substr(0, lastSlash + 1) + includePath;
        }
        spdlog::debug("Loading included config: {}", includePath);
        if (!load(includePath)) {  // Recursively load base
            return false;
        }
        current.erase("$include");
        data.merge_patch(current);  // Override with current file's values
    } else {
        data = current;
    }

    auto controls = data.find("controls");
    if(controls != data.end()) {
        for(auto & it : *controls) {
            auto key = it.find("key");
            auto command = it.find("command");
            if (key != it.end() && command != it.end()) {
                // An entry with no modifier keys is the historical form and
                // means "this key, unmodified".
                addKey(KeyChord{static_cast<Rml::Input::KeyIdentifier>(*key),
                                parseModifiers(it)},
                       *command);
            }
        }
    }
    return true;
}

unsigned Configuration::parseModifiers(const nlohmann::json& entry) {
    unsigned mods = KeyMod::NONE;
    if (entry.value("ctrl", false))  mods |= KeyMod::CTRL;
    if (entry.value("shift", false)) mods |= KeyMod::SHIFT;
    if (entry.value("alt", false))   mods |= KeyMod::ALT;
    return mods;
}

void Configuration::addKey(KeyChord chord, const std::string& cmd) {
    keyToCommand[chord] = cmd;
}

std::string Configuration::getCommand(Rml::Input::KeyIdentifier key, unsigned mods) const {
    auto it = keyToCommand.find(KeyChord{key, mods});
    if(it != keyToCommand.end()) {
        return it->second;
    }
    return {};
}

std::optional<glm::vec4> parseHexColor(const std::string& text) {
    std::string hex = text;
    if (!hex.empty() && hex.front() == '#') hex.erase(0, 1);
    if (hex.size() != 3 && hex.size() != 6 && hex.size() != 8) return std::nullopt;
    for (char c : hex) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) return std::nullopt;
    }
    auto component = [&hex](size_t index, size_t width) {
        const std::string part = hex.substr(index * width, width);
        // A short-form digit means both nibbles: #48c is #4488cc.
        const int value = std::stoi(width == 1 ? part + part : part, nullptr, 16);
        return static_cast<float>(value) / 255.0f;
    };
    const size_t width = hex.size() == 3 ? 1 : 2;
    const float alpha = hex.size() == 8 ? component(3, 2) : 1.0f;
    return glm::vec4(component(0, width), component(1, width), component(2, width), alpha);
}

std::string hexFromColor(const glm::vec4& color) {
    auto byte = [](float v) {
        return static_cast<int>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f));
    };
    std::ostringstream out;
    out << '#' << std::hex << std::setfill('0')
        << std::setw(2) << byte(color.r) << std::setw(2) << byte(color.g)
        << std::setw(2) << byte(color.b) << std::setw(2) << byte(color.a);
    return out.str();
}
