#ifndef GOBAN_GAMERECORD_H
#define GOBAN_GAMERECORD_H

#include <memory>
#include <algorithm>
#include <optional>
#include "Board.h"

#include "SGF.h"
#include "GameState.h"
#include <mutex>

class GameRecord {
public:
    enum EventType { PLAYER_SWITCHED, KIBITZ_MOVE, LAST_EVENT};
    const static std::array<std::string, LAST_EVENT> eventNames;

    GameRecord();
    size_t moveCount() const { return getTreeDepth(); }

    void undo();

    template <class UnaryFunction>
    void replay(UnaryFunction fMove) {
        auto path = getPathFromRoot();
        std::for_each(path.begin(), path.end(), fMove);
    }

    [[nodiscard]] Move lastMove() const;
    [[nodiscard]] Move lastStoneMove() const;
    [[nodiscard]] std::pair<Move, size_t> lastStoneMoveIndex() const;

    void move(const Move& move);
    void branchFromFinishedGame(const Move& move);  // Copy path and branch (preserves original)

    void annotate(const std::string& comment);

    void initGame(int boardSize, float komi, int handicap, const std::string& blackPlayer, const std::string& whitePlayer);

    void updatePlayers(const std::string& blackPlayer, const std::string& whitePlayer);

    void updateKomi(float komi);

    void saveAs(const std::string& fileName);

    [[nodiscard]] std::string getDefaultFileName() const { return defaultFileName; }
    void setDefaultFileName(const std::string& fileName) { defaultFileName = fileName; }
    [[nodiscard]] size_t getNumGames() const { return numGames; }
    [[nodiscard]] bool hasNewMoves() const { return gameHasNewMoves; }

    void setHandicapStones(const std::vector<Position>& stones);

    void finalizeGame(float scoreDelta);

    [[nodiscard]] Move secondLastMove() const;


    struct SGFGameInfo {
        int boardSize;
        float komi;
        int handicap;
        std::string blackPlayer;
        std::string whitePlayer;
        std::vector<Position> handicapStones;
        LibSgfcPlusPlus::SgfcGameResult gameResult;
    };

    bool loadFromSGF(const std::string& fileName, SGFGameInfo& gameInfo, int gameIndex = 0);

private:

    void appendGameToDocument();

    // Helper to extract Move from SGF node (reduces code duplication)
    static std::optional<Move> extractMoveFromNode(
        const std::shared_ptr<LibSgfcPlusPlus::ISgfcNode>& node,
        int boardSizeColumns);

    // Tree traversal helpers (SGF as single source of truth)
    [[nodiscard]] size_t getTreeDepth() const;  // Depth from root to currentNode
    [[nodiscard]] std::vector<Move> getPathFromRoot() const;  // All moves from root to currentNode
    [[nodiscard]] bool isAtRoot() const;  // currentNode is root or first child of root

    typedef LibSgfcPlusPlus::SgfcPlusPlusFactory F;
    typedef LibSgfcPlusPlus::SgfcPropertyType T;
    typedef LibSgfcPlusPlus::SgfcPropertyValueType V;
    typedef LibSgfcPlusPlus::SgfcConstants C;

    std::shared_ptr<LibSgfcPlusPlus::ISgfcNode> currentNode;
    std::shared_ptr<LibSgfcPlusPlus::ISgfcGame> game;
    std::shared_ptr<LibSgfcPlusPlus::ISgfcDocument> doc;
    std::shared_ptr<LibSgfcPlusPlus::ISgfcTreeBuilder> builder;
    LibSgfcPlusPlus::SgfcBoardSize boardSize;

    std::string defaultFileName;
    std::mutex mutex;
    size_t numGames;
    bool gameHasNewMoves;
    bool gameInDocument;  // True when game is already part of doc (prevent re-append)

public:
    // Navigation methods (SGF tree-based)
    [[nodiscard]] bool hasNextMove() const;
    [[nodiscard]] bool hasPreviousMove() const { return currentNode && currentNode->HasParent() && !isAtRoot(); }
    [[nodiscard]] Move getNextMove() const;
    [[nodiscard]] size_t getViewPosition() const { return getTreeDepth(); }
    [[nodiscard]] size_t getLoadedMovesCount() const;  // Total moves on main line
    [[nodiscard]] bool isNavigating() const { return game != nullptr; }  // Always navigable if game exists
    [[nodiscard]] bool isAtEndOfNavigation() const { return !hasNextMove(); }

    // Get color to move based on SGF tree position
    [[nodiscard]] Color getColorToMove() const;

    // Check if game has a result (RE property in SGF)
    [[nodiscard]] bool hasGameResult() const;

    // Check if current position is a finished game state (resign or double pass)
    [[nodiscard]] bool isGameFinished() const;

    // Check if main line (following first children) ends in a finished state
    [[nodiscard]] bool isMainLineFinished() const;

    // Check if RE property indicates resignation (+R)
    [[nodiscard]] bool isResignationResult() const;

    // Unified check: at a finished game position (resign, double-pass, or has result at end)
    [[nodiscard]] bool isAtFinishedGame() const;

    // Check if territory should be displayed (at end of scored game, not resignation)
    [[nodiscard]] bool shouldShowTerritory() const;

    // Get result message type based on RE property
    [[nodiscard]] GameState::Message getResultMessage() const;

    // Remove RE (result) property from root node
    void removeGameResult();

    // Multi-variation support
    [[nodiscard]] std::vector<Move> getVariations() const;
    bool navigateToChild(const Move& move, bool promoteToMainLine = false);
    void promoteCurrentPathToMainLine();

    // Get comment from current node (C property)
    [[nodiscard]] std::string getComment() const;

    // Get markup annotations from current node (LB/TR/SQ/CR/MA properties)
    [[nodiscard]] std::vector<BoardMarkup> getMarkup() const;
};

#endif //GOBAN_GAMERECORD_H
