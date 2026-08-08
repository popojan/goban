#ifndef GOBAN_GAMEPHASE_H
#define GOBAN_GAMEPHASE_H

/** \brief What the game itself is doing.
 *
 * Step 1 of `docs/adr/0002-explicit-game-state.md`. The phase is *derived* from
 * `GobanModel::started` and `GobanModel::isGameOver`, which remain the
 * authoritative state; nothing writes a phase yet. Adding it therefore cannot
 * change behaviour. What it buys is a single name for each combination those
 * two booleans actually produce, one place to read the lifecycle from, and —
 * via the tests in `tests/test_gamephase.cpp` — a written-down transition
 * table, which is the prerequisite for step 2 (making the phase authoritative).
 *
 * The four states are exactly the ADR's:
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
