#include "PlayerManager.h"
#include "UserSettings.h"
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
    std::lock_guard<std::mutex> lock(mutex);
    if (coach < players.size() && players[coach]->isTypeOf(Player::ENGINE)) {
        return dynamic_cast<Engine*>(players[coach]);
    }
    return nullptr;
}

Engine* PlayerManager::currentKibitz() const {
    std::lock_guard<std::mutex> lock(mutex);
    if (kibitz < players.size() && players[kibitz]->isTypeOf(Player::ENGINE)) {
        return dynamic_cast<Engine*>(players[kibitz]);
    }
    return nullptr;
}

Player* PlayerManager::currentPlayer(Color colorToMove) const {
    std::lock_guard<std::mutex> lock(mutex);
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
    std::lock_guard<std::mutex> lock(mutex);
    return activePlayer[which];
}

bool PlayerManager::areBothPlayersHuman() const {
    std::lock_guard<std::mutex> lock(mutex);
    // Bounds-checked like areBothPlayersEngines() below. setGameMode() asks this
    // from the UI thread, which can be before any player has been registered.
    if (activePlayer[0] >= players.size() || activePlayer[1] >= players.size()) {
        return false;
    }
    return players[activePlayer[0]]->isTypeOf(Player::HUMAN)
        && players[activePlayer[1]]->isTypeOf(Player::HUMAN);
}

bool PlayerManager::areBothPlayersEngines() const {
    std::lock_guard<std::mutex> lock(mutex);
    if (players.empty() || activePlayer[0] >= players.size() || activePlayer[1] >= players.size()) {
        return false;
    }
    return players[activePlayer[0]]->isTypeOf(Player::ENGINE)
        && players[activePlayer[1]]->isTypeOf(Player::ENGINE);
}

Player* PlayerManager::humanPlayer() const {
    std::lock_guard<std::mutex> lock(mutex);
    return human < players.size() ? players[human] : nullptr;
}

bool PlayerManager::isActivePlayerEngine(int which) const {
    std::lock_guard<std::mutex> lock(mutex);
    const size_t index = activePlayer[which];
    return index < players.size() && players[index]->isTypeOf(Player::ENGINE);
}

std::string PlayerManager::getName(size_t id) const {
    std::lock_guard<std::mutex> lock(mutex);
    if (id < players.size()) {
        return players[id]->getName();
    }
    return "";
}

Engine* PlayerManager::loadSingleEngine(const nlohmann::json& botConfig) {
    if (!botConfig.value("enabled", 1)) {
        return nullptr;
    }

    auto path = botConfig.value("path", "");
    auto name = botConfig.value("name", "");
    auto command = botConfig.value("command", "");
    auto parameters = botConfig.value("parameters", "");
    auto messages = botConfig.value("messages", nlohmann::json::array());

    if (command.empty()) {
        return nullptr;
    }

    try {
        auto engine = new GtpEngine(command, parameters, path, name, messages);

        // Optional per-engine response timeout. Engines differ enormously here:
        // a CPU KataGo can spend a minute loading weights, while a scripted test
        // engine should fail in seconds.
        if (botConfig.contains("timeout_ms")) {
            const int timeout = botConfig.value("timeout_ms", GtpClient::DEFAULT_COMMAND_TIMEOUT_MS);
            engine->setCommandTimeout(timeout);
            spdlog::info("Engine [{}] command timeout set to {} ms", name, timeout);
        }
        if (botConfig.contains("scoring_timeout_ms")) {
            const int timeout = botConfig.value("scoring_timeout_ms",
                                               GtpClient::DEFAULT_SCORING_TIMEOUT_MS);
            engine->setScoringTimeout(timeout);
            spdlog::info("Engine [{}] scoring timeout set to {} ms", name, timeout);
        }

        // Lock while modifying shared player/engine vectors and coach/kibitz indices
        // (loadSingleEngine is called from parallel threads)
        std::lock_guard<std::mutex> lock(mutex);
        size_t id = addEngine(engine);

        // Handle coach/kibitz flags. Keyed on the explicit flags rather than on
        // `index == 0`, which is also a legitimate engine: with the old test, a
        // main engine that happened to load first was silently overwritten by
        // the next one carrying "main".
        if (botConfig.value("main", 0) && !coachConfigured) {
            coach = id;
            coachConfigured = true;
            spdlog::info("Setting [{}] engine as coach and referee.", players[id]->getName());
        }
        if (botConfig.value("kibitz", 0) && !kibitzConfigured) {
            kibitz = id;
            kibitzConfigured = true;
            spdlog::info("Setting [{}] engine as trusted kibitz.", players[id]->getName());
        }

        // The analysis role is configuration, not an index — see analysisConfig().
        if (botConfig.value("analysis", 0) && !analysisBotSet) {
            analysisBot = botConfig;
            analysisBotSet = true;
        }
        if (botConfig.value("kibitz", 0) && !kibitzBotSet) {
            kibitzBot = botConfig;
            kibitzBotSet = true;
        }

        return engine;
    } catch (const std::exception& e) {
        spdlog::error("Failed to load engine '{}': {}", name.empty() ? command : name, e.what());
        return nullptr;
    }
}

