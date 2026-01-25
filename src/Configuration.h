#ifndef GOBAN_CONFIGURATION_H
#define GOBAN_CONFIGURATION_H

#include <RmlUi/Core/Input.h>
#include <unordered_map>
#include <string>
#include <nlohmann/json.hpp>

struct Configuration {

    explicit Configuration(const std::string& fileName) {
        valid = load(fileName);
    }
    bool load(const std::string& fileName);

    nlohmann::json data;
    bool valid = false;

    std::string getCommand(Rml::Input::KeyIdentifier key) const;

private:

    void addKey(Rml::Input::KeyIdentifier key, const std::string& cmd);

    std::unordered_map<Rml::Input::KeyIdentifier, std::string> keyToCommand;
};

#endif
