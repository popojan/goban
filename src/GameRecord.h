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
    size_t moveCount() { return history.size(); }

    void undo();

    template <class UnaryFunction>
    void replay(UnaryFunction fMove) {
        std::for_each(history.begin(), history.end(), fMove);
    }

    [[nodiscard]] const Move lastMove() const  { return history.back(); }
    
    [[nodiscard]] const Move lastStoneMove() const {
        for (auto it = history.rbegin(); it != history.rend(); ++it) {
            if (*it == Move::NORMAL) {
                return *it;
            }
        }
        return Move(Move::INVALID, Color::EMPTY);
    }

    [[nodiscard]] std::pair<Move, size_t> lastStoneMoveIndex() const {
        for (auto it = history.rbegin(); it != history.rend(); ++it) {
            if (*it == Move::NORMAL) {
                size_t index = std::distance(history.begin(), it.base()) - 1;
                return std::make_pair(*it, index + 1); // +1 for 1-based move numbering
            }
        }
        return std::make_pair(Move(Move::INVALID, Color::EMPTY), 0);
    }

    void move(const Move& move);

    void annotate(const std::string& comment);

    void initGame(int boardSize, float komi, int handicap, const std::string& blackPlayer, const std::string& whitePlayer);

    void saveAs(const std::string& fileName);

    void setHandicapStones(const std::vector<Position>& stones);

    void finalizeGame(float scoreDelta);

    [[nodiscard]] const Move secondLastMove() const {
        return history.size() > 1 ? history[history.size() -2]
            : Move(Move::Special::INVALID, Color::BLACK);
    }


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

    typedef LibSgfcPlusPlus::SgfcPlusPlusFactory F;
    typedef LibSgfcPlusPlus::SgfcPropertyType T;
    typedef LibSgfcPlusPlus::SgfcPropertyValueType V;
    typedef LibSgfcPlusPlus::SgfcConstants C;

    std::shared_ptr<LibSgfcPlusPlus::ISgfcNode> currentNode;
    std::shared_ptr<LibSgfcPlusPlus::ISgfcGame> game;
    std::shared_ptr<LibSgfcPlusPlus::ISgfcDocument> doc;
    std::shared_ptr<LibSgfcPlusPlus::ISgfcTreeBuilder> builder;
    LibSgfcPlusPlus::SgfcBoardSize boardSize;

    std::vector<Move> history;
    std::string defaultFileName;
    std::mutex mutex;
    size_t numGames;
    bool gameHasNewMoves;

    // SGF Navigation state
    std::vector<Move> loadedMoves;  // Full SGF moves (preserved after load)
    size_t viewPosition = 0;        // Current position in loadedMoves

public:
    // Navigation methods
    [[nodiscard]] bool hasNextMove() const { return viewPosition < loadedMoves.size(); }
    [[nodiscard]] bool hasPreviousMove() const { return viewPosition > 0; }
    [[nodiscard]] const Move& getNextMove() const { return loadedMoves[viewPosition]; }
    [[nodiscard]] size_t getViewPosition() const { return viewPosition; }
    [[nodiscard]] size_t getLoadedMovesCount() const { return loadedMoves.size(); }
    void advancePosition() { if (hasNextMove()) viewPosition++; }
    void retreatPosition() { if (hasPreviousMove()) viewPosition--; }
    void resetNavigation() { viewPosition = 0; loadedMoves.clear(); }
    [[nodiscard]] bool isNavigating() const { return !loadedMoves.empty(); }
    [[nodiscard]] bool isAtEndOfNavigation() const { return !loadedMoves.empty() && viewPosition >= loadedMoves.size(); }

    // Multi-variation support
    [[nodiscard]] std::vector<Move> getVariations() const;
    bool navigateToChild(const Move& move);  // Navigate to specific variation

    // History-only update (for following existing branches without creating SGF nodes)
    void pushHistory(const Move& move) { history.push_back(move); }
};

#endif //GOBAN_GAMERECORD_H