std::optional<nlohmann::json> PlayerManager::analysisConfig() const {
    std::lock_guard<std::mutex> lock(mutex);
    if (!analysisBotSet && !kibitzBotSet) return std::nullopt;

    nlohmann::json bot = analysisBotSet ? analysisBot : kibitzBot;
    if (bot.contains("analysis_command")) {
        bot["command"] = bot["analysis_command"];
    }
    if (bot.contains("analysis_parameters")) {
        bot["parameters"] = bot["analysis_parameters"];
    }
    return bot;
}

void PlayerManager::beginLoading(const std::string& name) {
    std::lock_guard<std::mutex> lock(loadMutex);
    loadingEngines.push_back(name);
}

void PlayerManager::finishLoading(const std::string& name) {
    std::lock_guard<std::mutex> lock(loadMutex);
    auto it = std::find(loadingEngines.begin(), loadingEngines.end(), name);
    if (it != loadingEngines.end()) {
        loadingEngines.erase(it);
    }
}

std::string PlayerManager::loadingSummary() const {
    std::lock_guard<std::mutex> lock(loadMutex);
    if (loadingEngines.empty()) {
        return {};
    }
    // The first one still pending, which under parallel loading is whichever
    // was spawned first and has not answered — usually the slow one, which is
    // the one worth naming.
    std::string summary = loadingEngines.front();
    if (loadingEngines.size() > 1) {
        summary += " (+" + std::to_string(loadingEngines.size() - 1) + " more)";
    }
    return summary;
}

bool PlayerManager::isLoading() const {
    std::lock_guard<std::mutex> lock(loadMutex);
    return !loadingEngines.empty();
}

void PlayerManager::loadHumanPlayers(const std::shared_ptr<Configuration>& config) {
    // Same lock loadSingleEngine() takes, and for the same reason: this appends
    // to `players` and rewrites activePlayer[] while the UI thread may be
    // reading players[activePlayer[…]] through currentPlayer(). A push_back that
    // reallocates leaves that read dereferencing freed memory. addPlayer() does
    // not lock — callers do.
    std::lock_guard<std::mutex> lock(mutex);
    sgf = static_cast<size_t>(-1);

    auto humans = config->data.find("humans");
    if (humans != config->data.end()) {
        for (auto& it : *humans) {
            human = addPlayer(new LocalHumanPlayer(it));
        }
    } else {
        human = addPlayer(new LocalHumanPlayer("Human"));
    }

    activePlayer[0] = human;
    activePlayer[1] = !engines.empty() ? coach : human;

    // The engine carrying "main" never loaded, so currentCoach() is about to
    // hand out players[0] — an arbitrary engine, and under parallel loading not
    // even a deterministic one: it is whichever finished first. Say so. Silence
    // here cost a session, where a missing GNU Go promoted an engine that
    // answers "unclear groups" to final_score into the referee.
    if (!engines.empty() && !coachConfigured) {
        spdlog::warn("No engine claimed \"main\" (the configured one may have failed to "
                     "load); using [{}] as coach and referee. It decides legality and "
                     "scoring, so set \"main\": 1 on an engine you trust for that.",
                     players[coach]->getName());
    }

    // Default kibitz to coach if not set
    if (!engines.empty() && !kibitzConfigured) {
        kibitz = coach;
        spdlog::info("No kibitz set. Defaulting to [{}] coach engine.", players[kibitz]->getName());
    }
}

void PlayerManager::loadEngines(const std::shared_ptr<Configuration>& config) {
    // Sequential loading (legacy method)
    auto bots = config->data.find("bots");
    if (bots != config->data.end()) {
        for (auto it = bots->begin(); it != bots->end(); ++it) {
            loadSingleEngine(*it);
        }
    }
    loadHumanPlayers(config);
}

void PlayerManager::removeSgfPlayers() {
    // Erasing from `players` is the most destructive thing that happens to it,
    // so it is done under the lock the readers already take. Observers are
    // notified afterwards, outside the lock: onPlayerChange() calls into
    // GobanModel, and holding this mutex across that would put two locks in an
    // order nothing else uses.
    bool blackWasSgf = false;
    bool whiteWasSgf = false;
    {
        std::lock_guard<std::mutex> lock(mutex);
        // Remove players with SGF_PLAYER type (created during SGF loading)
        // Track if active players were SGF players before removal
        blackWasSgf = players[activePlayer[0]]->isTypeOf(Player::SGF_PLAYER);
        whiteWasSgf = players[activePlayer[1]]->isTypeOf(Player::SGF_PLAYER);

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

        // Restore active players from UserSettings if they were SGF players
        auto& settings = UserSettings::instance();
        auto findByName = [this](const std::string& name, size_t fallback) -> size_t {
            for (size_t i = 0; i < players.size(); i++) {
                if (players[i]->getName() == name) return i;
            }
            return fallback;
        };
        if (blackWasSgf) {
            activePlayer[0] = findByName(settings.getBlackPlayer(), human);
        }
        if (whiteWasSgf) {
            activePlayer[1] = findByName(settings.getWhitePlayer(), coach);
        }

    }  // lock released before observers are called

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
        players.size(), activePlayer[0], activePlayer[1]);
}
