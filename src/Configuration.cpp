#include "Configuration.h"

#include <fstream>

void Configuration::load(const std::string& fileName) {
    std::ifstream fin(fileName);
    fin >> data;
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