/** \file
 *  \brief The authoritative game lifecycle state (ADR-0002).
 *
 * `GobanModel::gamePhase` is the only lifecycle state there is; the `started`
 * and `isGameOver` booleans this replaced are gone with no trace left, and the
 * old `started && isGameOver` combination is now unrepresentable. Ask
 * `GobanModel::phase()`, change it only through the named transitions, and read
 * `phaseName()` for the spelling scenarios assert on.
 */
#ifndef GOBAN_GAMEPHASE_H
#define GOBAN_GAMEPHASE_H

/** \brief What the game itself is doing.
 *
 * The four states are exactly ADR-0002's:
 *
 *     Setup ──start()──> Playing ──pause()/navigate back──> Paused
 *       ^                   │                                 │
 *       │                   └── double pass / resign ──> Finished
 *       └────────────── new game (onBoardSized) ──────────────┘
 */
enum class GamePhase {
    Setup,     ///< Nothing to resume: an empty record awaiting configuration.
    Playing,   ///< Active play; the game loop may call genmove.
    Paused,    ///< A game exists but play is stopped (review, or a loaded SGF).
    Finished   ///< Resignation or double pass, or a loaded game at its result.
};

/// Stable lowercase name, used by GobanControl::dumpState() and hence by
/// scenario `expect phase <name>` steps.
inline const char* phaseName(GamePhase phase) {
    switch (phase) {
        case GamePhase::Setup:    return "setup";
        case GamePhase::Playing:  return "playing";
        case GamePhase::Paused:   return "paused";
        case GamePhase::Finished: return "finished";
    }
    return "unknown";
}

#endif // GOBAN_GAMEPHASE_H
