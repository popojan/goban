#include "Configuration.h"

#include <fstream>
#include <spdlog/spdlog.h>

bool Configuration::load(const std::string& fileName) {
    std::ifstream fin(fileName);
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
                addKey(static_cast<Rml::Input::KeyIdentifier>(*key), *command);
            }
        }
    }
    return true;
}

void Configuration::addKey(Rml::Input::KeyIdentifier key, const std::string& cmd) {
    keyToCommand[key] = cmd;
}

std::string Configuration::getCommand(Rml::Input::KeyIdentifier key) const {
    auto it = keyToCommand.find(key);
    if(it != keyToCommand.end()) {
        return it->second;
    }
    return {};
}