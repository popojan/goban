#include "GameRecord.h"
#include <iomanip>
#include <chrono>

using namespace LibSgfcPlusPlus;

const std::array<std::string, GameRecord::LAST_EVENT> GameRecord::eventNames = {
        "", "switched_player:", "kibitz_move:"
};

GameRecord::GameRecord():
        currentNode(nullptr),
        game(nullptr),
        doc(nullptr),
        numGames(0u),
        gameStarted(false)
{
    std::time_t t = std::time(nullptr);
    std::tm time {};
    time = *std::localtime(&t);
    std::ostringstream ss;
    ss << "./data/sgf/" << std::put_time(&time, "%Y-%m-%dT%H-%M-%S") << ".sgf";
    defaultFileName = ss.str();
}

void GameRecord::move(const Move& move)  {

    std::lock_guard<std::mutex> lock(mutex);

    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyValueFactory> vF(F::CreatePropertyValueFactory());
    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyFactory> pF(F::CreatePropertyFactory());

    using namespace LibSgfcPlusPlus;

    history.push_back(move);

    std::vector<std::shared_ptr<ISgfcProperty> > properties;

    auto newNode(F::CreateNode());

    if (move == Move::NORMAL || move == Move::PASS){
        if(!gameStarted) {
            if(doc == nullptr)
                doc = F::CreateDocument(game);
            else
                doc->AppendGame(game);
            ++numGames;
            gameStarted = true;
        }
        auto col = move.col == Color::BLACK ? SgfcColor::Black : SgfcColor::White;
        auto type = (move.col == Color::BLACK ? T::B : T::W);
        std::shared_ptr<ISgfcPropertyValue> value{nullptr};

        if(move == Move::NORMAL) {
            auto pos = move.pos.toSgf(boardSize.Columns);
            value = vF->CreateGoMovePropertyValue(pos, boardSize, col);
        } else {
            value =  vF->CreateGoMovePropertyValue(col);
        }

        std::shared_ptr<ISgfcProperty> property(pF->CreateProperty(type, value));
        properties.push_back(property);
        newNode->SetProperties(properties);
        game->GetTreeBuilder()->InsertChild(currentNode, newNode, currentNode->GetFirstChild());
        currentNode = newNode;
    }
}

void GameRecord::annotate(const std::string& comment) {
    if(currentNode == nullptr)
        return;
    auto properties = currentNode->GetProperties();
    std::string appendTo("");
    std::vector<std::shared_ptr<ISgfcProperty> > newProperties;

    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyValueFactory> vF(F::CreatePropertyValueFactory());
    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyFactory> pF(F::CreatePropertyFactory());
    for(auto property: properties) {
        if(property->GetPropertyType() == T::C) {
            auto val = property->GetPropertyValue();
            appendTo = val->ToSingleValue()->GetRawValue();
        }
        else {
            newProperties.push_back(property);
        }
    }
    std::ostringstream val;
    val << appendTo << comment << " ";
    auto property = pF->CreateProperty(T::C, vF->CreateSimpleTextPropertyValue(val.str()));
    newProperties.push_back(property);
    currentNode->SetProperties(newProperties);
}

void GameRecord::setHandicapStones(const std::vector<Position>& stones) {
    using namespace LibSgfcPlusPlus;

    std::lock_guard<std::mutex> lock(mutex);

    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyValueFactory> vF(F::CreatePropertyValueFactory());
    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyFactory> pF(F::CreatePropertyFactory());

    if(stones.empty())
        return;

    auto type = T::AB;

    auto newNode(F::CreateNode());
    std::vector<std::shared_ptr<ISgfcProperty> > properties;
    std::shared_ptr<ISgfcProperty> property(pF->CreateProperty(type));
    std::vector<std::shared_ptr<ISgfcPropertyValue> > values;
    for(auto stone: stones) {
        auto pos = stone.toSgf(boardSize.Columns);
        values.push_back(vF->CreateStonePropertyValue(pos));
    }
    property->SetPropertyValues(values);
    properties.push_back(property);
    newNode->SetProperties(properties);
    game->GetTreeBuilder()->InsertChild(currentNode, newNode, currentNode->GetFirstChild());
    currentNode = newNode;
}

void GameRecord::initGame(int boardSizeInt, float komi, int handicap, const std::string& blackPlayer, const std::string& whitePlayer) {

    std::lock_guard<std::mutex> lock(mutex);

    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyValueFactory> vF(F::CreatePropertyValueFactory());
    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyFactory> pF(F::CreatePropertyFactory());

    history.clear();

    boardSize.Columns = boardSizeInt;
    boardSize.Rows = boardSizeInt;

    game = F::CreateGame();
    currentNode = game->GetRootNode();
    gameStarted = false;

    using namespace LibSgfcPlusPlus;
    std::vector<std::shared_ptr<ISgfcProperty> > properties;

    auto ha(pF->CreateProperty(T::HA, vF->CreateNumberPropertyValue(handicap)));
    auto bp(pF->CreateProperty(T::PB, vF->CreateSimpleTextPropertyValue(blackPlayer)));
    auto wp(pF->CreateProperty(T::PW, vF->CreateSimpleTextPropertyValue(whitePlayer)));
    auto km(pF->CreateProperty(T::KM, vF->CreateRealPropertyValue(komi)));
    auto sz(pF->CreateBoardSizeProperty(vF->CreateNumberPropertyValue(boardSizeInt)));
    auto us(pF->CreateProperty(T::US,vF->CreateSimpleTextPropertyValue(
            "Red Carpet Goban")));

    std::string val;
    {
        std::ostringstream ss;
        ss << std::string("Game #") << (numGames+1);
        val = ss.str();
    }
    auto gn(pF->CreateProperty(T::GN, vF->CreateSimpleTextPropertyValue(val)));
    {
        std::ostringstream ss;
        std::time_t t = std::time(nullptr);
        std::tm time {};
        time = *std::localtime(&t);
        ss << std::put_time(&time, "%Y-%m-%d");
        val = ss.str();
    }

    auto dt(pF->CreateProperty(T::DT,vF->CreateSimpleTextPropertyValue(val)));

    properties.push_back(us);
    properties.push_back(gn);
    properties.push_back(dt);
    properties.push_back(ha);
    properties.push_back(bp);
    properties.push_back(wp);
    properties.push_back(sz);
    properties.push_back(km);
    currentNode->SetProperties(properties);

}

void GameRecord::finalizeGame(const GameState::Result& result) {
    using namespace LibSgfcPlusPlus;

    std::lock_guard<std::mutex> lock(mutex);

    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyValueFactory> vF(F::CreatePropertyValueFactory());
    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyFactory> pF(F::CreatePropertyFactory());

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
    std::lock_guard<std::mutex> lock(mutex);

    std::string fn(fileName.length() > 0 ? fileName : defaultFileName);

    std::shared_ptr<LibSgfcPlusPlus::ISgfcDocumentWriter> writer(F::CreateDocumentWriter());
    try {
        writer->WriteSgfFile(doc, fn);
        spdlog::info("Writing sgf file [{}] success!", fn);
    } catch (std::exception& ex) {
        spdlog::error("Writing sgf file [{}] failed: {}", fn, ex.what());
    }
}

void GameRecord::undo() {

    std::lock_guard<std::mutex> lock(mutex);

    if(currentNode->HasParent()) {
        history.pop_back();
        currentNode = currentNode->GetParent();
    }
}