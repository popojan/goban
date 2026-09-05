/** \file
 *  \brief What produces the opponent's moves.
 *
 * Its own header for the same reason GamePhase.h is: the enum is asked about
 * far from the class that owns it — UserSettings persists it, UiActions decides
 * over it — and pulling GameThread.h in for a three-value enum would drag the
 * whole game loop with it. `GameThread::gameMode` is the source of truth;
 * `GobanModel::tsumegoMode` is only the copy published for threads that cannot
 * reach it.
 */
#ifndef GOBAN_GAMEMODE_H
#define GOBAN_GAMEMODE_H

#include <optional>
#include <string>

/** \brief Game mode determining player interaction behavior.
 *
 * The declaration order is the `#selectGameMode` option order in every
 * language's `config/gui/<lang>/goban.rml`: `ElementGame::OnUpdate()` casts the
 * mode straight to a selection index, as it does for the select beside it.
 */
enum class GameMode {
    MATCH,      ///< Strict turn alternation, assigned roles.
    EXPLORE,    ///< Human plays either colour; the kibitz engine answers every
                ///< move. That reply is the distinguishing property, not the
                ///< free navigation a loaded game has anyway while Paused.
    TSUMEGO     ///< A problem: the record answers, on the solution path and on a
                ///< refuted one alike. Player assignment is not consulted.
};

/// One spelling per mode, shared by the log, `dumpState()`'s `mode` key (and so
/// by scenario `expect mode <name>` steps), the menu select's option values and
/// `user.json`'s `game_mode`. A second copy would let the settings file and the
/// scenario suite disagree about a name.
inline const char* gameModeName(GameMode mode) {
    switch (mode) {
        case GameMode::MATCH:   return "match";
        case GameMode::EXPLORE: return "explore";
        case GameMode::TSUMEGO: return "tsumego";
    }
    return "match";
}

/// The inverse, and empty rather than defaulting: "this is not a mode" has to
/// be distinguishable, or `game_mode <typo>` silently switches to whichever
/// value happened to be first.
inline std::optional<GameMode> parseGameMode(const std::string& name) {
    if (name == "match")   return GameMode::MATCH;
    if (name == "explore") return GameMode::EXPLORE;
    if (name == "tsumego") return GameMode::TSUMEGO;
    return std::nullopt;
}

#endif // GOBAN_GAMEMODE_H
