#ifndef GOBAN_GAMERECORD_H
#define GOBAN_GAMERECORD_H

#define LIBSGFCPLUSPLUS_STATIC_DEFINE

#include <memory>
#include "Board.h"

#include <ISgfcTreeBuilder.h>
#include <SgfcPlusPlusFactory.h>
#include <ISgfcPropertyFactory.h>
#include <ISgfcPropertyValueFactory.h>
#include <ISgfcPropertyValue.h>
#include <SgfcPropertyType.h>
#include <SgfcPlusPlusExport.h>
#include <SgfcPropertyValueType.h>
#include <ISgfcGoMovePropertyValue.h>
#include <SgfcGameType.h>
#include <ISgfcGame.h>
#include <ISgfcNode.h>
#include <ISgfcDocument.h>
#include <ISgfcDocumentWriter.h>
#include <SgfcConstants.h>

class GameRecord {
public:

    GameRecord():
        vF(F::CreatePropertyValueFactory()),
        pF(F::CreatePropertyFactory()),
        currentNode(F::CreateNode()),
        game(F::CreateGame(currentNode)),
        builder(game->GetTreeBuilder()),
        doc(F::CreateDocument())
        {
            //clear();s
        }

    int moveCount() { return history.size(); }

    void undo();

    template <class UnaryFunction>
    void replay(UnaryFunction fMove) {
        std::for_each(history.begin(), history.end(), fMove);
    }

    const Move& lastMove() const  { return history.back(); }

    void move(const Move& move);

    void clear(int boardSize);

    void saveAs(const std::string& fileName);

private:

    typedef LibSgfcPlusPlus::SgfcPlusPlusFactory F;
    typedef LibSgfcPlusPlus::SgfcPropertyType T;
    typedef LibSgfcPlusPlus::SgfcPropertyValueType V;
    typedef LibSgfcPlusPlus::SgfcConstants C;
    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyValueFactory> vF;
    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyFactory> pF;

    std::shared_ptr<LibSgfcPlusPlus::ISgfcNode> currentNode;
    std::shared_ptr<LibSgfcPlusPlus::ISgfcGame> game;
    std::shared_ptr<LibSgfcPlusPlus::ISgfcTreeBuilder> builder;
    std::shared_ptr<LibSgfcPlusPlus::ISgfcDocument> doc;
    LibSgfcPlusPlus::SgfcBoardSize boardSize;
    std::vector<Move> history;


};

#endif //GOBAN_GAMERECORD_H
