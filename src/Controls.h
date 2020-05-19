//
// Created by jan on 19.05.20.
//

#ifndef GOBAN_CONTROLS_H
#define GOBAN_CONTROLS_H

#include <Rocket/Core/Input.h>
#include <unordered_map>
#include <string>

struct Controls {
    std::unordered_map<std::string, Rocket::Core::Input::KeyIdentifier> commandToKey;
    std::unordered_map<Rocket::Core::Input::KeyIdentifier, std::string> keyToCommand;
    Controls& add(const std::string& cmd, Rocket::Core::Input::KeyIdentifier key);
};

#endif //GOBAN_CONTROLS_H
