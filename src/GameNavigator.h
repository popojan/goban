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
    using ActivePlayersProvider = std::function<std::vector<Player*>()>;
    using ObserverList = std::vector<GameObserver*>;

    /** \brief Result of navigateToVariation for caller to handle Analysis mode */
    struct VariationResult {
        bool success = false;
        bool newBranch = false;  // true if new branch created (caller may switch to Analysis mode)
    };

    GameNavigator(GobanModel& model, CoachProvider getCoach,
                  ActivePlayersProvider getActivePlayers, ObserverList& observers);

    // Navigation methods for SGF replay
    bool navigateBack();
    bool navigateForward();
    VariationResult navigateToVariation(const Move& move, bool promote = true);
    bool navigateToStart();
    bool navigateToEnd();
    bool navigateToTreePath(int pathLength, const std::vector<int>& branchChoices);  // Navigate to specific tree position (for session restore)

    // Check if navigation is in progress (for blocking genmove)
    bool isNavigating() const { return navigationInProgress.load(); }

    // Board change notifications (public for use by GameThread::executeNavCommand)
    void notifyBoardChange(const Board& result) const;
    void notifyBoardChangeWithMove(const Board& result, const Move& move) const;

    // Build board state from SGF without engine dependency (local capture logic)
    void buildBoardFromSGF(Board& outBoard) const;

    // Apply tsumego "X to move" hint at initial position when comment is empty
    void applyTsumegoHint();

private:
    // Scope guard to set/clear navigationInProgress flag
    struct NavigationGuard {
        std::atomic<bool>& flag;
        NavigationGuard(std::atomic<bool>& f) : flag(f) { flag = true; }
        ~NavigationGuard() { flag = false; }
    };

    // Helper to sync move across all engines
    void syncEngines(const Move& move, Engine* coach, bool isUndo) const;

    // Set pass message with variation label (e.g. "11a Black passes")
    void setPassMessage(const Move& passMove);

    // Sync model state after navigation (color, comment, markup, board)
    void syncStateAfterNavigation();

    GobanModel& model;
    CoachProvider getCoach;
    ActivePlayersProvider getActivePlayers;
    ObserverList& gameObservers;
    std::atomic<bool> navigationInProgress{false};
};

#endif // GAMENAVIGATOR_H
