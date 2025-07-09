#ifndef GOBAN_GAMERECORD_H
#define GOBAN_GAMERECORD_H

#include <memory>
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

    void move(const Move& move);

    void annotate(const std::string& comment);

    void initGame(int boardSize, float komi, int handicap, const std::string& blackPlayer, const std::string& whitePlayer);

    void saveAs(const std::string& fileName);

    void setHandicapStones(const std::vector<Position>& stones);

    void finalizeGame(const GameState::Result& result);

    [[nodiscard]] const Move secondLastMove() const {
        return history.size() > 1 ? history[history.size() -2]
            : Move(Move::Special::INVALID, Color(Color::BLACK));
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
    bool gameStarted;
};

#endif //GOBAN_GAMERECORD_H
