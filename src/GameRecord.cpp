#include "GameRecord.h"

void GameRecord::move(const Move& move)  {

    history.push_back(move);

    using namespace LibSgfcPlusPlus;

    auto pos = move.pos.toSgf(this->boardSize.Columns);
    auto col = move.col == Color::BLACK ? SgfcColor::Black : SgfcColor::White;
    auto type = (move.col == Color::BLACK ? T::B : T::W);
    auto value (vF->CreateGoMovePropertyValue(pos, boardSize, col));
    std::shared_ptr<ISgfcProperty> property(pF->CreateProperty(type, value));
    std::vector<std::shared_ptr<ISgfcProperty> > properties; //(node->GetProperties());
    properties.push_back(property);

    auto newNode(F::CreateNode());
    newNode->SetProperties(properties);

    builder->AppendChild(currentNode, newNode);
    currentNode = newNode;
}

void GameRecord::clear(int boardSize) {
    history.clear();
    auto size = vF->CreateNumberPropertyValue(boardSize);
    this->boardSize.Columns = this->boardSize.Rows = boardSize;
}

void GameRecord::saveAs(const std::string& fileName) {
    if(doc->GetGames().empty()) {
        doc->AppendGame(game);
    }
    std::shared_ptr<LibSgfcPlusPlus::ISgfcDocumentWriter> writer(F::CreateDocumentWriter());
    try {
        writer->WriteSgfFile(doc, fileName);
        spdlog::info("Writing sgf file [{}] success!", fileName);
    } catch (std::exception& ex) {
        spdlog::error("Writing sgf file [{}] failed: {}", fileName, ex.what());
    };
}

void  GameRecord::undo() {
    if(currentNode->HasParent()) {
        history.pop_back();
        currentNode = currentNode->GetParent();
    }
}