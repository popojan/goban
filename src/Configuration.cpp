#include "Configuration.h"

#include <filesystem>
#include <fstream>
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