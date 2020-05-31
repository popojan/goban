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
#include <SgfcConstants.h>

class GameRecord {
public:

    GameRecord():
        vF(F::CreatePropertyValueFactory()),
        pF(F::CreatePropertyFactory()),
        currentNode(F::CreateNode()),
        game(F::CreateGame(currentNode)),
        doc()
        {
            //clear();s
        }

    int moveCount() { return history.size(); }

    void undo() { history.pop_back(); }

    template <class UnaryFunction>
    void replay(UnaryFunction fMove) {
        std::for_each(history.begin(), history.end(), fMove);
    }

    const Move& lastMove() const  { return history.back(); }
    void move(const Move& move) {

        history.push_back(move);

        using namespace LibSgfcPlusPlus;

        auto pos = move.pos.toSgf();
        auto col = move.col == Color::BLACK ? SgfcColor::Black : SgfcColor::White;
        auto type = (move.col == Color::BLACK ? SgfcPropertyType::B : SgfcPropertyType::W);
        auto value (vF->CreateGoMovePropertyValue(pos, boardSize, col));
        std::shared_ptr<ISgfcProperty> property = pF->CreateProperty(type, value);
        std::shared_ptr<ISgfcTreeBuilder> builder(game->GetTreeBuilder());
        auto newNode(F::CreateNode());
        std::vector<std::shared_ptr<ISgfcProperty> > properties; //(node->GetProperties());
        properties.push_back(property);
        newNode->SetProperties(properties);
        builder->AppendChild(currentNode, newNode);
        currentNode = newNode;
    }
    void clear(int boardSize) {
        history.clear();

        auto size = vF->CreateNumberPropertyValue(boardSize);
        //this->boardSizeProperty = pF->CreateBoardSizeProperty(size);
        this->boardSize.Columns = this->boardSize.Rows = boardSize;

    }
    void saveAs(const std::string& fileName) { }

private:

    typedef LibSgfcPlusPlus::SgfcPlusPlusFactory F;
    typedef LibSgfcPlusPlus::SgfcPropertyType T;
    typedef LibSgfcPlusPlus::SgfcPropertyValueType V;
    typedef LibSgfcPlusPlus::SgfcConstants C;
    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyValueFactory> vF;
    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyFactory> pF;

    std::shared_ptr<LibSgfcPlusPlus::ISgfcNode> currentNode;
    std::shared_ptr<LibSgfcPlusPlus::ISgfcGame> game;
    std::shared_ptr<LibSgfcPlusPlus::ISgfcDocument> doc;
    LibSgfcPlusPlus::SgfcBoardSize boardSize;
    std::vector<Move> history;


};

#endif //GOBAN_GAMERECORD_H
