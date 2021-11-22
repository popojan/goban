#include "GameRecord.h"
#include "SGF.h"
#include <iomanip>
#include <chrono>

using namespace LibSgfcPlusPlus;

GameRecord::GameRecord():
        vF(F::CreatePropertyValueFactory()),
        pF(F::CreatePropertyFactory()),
        currentNode(0),
        game(0),
        builder(0),
        doc(F::CreateDocument())
{
    std::time_t t = std::time(nullptr);
    std::tm time (*std::localtime(&t));
    std::ostringstream ss;
    ss << "./data/sgf/" << std::put_time(&time, "%Y-%m-%dT%H:%M:%S") << ".sgf";
    defaultFileName = ss.str();
}

void GameRecord::move(const Move& move)  {

    using namespace LibSgfcPlusPlus;

    history.push_back(move);

    std::vector<std::shared_ptr<ISgfcProperty> > properties;

    auto newNode(F::CreateNode());

    if (move == Move::NORMAL || move == Move::PASS){
        auto pos = move == Move::PASS ? "" : move.pos.toSgf(this->boardSize.Columns);
        auto col = move.col == Color::BLACK ? SgfcColor::Black : SgfcColor::White;
        auto type = (move.col == Color::BLACK ? T::B : T::W);
        auto value (vF->CreateGoMovePropertyValue(pos, boardSize, col));
        std::shared_ptr<ISgfcProperty> property(pF->CreateProperty(type, value));
        properties.push_back(property);
        newNode->SetProperties(properties);
        builder->InsertChild(currentNode, newNode, currentNode->GetFirstChild());
        currentNode = newNode;
    }
}

void GameRecord::setHandicapStones(const std::vector<Position> &stones) {
    using namespace LibSgfcPlusPlus;

    auto col = SgfcColor::Black;
    auto type = T::AB;

    auto newNode(F::CreateNode());
    std::vector<std::shared_ptr<ISgfcProperty> > properties;
    for(auto stone: stones) {
        auto pos = stone.toSgf(boardSize.Columns);
        auto value (vF->CreateGoMovePropertyValue(pos, boardSize, col));
        std::shared_ptr<ISgfcProperty> property(pF->CreateProperty(type, value));
        properties.push_back(property);
    }
    newNode->SetProperties(properties);
    builder->InsertChild(currentNode, newNode, currentNode->GetFirstChild());
    currentNode = newNode;
}

void GameRecord::initGame(int boardSize, float komi, int handicap, const std::string& blackPlayer, const std::string& whitePlayer) {
    history.clear();
    this->boardSize.Columns = this->boardSize.Rows = boardSize;

    game = F::CreateGame();
    builder = game->GetTreeBuilder();
    currentNode = F::CreateNode();

    using namespace LibSgfcPlusPlus;
    std::vector<std::shared_ptr<ISgfcProperty> > properties;

    auto ha(pF->CreateProperty(T::HA, vF->CreateNumberPropertyValue(handicap)));
    auto bp(pF->CreateProperty(T::PB, vF->CreateSimpleTextPropertyValue(blackPlayer)));
    auto wp(pF->CreateProperty(T::PW, vF->CreateSimpleTextPropertyValue(whitePlayer)));
    auto km(pF->CreateProperty(T::KM, vF->CreateRealPropertyValue(komi)));
    auto sz(pF->CreateBoardSizeProperty(vF->CreateNumberPropertyValue(boardSize)));
    auto us(pF->CreateProperty(T::US,vF->CreateSimpleTextPropertyValue(
            "Red Carpet Goban")));
    auto gn(pF->CreateProperty(T::GN,vF->CreateSimpleTextPropertyValue(
            "Just another game of go")));
    std::ostringstream ss;
    auto now = std::chrono::system_clock::now();

    std::time_t t = std::time(nullptr);
    std::tm time (*std::localtime(&t));
    ss << std::put_time(&time, "%Y-%m-%d");

    auto dt(pF->CreateProperty(T::DT,vF->CreateSimpleTextPropertyValue(
            ss.str())));

    properties.push_back(us);
    properties.push_back(gn);
    properties.push_back(dt);
    properties.push_back(ha);
    properties.push_back(bp);
    properties.push_back(wp);
    properties.push_back(sz);
    properties.push_back(km);
    currentNode->SetProperties(properties);
    game->SetRootNode(currentNode);
    doc->AppendGame(game);
}

void GameRecord::finalizeGame(const GameState::Result& result) {
    using namespace LibSgfcPlusPlus;

    auto type = T::RE;
    std::ostringstream ss;
    ss << (result.delta > 0.0 ? "W+" : "B+");
    if(result.reason == GameState::RESIGNATION)
        ss << "R";
    else
        ss << std::abs(result.delta);

    auto value (vF->CreateSimpleTextPropertyValue(ss.str()));

    std::vector<std::shared_ptr<ISgfcProperty> > properties;
    auto p = game->GetRootNode()->GetProperties();
    std::copy(p.begin(), p.end(), std::back_inserter(properties));
    std::shared_ptr<ISgfcProperty> property(pF->CreateProperty(type, value));
    properties.push_back(property);

    game->GetRootNode()->SetProperties(properties);

}
void GameRecord::saveAs(const std::string& fileName) {
    std::string fn(fileName.length() > 0 ? fileName : defaultFileName);

    std::shared_ptr<LibSgfcPlusPlus::ISgfcDocumentWriter> writer(F::CreateDocumentWriter());
    try {
        writer->WriteSgfFile(doc, fn);
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