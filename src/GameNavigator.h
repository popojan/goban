#ifndef GAMENAVIGATOR_H
#define GAMENAVIGATOR_H

#include <atomic>
#include <vector>
#include <functional>
#include "Board.h"
#include "GameObserver.h"

class GobanModel;
class Engine;
class Player;

/** \brief Handles SGF tree navigation operations
 *
 * Extracted from GameThread to separate navigation logic from game loop.
 * Uses callbacks to access engine and observers owned by GameThread.
 *
 * Design invariants:
 * - Navigation must not interleave with genmove (use isNavigating() to block)
 * - All engines must stay in sync during navigation
 * - Territory display only at finished game positions
 */
class GameNavigator {
public:
    using CoachProvider = std::function<Engine*()>;
    using PlayerList = std::vector<Player*>;
    using ObserverList = std::vector<GameObserver*>;

    /** \brief Result of navigateToVariation for caller to handle Analysis mode */
    struct VariationResult {
        bool success = false;
        bool newBranch = false;  // true if new branch created (caller may switch to Analysis mode)
    };

    GameNavigator(GobanModel& model, CoachProvider getCoach,
                  PlayerList& players, ObserverList& observers);

    // Navigation methods for SGF replay
    bool navigateBack();
    bool navigateForward();
    VariationResult navigateToVariation(const Move& move);
    bool navigateToStart();
    bool navigateToEnd();

    // Check if navigation is in progress (for blocking genmove)
    bool isNavigating() const { return navigationInProgress.load(); }

private:
    // Scope guard to set/clear navigationInProgress flag
    struct NavigationGuard {
        std::atomic<bool>& flag;
        NavigationGuard(std::atomic<bool>& f) : flag(f) { flag = true; }
        ~NavigationGuard() { flag = false; }
    };

    // Helper to sync move across all engines
    void syncEngines(const Move& move, Engine* coach, bool isUndo);

    // Helper to notify observers of board change
    void notifyBoardChange(const Board& result);
    void notifyBoardChangeWithMove(const Board& result, const Move& move);

    GobanModel& model;
    CoachProvider getCoach;
    PlayerList& players;
    ObserverList& gameObservers;
    std::atomic<bool> navigationInProgress{false};
};

#endif // GAMENAVIGATOR_H
