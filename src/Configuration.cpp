#include "Configuration.h"

#include <fstream>
#include <spdlog/spdlog.h>

void Configuration::load(const std::string& fileName) {
    std::ifstream fin(fileName);
    if (!fin.is_open()) {
        spdlog::error("Failed to open config file: {}", fileName);
        return;
    }

    nlohmann::json current;
    fin >> current;

    // Handle $include - load base config first, then override with current values
    if (current.contains("$include")) {
        std::string includePath = current["$include"];
        // Make path relative to current file's directory
        size_t lastSlash = fileName.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            includePath = fileName.substr(0, lastSlash + 1) + includePath;
        }
        spdlog::debug("Loading included config: {}", includePath);
        load(includePath);  // Recursively load base
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