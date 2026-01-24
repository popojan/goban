#include "PlayerManager.h"
#include "gtpclient.h"
#include <spdlog/spdlog.h>
#include <algorithm>

PlayerManager::PlayerManager(ObserverList& observers)
    : gameObservers(observers)
{
}

PlayerManager::~PlayerManager() {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto& player : players) {
        delete player;
    }
    players.clear();
}

size_t PlayerManager::addEngine(Engine* engine) {
    engines.push_back(engine);
    players.push_back(engine);
    return players.size() - 1;
}

size_t PlayerManager::addPlayer(Player* player) {
    players.push_back(player);
    return players.size() - 1;
}

Engine* PlayerManager::currentCoach() const {
    if (coach < players.size() && players[coach]->isTypeOf(Player::ENGINE)) {
        return dynamic_cast<Engine*>(players[coach]);
    }
    return nullptr;
}

Engine* PlayerManager::currentKibitz() const {
    if (kibitz < players.size() && players[kibitz]->isTypeOf(Player::ENGINE)) {
        return dynamic_cast<Engine*>(players[kibitz]);
    }
    return nullptr;
}

Player* PlayerManager::currentPlayer(Color colorToMove) const {
    int which = colorToMove == Color::BLACK ? 0 : 1;
    if (activePlayer[which] < players.size()) {
        return players[activePlayer[which]];
    }
    return nullptr;
}

size_t PlayerManager::activatePlayer(int which, size_t newIndex) {
    std::lock_guard<std::mutex> lock(mutex);

    if (newIndex >= players.size()) return activePlayer[which];
    size_t oldIndex = activePlayer[which];
    //following guard causes single-engine self-play not notify observers
    //if (oldIndex == newIndex) return oldIndex;

    activePlayer[which] = newIndex;

    // Interrupt old player to unblock any blocking genmove
    if (interruptPlayer) {
        interruptPlayer();
    }

    // Notify observers of player change
    std::for_each(
        gameObservers.begin(), gameObservers.end(),
        [which, this](GameObserver* observer) {
            observer->onPlayerChange(which, players[activePlayer[which]]->getName());
        }
    );

    spdlog::debug("activatePlayer: which={}, old={}, new={}, name='{}'",
        which, oldIndex, newIndex, players[newIndex]->getName());

    return newIndex;
}

size_t PlayerManager::getActivePlayer(int which) const {
    return activePlayer[which];
}

void PlayerManager::setActivePlayer(int which, size_t index) {
    std::lock_guard<std::mutex> lock(mutex);
    if (index < players.size()) {
        activePlayer[which] = index;
    }
}

bool PlayerManager::areBothPlayersHuman() const {
    return players[activePlayer[0]]->isTypeOf(Player::HUMAN)
        && players[activePlayer[1]]->isTypeOf(Player::HUMAN);
}

std::string PlayerManager::getName(size_t id) const {
    if (id < players.size()) {
        return players[id]->getName();
    }
    return "";
}

void PlayerManager::loadEngines(const std::shared_ptr<Configuration>& config) {
    auto bots = config->data.find("bots");
    if (bots != config->data.end()) {
        bool hasCoach = false;
        bool hasKibitz = false;
        for (auto it = bots->begin(); it != bots->end(); ++it) {
            if (it->value("enabled", 1)) {
                auto path = it->value("path", "");
                auto name = it->value("name", "");
                auto command = it->value("command", "");
                auto parameters = it->value("parameters", "");
                auto main = it->value("main", 0);
                auto kibitzFlag = it->value("kibitz", 0);
                auto messages = it->value("messages", nlohmann::json::array());

                if (!command.empty()) {
                    auto engine = new GtpEngine(command, parameters, path, name, messages);
                    size_t id = addEngine(engine);

                    if (main) {
                        if (!hasCoach) {
                            hasCoach = true;
                            coach = id;
                            spdlog::info("Setting [{}] engine as coach and referee.",
                                players[id]->getName());
                        } else {
                            spdlog::warn("Ignoring coach flag for [{}] engine, coach has already been set to [{}].",
                                players[id]->getName(),
                                players[coach]->getName());
                        }
                    }
                    if (kibitzFlag) {
                        if (!hasKibitz) {
                            hasKibitz = true;
                            kibitz = id;
                            spdlog::info("Setting [{}] engine as trusted kibitz.",
                                players[id]->getName());
                        } else {
                            spdlog::warn("Ignoring kibitz flag for [{}] engine, kibitz has already been set to [{}].",
                                players[id]->getName(),
                                players[coach]->getName());
                        }
                    }
                }
            }
        }
        if (!hasKibitz) {
            kibitz = coach;
            spdlog::info("No kibitz set. Defaulting to [{}] coach engine.",
                players[kibitz]->getName());
        }
    }

    sgf = static_cast<size_t>(-1);

    auto humans = config->data.find("humans");
    if (humans != config->data.end()) {
        for (auto& it : *humans) {
            human = addPlayer(new LocalHumanPlayer(it));
        }
    } else {
        human = addPlayer(new LocalHumanPlayer("Human"));
    }

    numPlayers = human + 1;
    activePlayer[0] = human;
    activePlayer[1] = coach;
}

void PlayerManager::removeSgfPlayers() {
    // Remove players with SGF_PLAYER type (created during SGF loading)
    // Track if active players were SGF players before removal
    bool blackWasSgf = players[activePlayer[0]]->isTypeOf(Player::SGF_PLAYER);
    bool whiteWasSgf = players[activePlayer[1]]->isTypeOf(Player::SGF_PLAYER);

    // Iterate backwards to avoid index shifting issues
    for (int i = static_cast<int>(players.size()) - 1; i >= 0; --i) {
        if (players[i]->isTypeOf(Player::SGF_PLAYER)) {
            spdlog::info("Removing SGF player '{}' at index {}", players[i]->getName(), i);
            // Adjust active player indices if they point to players after this one
            if (static_cast<size_t>(i) < activePlayer[0]) activePlayer[0]--;
            if (static_cast<size_t>(i) < activePlayer[1]) activePlayer[1]--;
            delete players[i];
            players.erase(players.begin() + i);
        }
    }

    numPlayers = players.size();

    // Reset to defaults if the active player was an SGF player
    if (blackWasSgf) {
        activePlayer[0] = human;
    }
    if (whiteWasSgf) {
        activePlayer[1] = coach;
    }

    // Notify observers so state.black/state.white reflect the new active players
    if (blackWasSgf) {
        for (auto* observer : gameObservers) {
            observer->onPlayerChange(0, players[activePlayer[0]]->getName());
        }
    }
    if (whiteWasSgf) {
        for (auto* observer : gameObservers) {
            observer->onPlayerChange(1, players[activePlayer[1]]->getName());
        }
    }

    spdlog::debug("removeSgfPlayers: {} players remaining, activePlayer=[{}, {}]",
        numPlayers, activePlayer[0], activePlayer[1]);
}
