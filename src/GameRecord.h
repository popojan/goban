#ifndef GOBAN_GAMERECORD_H
#define GOBAN_GAMERECORD_H

#define LIBSGFCPLUSPLUS_STATIC_DEFINE

#include <memory>
#include "Board.h"

#include "SGF.h"
#include "GameState.h"

class GameRecord {
public:

    GameRecord();
    int moveCount() { return history.size(); }

    void undo();

    template <class UnaryFunction>
    void replay(UnaryFunction fMove) {
        std::for_each(history.begin(), history.end(), fMove);
    }

    const Move& lastMove() const  { return history.back(); }

    void move(const Move& move);

    void initGame(int boardSize, float komi, int handicap, const std::string& blackPlayer, const std::string& whitePlayer);

    void saveAs(const std::string& fileName);

    void setHandicapStones(const std::vector<Position>& stones);

    void finalizeGame(const GameState::Result& result);
private:

    typedef LibSgfcPlusPlus::SgfcPlusPlusFactory F;
    typedef LibSgfcPlusPlus::SgfcPropertyType T;
    typedef LibSgfcPlusPlus::SgfcPropertyValueType V;
    typedef LibSgfcPlusPlus::SgfcConstants C;
    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyValueFactory> vF;
    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyFactory> pF;

    std::shared_ptr<LibSgfcPlusPlus::ISgfcNode> currentNode, rootNode;
    std::shared_ptr<LibSgfcPlusPlus::ISgfcGame> game;
    std::shared_ptr<LibSgfcPlusPlus::ISgfcTreeBuilder> builder;
    std::shared_ptr<LibSgfcPlusPlus::ISgfcDocument> doc;
    LibSgfcPlusPlus::SgfcBoardSize boardSize;
    std::vector<Move> history;
    std::string defaultFileName;

};

#endif //GOBAN_GAMERECORD_H
