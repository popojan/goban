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

    void annotate(const std::string& comment) const;

    void initGame(int boardSize, float komi, int handicap, const std::string& blackPlayer, const std::string& whitePlayer);

    void updatePlayers(const std::string& blackPlayer, const std::string& whitePlayer);

    void updateKomi(float komi);

    void saveAs(const std::string& fileName);

    [[nodiscard]] std::string getDefaultFileName() const { return defaultFileName; }
    void setDefaultFileName(const std::string& fileName) { defaultFileName = fileName; }
    void clearSession();  // Clear doc to start new session (used by archive)
    [[nodiscard]] size_t getNumGames() const { return numGames; }
    [[nodiscard]] bool hasNewMoves() const { return gameHasNewMoves; }
    [[nodiscard]] bool hasUnsavedChanges() const { return unsavedChanges; }

    void setHandicapStones(const std::vector<Position>& stones);

    void finalizeGame(float scoreDelta);

    [[nodiscard]] Move secondLastMove() const;


    struct SGFGameInfo {
        int boardSize;
        float komi;
        int handicap;
        std::string blackPlayer;
        std::string whitePlayer;
        std::vector<Position> setupBlackStones;      // AB: added black stones
        std::vector<Position> setupWhiteStones;    // AW: added white stones
        LibSgfcPlusPlus::SgfcGameResult gameResult;
    };

    bool loadFromSGF(const std::string& fileName, SGFGameInfo& gameInfo, int gameIndex = 0, bool startAtRoot = false);

    // Get player names from currently loaded game (reads PB/PW properties)
    std::pair<std::string, std::string> getPlayerNames() const;

    // Count stone moves (non-pass) of given color from root to current position
    // Used for capture calculation: captured = stonesPlayed - stonesOnBoard
    int countStoneMoves(const Color& color) const;

    // Quick peek at SGF file to get board size without full parsing
    // Returns board size or -1 if file doesn't exist or can't be parsed
    static int peekBoardSize(const std::string& fileName);

    // Tsumego detection heuristic (setup stones + small board + few moves)
    static bool isTsumego(const SGFGameInfo& info, size_t mainLineMoveCount);

private:

    void appendGameToDocument();

    // Internal save without mutex lock (for use within already-locked methods)
    void saveAsInternal(const std::string& fileName);

    // Helper to extract Move from SGF node (reduces code duplication)
    static std::optional<Move> extractMoveFromNode(
        const std::shared_ptr<LibSgfcPlusPlus::ISgfcNode>& node,
        int boardSizeColumns);

    // Tree traversal helpers (SGF as single source of truth)
    [[nodiscard]] size_t getTreeDepth() const;  // Depth from root to currentNode
    [[nodiscard]] std::vector<Move> getPathFromRoot() const;  // All moves from root to currentNode
    [[nodiscard]] bool isAtRoot() const;  // currentNode is root or setup-only ancestor

    // FF[3] compat: find the effective root (skip empty/setup-only nodes from root)
    // Returns the deepest non-move node before actual moves begin
    [[nodiscard]] std::shared_ptr<LibSgfcPlusPlus::ISgfcNode> findEffectiveRoot(
        const std::shared_ptr<LibSgfcPlusPlus::ISgfcNode>& rootNode) const;

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
    bool unsavedChanges = false;  // True when changes made since last save

    // Loaded external SGF document (for game cycling with PageUp/PageDown)
    std::shared_ptr<LibSgfcPlusPlus::ISgfcDocument> loadedExternalDoc;
    int loadedGameIndex = 0;

    // Helper: extract game info from root node (shared by loadFromSGF and switchToGame)
    void extractGameInfo(const std::shared_ptr<LibSgfcPlusPlus::ISgfcNode>& rootNode, SGFGameInfo& gameInfo) const;

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
    void removeGameResult() const;

    // Multi-variation support
    [[nodiscard]] std::vector<Move> getVariations() const;
    bool navigateToChild(const Move& move, bool promoteToMainLine = false);
    void promoteCurrentPathToMainLine() const;

    // Get comment from current node (C property)
    [[nodiscard]] std::string getComment() const;

    // Get markup annotations from current node (LB/TR/SQ/CR/MA properties)
    [[nodiscard]] std::vector<BoardMarkup> getMarkup() const;

    // Build board state from SGF by replaying moves (no engine dependency)
    // Populates outBoard with stones placed and captures processed
    // koPosition is set for ko rule enforcement
    void buildBoardFromMoves(Board& outBoard, Position& koPosition) const;

    // Get the board size from SGF
    [[nodiscard]] int getBoardSize() const { return boardSize.Columns; }

    // External SGF game cycling (PageUp/PageDown)
    [[nodiscard]] bool hasLoadedExternalDoc() const { return loadedExternalDoc != nullptr; }
    [[nodiscard]] int getLoadedGameIndex() const { return loadedGameIndex; }
    [[nodiscard]] size_t getLoadedGameCount() const;
    bool switchToGame(int gameIndex, SGFGameInfo& gameInfo, bool startAtRoot = false);
};

#endif //GOBAN_GAMERECORD_H
