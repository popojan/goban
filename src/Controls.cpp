#include "Controls.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

void Controls::addFromJson(const std::string& fileName) {
    std::ifstream fin(fileName);
    json j;
    fin >> j;
    for(auto it = j["controls"].begin(); it != j["controls"].end(); ++it) {
        add((*it)["command"], static_cast<Rocket::Core::Input::KeyIdentifier>((*it)["key"]));
    }
}
Controls& Controls::add(const std::string& cmd, Rocket::Core::Input::KeyIdentifier key) {
    commandToKey[cmd] = key;
    keyToCommand[key] = cmd;
    return (*this);
}