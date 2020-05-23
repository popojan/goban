#ifndef GOBAN_CONFIGURATION_H
#define GOBAN_CONFIGURATION_H

#include <Rocket/Core/Input.h>
#include <unordered_map>
#include <string>
#include "json.hpp"

struct Configuration {

    void load(const std::string& fileName);

    nlohmann::json data;

    std::string getCommand(Rocket::Core::Input::KeyIdentifier key) const;

private:

    void addKey(Rocket::Core::Input::KeyIdentifier key, const std::string& cmd);

    std::unordered_map<Rocket::Core::Input::KeyIdentifier, std::string> keyToCommand;
};

#endif
