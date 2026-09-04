/** \file
 *  \brief Player and engine lifecycle: registration, ownership, coach and kibitz.
 *
 * Owns every Player (and deletes them), and holds the coach, kibitz and human
 * indices plus `activePlayer[]`, the single source of truth for who plays each
 * colour.
 *
 * This is the one object written from the loader threads at startup and read
 * from both the game and UI threads for the rest of the session. Everything goes
 * through `mutex` — *including* the writers, which is the half that was once
 * missing and cost a SIGSEGV. `addEngine()`/`addPlayer()` deliberately do not
 * lock: their callers already hold it while assigning the indices in the same
 * critical section. Observers are notified after the lock is released, so that
 * a callback into GobanModel cannot nest two mutexes.
 */
#ifndef PLAYERMANAGER_H
#define PLAYERMANAGER_H

#include <vector>
#include <array>
#include <mutex>
#include <memory>
#include <optional>
#include <functional>
#include "player.h"
#include "GameObserver.h"
#include "Configuration.h"

class Engine;

/** \brief Manages player and engine lifecycle
 *
 * Extracted from GameThread to separate player management from game loop.
 * Handles:
 * - Player/engine registration and ownership
 * - Coach and kibitz engine tracking via indices
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
    Engine* currentCoach() const;
    Engine* currentKibitz() const;
    Player* currentPlayer(Color colorToMove) const;

    // Active player management (activePlayer[] is the single source of truth for who plays each color)
    size_t activatePlayer(int which, size_t newIndex);
    size_t getActivePlayer(int which) const;

    // Check if both active players are human (used for Analysis mode check)
    bool areBothPlayersHuman() const;
    // Check if both active players are engines (for bot-bot detection)
    bool areBothPlayersEngines() const;

    // Player info
    std::string getName(size_t id) const;

    /// A **copy** of the player list, taken under the mutex.
    ///
    /// It used to hand out a reference to the vector itself, with no lock —
    /// which is the same partial-locking shape as the writers-unlocked bug
    /// above, only the other way round. `loadEnginesParallel()` starts the game
    /// loop as soon as the *coach* is ready and lets the remaining engines keep
    /// loading, so the game thread walks this list (initial sync, scoring,
    /// syncOtherEngines) while loader threads are still `push_back`ing into it.
    /// A reallocation there invalidates the iterator the game thread is holding.
    /// A vector of a handful of pointers is nothing to copy, and every caller
    /// wanted a stable list anyway.
    std::vector<Player*> getPlayers() const {
        std::lock_guard<std::mutex> lock(mutex);
        return players;
    }

    /// Whether the player assigned to a colour is an engine. One lock, so the
    /// index and the list it indexes cannot come from two different moments.
    bool isActivePlayerEngine(int which) const;


    // Engine loading progress, for the status indicator.
    //
    // Until uiReady flips, availableActions() returns all-false and the whole
    // toolbar is greyed. That is correct, and it is also indistinguishable from
    // a broken application — for up to a minute, if a CPU KataGo is loading
    // weights. These three name what is happening so the interface can say so,
    // and they name the *engine*, because "still loading" without a name tells a
    // user with two engines nothing about which one is wedged.
    //
    // Written from the per-engine loader threads and read from the UI thread;
    // loadMutex is its own rather than the player-list mutex, so an engine
    // finishing cannot block a frame behind loadSingleEngine().
    void beginLoading(const std::string& name);
    void finishLoading(const std::string& name);
    /// Empty when nothing is loading. One name, or "name (+N more)".
    [[nodiscard]] std::string loadingSummary() const;
    [[nodiscard]] bool isLoading() const;

    // Configuration
    void loadEngines(const std::shared_ptr<Configuration>& config);
    void loadHumanPlayers(const std::shared_ptr<Configuration>& config);  // Load human player entries
    void removeSgfPlayers();

    // Load a single engine from JSON config entry, returns engine or nullptr on failure
    Engine* loadSingleEngine(const nlohmann::json& botConfig);

    /// The bot configuration a dedicated analysis process should be built from,
    /// or nullopt when no enabled engine nominated itself (ADR-0007 decision 2).
    ///
    /// Resolution is `"analysis": 1`, else `"kibitz": 1`, else nothing — and
    /// deliberately *not* a fallback to the coach, which on a stock config is
    /// GNU Go and cannot analyse anyway. There is no index here because there is
    /// no `Player`: the analysis engine is a second process built from the same
    /// configuration, never registered with `addEngine()`, so it cannot reach the
    /// player dropdowns or `syncOtherEngines()`.
    ///
    /// `analysis_command` and `analysis_parameters` are substituted over
    /// `command` and `parameters` when present, which is what lets the analysis
    /// instance run a CPU backend while the playing engine keeps the GPU.
    /// `designated` distinguishes `"analysis": 1` from the `"kibitz"` fallback.
    /// Returned with the config rather than via a second accessor: two
    /// accessors are two moments (see the file comment).
    struct AnalysisChoice {
        nlohmann::json config;
        bool designated = false;
    };
    [[nodiscard]] std::optional<AnalysisChoice> analysisConfig() const;

    /// The human player, or nullptr if none has been registered yet. Analysis
    /// mode blocks on this one whatever colour is to move. A single lock, unlike
    /// the `getPlayers()[getHumanIndex()]` it replaces, which took the list and
    /// the index at two different moments and bounds-checked neither.
    Player* humanPlayer() const;

    // Special indices
    size_t getHumanIndex() const { std::lock_guard<std::mutex> lock(mutex); return human; }
    size_t getCoachIndex() const { return coach; }
    size_t getKibitzIndex() const { return kibitz; }

    // Set callback for interrupting current player (used by activatePlayer)
    void setInterruptCallback(InterruptCallback callback) { interruptPlayer = std::move(callback); }

    // Mutex access for external synchronization (e.g., game loop)
    std::mutex& getMutex() const { return mutex; }

private:
    ObserverList& gameObservers;
    std::vector<Engine*> engines;
    std::vector<Player*> players;

    size_t human{0};
    size_t sgf{0};
    size_t coach{0};
    size_t kibitz{0};
    /// Whether `coach`/`kibitz` were actually claimed by a bot carrying the
    /// "main"/"kibitz" flag, as opposed to still holding their initial 0.
    /// Index 0 is a perfectly valid engine, so the index alone cannot say —
    /// and when the configured main engine fails to load, currentCoach() hands
    /// out players[0] regardless. That silent promotion made an engine that
    /// cannot score the referee, which is how a game hung on "Calculating
    /// score…". Only the warning is new; the fallback itself is deliberate,
    /// since some engine is better than none.
    bool coachConfigured{false};
    bool kibitzConfigured{false};
    /// The two candidates for the analysis role, kept as configuration rather
    /// than as an index. Both are recorded because engines load in parallel and
    /// in arbitrary order, so the kibitz engine may well be seen before the one
    /// carrying "analysis" — deciding on first sight would make the winner
    /// depend on which process started faster.
    nlohmann::json analysisBot;
    nlohmann::json kibitzBot;
    bool analysisBotSet{false};
    bool kibitzBotSet{false};
    std::array<size_t, 2> activePlayer{0, 0};

    mutable std::mutex mutex;
    mutable std::mutex loadMutex;
    std::vector<std::string> loadingEngines;
    InterruptCallback interruptPlayer;
};

#endif // PLAYERMANAGER_H
