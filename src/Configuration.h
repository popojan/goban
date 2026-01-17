#ifndef GOBAN_CONFIGURATION_H
#define GOBAN_CONFIGURATION_H

#include <RmlUi/Core/Input.h>
#include <unordered_map>
#include <string>
#include "json.hpp"

struct Configuration {

    explicit Configuration(const std::string& fileName) {
        load(fileName);
    }
    void load(const std::string& fileName);

    nlohmann::json data;

    std::string getCommand(Rml::Input::KeyIdentifier key) const;

private:

    void addKey(Rml::Input::KeyIdentifier key, const std::string& cmd);

    std::unordered_map<Rml::Input::KeyIdentifier, std::string> keyToCommand;
};

#endif
