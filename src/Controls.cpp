#include "Controls.h"

Controls& Controls::add(const std::string& cmd, Rocket::Core::Input::KeyIdentifier key) {
    commandToKey[cmd] = key;
    keyToCommand[key] = cmd;
    return (*this);
}