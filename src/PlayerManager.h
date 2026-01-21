#ifndef PLAYERMANAGER_H
#define PLAYERMANAGER_H

#include <vector>
#include <array>
#include <mutex>
#include <memory>
#include <functional>
#include "player.h"
#include "GameObserver.h"
#include "Configuration.h"

class Engine;

/** \brief Manages player and engine lifecycle and role assignment
 *
 * Extracted from GameThread to separate player management from game loop.
 * Handles:
 * - Player/engine registration and ownership
 * - Role assignment (BLACK, WHITE, COACH, KIBITZ)
 * - Active player tracking and switching
 * - Engine loading from configuration
 */
class PlayerManager {
public:
    using ObserverList = std::vector<GameObserver*>;
    using InterruptCallback = std::function<void()>;
    using ColorProvider = std::function<Color()>;

    explicit PlayerManager(ObserverList& observers);
    ~PlayerManager();

    // Player/engine registration
    size_t addEngine(Engine* engine);
    size_t addPlayer(Player* player);

    // Current player queries
    Engine* currentCoach();
    Engine* currentKibitz();
    Player* currentPlayer(Color colorToMove);

    // Role management
    void setRole(size_t playerIndex, int role, bool add);

    // Active player management
    size_t activatePlayer(int which, int delta);
    size_t getActivePlayer(int which) const;
    void setActivePlayer(int which, size_t index);  // For SGF loading

    // Check if both active players are human (used for Analysis mode check)
    bool areBothPlayersHuman() const;

    // Player info
    std::string getName(size_t id) const;
    const std::vector<Player*>& getPlayers() const { return players; }
    size_t getNumPlayers() const { return numPlayers; }

    // Configuration
    void loadEngines(const std::shared_ptr<Configuration>& config);
    void removeSgfPlayers();

    // Special indices
    size_t getHumanIndex() const { return human; }
    size_t getCoachIndex() const { return coach; }
    size_t getKibitzIndex() const { return kibitz; }

    // Set callback for interrupting current player (used by setRole)
    void setInterruptCallback(InterruptCallback callback) { interruptPlayer = std::move(callback); }

    // Mutex access for external synchronization (e.g., game loop)
    std::mutex& getMutex() { return mutex; }

private:
    ObserverList& gameObservers;
    std::vector<Engine*> engines;
    std::vector<Player*> players;

    std::size_t human{0};
    std::size_t sgf{0};
    std::size_t coach{0};
    std::size_t kibitz{0};
    std::size_t numPlayers{0};
    std::array<std::size_t, 2> activePlayer{0, 0};

    mutable std::mutex mutex;
    InterruptCallback interruptPlayer;
};

#endif // PLAYERMANAGER_H
