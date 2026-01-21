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

Engine* PlayerManager::currentCoach() {
    for (auto& engine : engines) {
        if (engine->hasRole(Player::COACH)) return engine;
    }
    return nullptr;
}

Engine* PlayerManager::currentKibitz() {
    for (auto& engine : engines) {
        if (engine->hasRole(Player::KIBITZ)) return engine;
    }
    return nullptr;
}

Player* PlayerManager::currentPlayer(Color colorToMove) {
    int roleToMove = colorToMove == Color::BLACK ? Player::BLACK : Player::WHITE;
    for (auto& player : players) {
        if (player->hasRole(roleToMove)) return player;
    }
    return nullptr;
}

void PlayerManager::setRole(size_t playerIndex, int role, bool add) {
    if (players.size() <= playerIndex) return;

    Player* player = players[playerIndex];
    player->setRole(role, add);
    spdlog::debug("Player[{}] newType = [human = {}, computer = {}] newRole = [black = {}, white = {}]",
        playerIndex,
        player->isTypeOf(Player::HUMAN),
        player->isTypeOf(Player::ENGINE),
        player->hasRole(Player::BLACK),
        player->hasRole(Player::WHITE)
    );

    // Interrupt current player if removing role from human
    if (!add && interruptPlayer) {
        interruptPlayer();
    }

    // Notify observers when adding a role
    if (add) {
        std::for_each(
            gameObservers.begin(), gameObservers.end(),
            [player, role](GameObserver* observer) {
                observer->onPlayerChange(role, player->getName());
            }
        );
    }
}

size_t PlayerManager::activatePlayer(int which, int delta) {
    std::lock_guard<std::mutex> lock(mutex);

    size_t oldp = activePlayer[which];
    size_t newp = (oldp + delta) % numPlayers;

    activePlayer[which] = newp;

    int role = which == 0 ? Player::BLACK : Player::WHITE;

    // setRole doesn't lock (observer notification is safe)
    setRole(oldp, role, false);
    setRole(newp, role, true);

    return newp;
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
            auto enabled = it->value("enabled", 1);
            if (enabled) {
                auto path = it->value("path", "");
                auto name = it->value("name", "");
                auto command = it->value("command", "");
                auto parameters = it->value("parameters", "");
                auto main = it->value("main", 0);
                auto kibitzFlag = it->value("kibitz", 0);
                auto messages = it->value("messages", nlohmann::json::array());

                int role = Player::SPECTATOR;

                if (!command.empty()) {
                    auto engine = new GtpEngine(command, parameters, path, name, messages);
                    size_t id = addEngine(engine);

                    if (main) {
                        if (!hasCoach) {
                            role |= Player::COACH;
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
                            role |= Player::KIBITZ;
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
                    setRole(id, role, true);
                }
            }
        }
        if (!hasKibitz) {
            kibitz = coach;
            spdlog::info("No kibitz set. Defaulting to [{}] coach engine.",
                players[kibitz]->getName());
            players[coach]->setRole(players[coach]->getRole() | Player::KIBITZ);
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

    // Only reset to defaults if the active player was an SGF player that got removed
    if (blackWasSgf) activePlayer[0] = human;
    if (whiteWasSgf) activePlayer[1] = coach;

    spdlog::debug("removeSgfPlayers: {} players remaining, activePlayer=[{}, {}]",
        numPlayers, activePlayer[0], activePlayer[1]);
}
