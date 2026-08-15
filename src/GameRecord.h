/** \file
 *  \brief The SGF game tree: the authoritative record, and the least thread-safe
 *         object in the program.
 *
 * Wraps libsgfc++ — the move tree, the navigation cursor, variations, markup,
 * comments, the result, and save/load of both the daily session document and
 * external files. `buildBoardFromMoves()` reconstructs any position by replaying
 * from the root with local capture and ko logic, so the board on screen never
 * depends on an engine's opinion.
 *
 * **Owned exclusively by the game thread while a game is running.** The const
 * accessors take no lock and the mutex covers neither the readers nor half the
 * mutators, so reading this from the UI thread is an observed crash rather than
 * a theoretical one — the UI reads `GobanModel::snapshot()` instead (ADR-0006).
 * The few direct readers that remain are explicit user actions listed in that
 * ADR, and they run with the loop stopped.
 */
#ifndef GOBAN_GAMERECORD_H
#define GOBAN_GAMERECORD_H

#include <memory>
#include <algorithm>
#include <optional>
#include "Board.h"

#include "SGF.h"
#include "GameState.h"
#include <mutex>
#include <atomic>

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

    void move(const Move& move, bool insertAsFirst = true);
    void branchFromFinishedGame(const Move& move);  // Copy path and branch (preserves original)

    void annotate(const std::string& comment) const;

    void initGame(int boardSize, float komi, int handicap, const std::string& blackPlayer, const std::string& whitePlayer);

    void updatePlayers(const std::string& blackPlayer, const std::string& whitePlayer);

    void updateKomi(float komi);

    /// False only when the write itself failed. An empty record is not a
    /// failure — there is nothing to write — so it reports success.
    bool saveAs(const std::string& fileName);

    [[nodiscard]] std::string getDefaultFileName() const { return defaultFileName; }
    void setDefaultFileName(const std::string& fileName) { defaultFileName = fileName; }
    void clearSession();  // Clear doc to start new session (used by archive)
    [[nodiscard]] size_t getNumGames() const { return numGames; }
    [[nodiscard]] bool hasNewMoves() const { return gameHasNewMoves; }
    /// Atomic: the Save button reads it every frame from the UI thread while
    /// the game thread sets it on each move and clears it on each autosave.
    /// A plain bool here is a data race, and unlike the tree walks it cannot
    /// be answered from GameSnapshot — saving is not a position change, so it
    /// has no publish point.
    [[nodiscard]] bool hasUnsavedChanges() const { return unsavedChanges.load(); }

    void setSuppressSessionCopy(bool suppress) { suppressSessionCopy = suppress; }

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

    // Unicode-safe file I/O (bypasses libsgfc's fopen which fails on non-ASCII paths)
    static std::optional<std::string> readFileContent(const std::string& filePath);
    static bool writeFileContent(const std::string& filePath, const std::string& content);

private:

    void appendGameToDocument();

    /// Records that the game now lives in the daily session document, clearing
    /// any external-file reference. Caller must hold the mutex.
    void markGameInSessionDocument();

    // Internal save without mutex lock (for use within already-locked methods)
    bool saveAsInternal(const std::string& fileName);

    // Helper to extract Move from SGF node (reduces code duplication)
    static std::optional<Move> extractMoveFromNode(
        const std::shared_ptr<LibSgfcPlusPlus::ISgfcNode>& node,
        int boardSizeColumns);

    // Tree traversal helpers (SGF as single source of truth)
    [[nodiscard]] size_t getTreeDepth() const;  // Depth from root to currentNode
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
    std::atomic<bool> unsavedChanges{false};  // True when changes made since last save
    bool suppressSessionCopy = false;  // True in tsumego mode: don't copy branches to daily session

    // Loaded external SGF document (for game cycling with PageUp/PageDown)
    std::shared_ptr<LibSgfcPlusPlus::ISgfcDocument> loadedExternalDoc;
    std::string loadedFilePath;  // Path of currently loaded SGF file (for session restore)

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

    /// Every move from the root to the cursor, in order.
    ///
    /// Public for one caller: `GobanModel::publishSnapshot()`, which copies it
    /// into `GameSnapshot::pathMoves` so the analysis thread can reach the
    /// position on screen without walking this tree (ADR-0006, ADR-0007). It is
    /// a tree read like every other const accessor here, and carries the same
    /// obligation — only whoever owns the record may call it.
    [[nodiscard]] std::vector<Move> getPathFromRoot() const;

    // Multi-variation support
    [[nodiscard]] std::vector<Move> getVariations() const;
    bool navigateToChild(const Move& move, bool promoteToMainLine = false);
    void promoteCurrentPathToMainLine() const;

    // Check if current node has BM (Bad Move) property
    [[nodiscard]] bool isBadMove() const;

    // Check if any node from root to current position has BM
    [[nodiscard]] bool isOnBadMovePath() const;

    // Mark current node with BM property
    void markBadMove();

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
    [[nodiscard]] int getLoadedGameIndex() const;
    [[nodiscard]] size_t getLoadedGameCount() const;
    [[nodiscard]] const std::string& getLoadedFilePath() const { return loadedFilePath; }
    bool switchToGame(int gameIndex, SGFGameInfo& gameInfo, bool startAtRoot = false);

    // Tree path for session persistence (compact format)
    struct TreePath {
        int length = 0;                    // Total navigation depth
        std::vector<int> branchChoices;    // Choices at multi-child nodes only
    };
    [[nodiscard]] TreePath getTreePath() const;
    bool navigateToTreePath(int pathLength, const std::vector<int>& branchChoices);
};

#endif //GOBAN_GAMERECORD_H
