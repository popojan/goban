#include "GameRecord.h"
#include "Configuration.h"
#include <iomanip>
#include <chrono>
#include <spdlog/spdlog.h>

extern std::shared_ptr<Configuration> config;

using namespace LibSgfcPlusPlus;

const std::array<std::string, GameRecord::LAST_EVENT> GameRecord::eventNames = {
        "switched_player:", "kibitz_move:"
};

std::optional<Move> GameRecord::extractMoveFromNode(
    const std::shared_ptr<ISgfcNode>& node,
    int boardSizeColumns)
{
    if (!node) return std::nullopt;

    for (const auto& property : node->GetProperties()) {
        if (property->GetPropertyType() == SgfcPropertyType::B ||
            property->GetPropertyType() == SgfcPropertyType::W) {

            auto moveValue = std::dynamic_pointer_cast<ISgfcGoMovePropertyValue>(
                property->GetPropertyValue());
            if (!moveValue) continue;

            Color color = (property->GetPropertyType() == SgfcPropertyType::B)
                ? Color::BLACK : Color::WHITE;

            if (moveValue->GetGoMove()->IsPassMove()) {
                return Move(Move::PASS, color);
            } else {
                auto sgfPoint = moveValue->GetGoMove()->GetStoneLocation();
                Position pos = Position::fromSgf(
                    sgfPoint->GetPosition(SgfcGoPointNotation::Sgf),
                    boardSizeColumns);
                return Move(pos, color);
            }
        }
    }
    return std::nullopt;
}

size_t GameRecord::getTreeDepth() const {
    if (!game || !currentNode) return 0;

    size_t depth = 0;
    auto node = currentNode;
    auto root = game->GetRootNode();

    while (node && node != root) {
        // Only count nodes that have moves (skip root/setup nodes)
        if (extractMoveFromNode(node, boardSize.Columns).has_value()) {
            depth++;
        }
        node = node->GetParent();
    }
    return depth;
}

std::vector<Move> GameRecord::getPathFromRoot() const {
    std::vector<Move> path;
    if (!game || !currentNode) return path;

    // Collect nodes from currentNode back to root
    std::vector<std::shared_ptr<ISgfcNode>> nodes;
    auto node = currentNode;
    auto root = game->GetRootNode();

    while (node && node != root) {
        nodes.push_back(node);
        node = node->GetParent();
    }

    // Reverse to get root-to-current order, extract moves
    for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
        if (auto move = extractMoveFromNode(*it, boardSize.Columns)) {
            path.push_back(*move);
        }
    }
    return path;
}

bool GameRecord::isAtRoot() const {
    if (!game || !currentNode) return true;
    auto root = game->GetRootNode();
    // At root if currentNode IS root, or is first child of root (setup node)
    return currentNode == root || currentNode->GetParent() == root;
}

Move GameRecord::lastMove() const {
    if (auto move = extractMoveFromNode(currentNode, boardSize.Columns)) {
        return *move;
    }
    return Move(Move::INVALID, Color::EMPTY);
}

Move GameRecord::lastStoneMove() const {
    auto node = currentNode;
    auto root = game ? game->GetRootNode() : nullptr;

    while (node && node != root) {
        if (auto move = extractMoveFromNode(node, boardSize.Columns)) {
            if (*move == Move::NORMAL) {
                return *move;
            }
        }
        node = node->GetParent();
    }
    return Move(Move::INVALID, Color::EMPTY);
}

std::pair<Move, size_t> GameRecord::lastStoneMoveIndex() const {
    auto node = currentNode;
    auto root = game ? game->GetRootNode() : nullptr;
    size_t depth = getTreeDepth();

    while (node && node != root) {
        if (auto move = extractMoveFromNode(node, boardSize.Columns)) {
            if (*move == Move::NORMAL) {
                return std::make_pair(*move, depth);
            }
        }
        node = node->GetParent();
        depth--;
    }
    return std::make_pair(Move(Move::INVALID, Color::EMPTY), 0);
}

Move GameRecord::secondLastMove() const {
    if (!currentNode || !currentNode->HasParent()) {
        return Move(Move::INVALID, Color::BLACK);
    }
    auto parent = currentNode->GetParent();
    if (auto move = extractMoveFromNode(parent, boardSize.Columns)) {
        return *move;
    }
    return Move(Move::INVALID, Color::BLACK);
}

GameRecord::GameRecord():
        currentNode(nullptr),
        game(nullptr),
        doc(nullptr),
        numGames(0u),
        gameHasNewMoves(false)
{
    // Read games path from config
    std::string gamesPath = "./games";
    if (config && config->data.contains("sgf_dialog")) {
        gamesPath = config->data["sgf_dialog"].value("games_path", "./games");
    }

    std::time_t t = std::time(nullptr);
    std::tm time {};
    time = *std::localtime(&t);
    std::ostringstream ss;
    ss << gamesPath << "/" << std::put_time(&time, "%Y-%m-%d") << ".sgf";
    defaultFileName = ss.str();
}

void GameRecord::move(const Move& move)  {

    std::lock_guard<std::mutex> lock(mutex);

    // SGF tree is the source of truth - no separate history vector needed
    if (!game) {
        spdlog::warn("GameRecord::move() called with no game - creating new game");
        // Create a minimal game if none exists
        game = F::CreateGame();
        currentNode = game->GetRootNode();
    }

    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyValueFactory> vF(F::CreatePropertyValueFactory());
    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyFactory> pF(F::CreatePropertyFactory());

    using namespace LibSgfcPlusPlus;

    std::vector<std::shared_ptr<ISgfcProperty> > properties;

    auto newNode(F::CreateNode());

    if (move == Move::NORMAL || move == Move::PASS) {
        // If creating a branch (existing children), invalidate old result
        // RE property reflects result of main line only
        if (currentNode->GetFirstChild() && hasGameResult()) {
            spdlog::debug("Removing game result - creating new main line branch");
            removeGameResult();
        }

        auto col = move.col == Color::BLACK ? SgfcColor::Black : SgfcColor::White;
        auto type = (move.col == Color::BLACK ? T::B : T::W);
        std::shared_ptr<ISgfcPropertyValue> value{nullptr};

        if (move == Move::NORMAL) {
            auto pos = move.pos.toSgf(boardSize.Columns);
            value = vF->CreateGoMovePropertyValue(pos, boardSize, col);
        } else {
            value = vF->CreateGoMovePropertyValue(col);
        }

        std::shared_ptr<ISgfcProperty> property(pF->CreateProperty(type, value));
        properties.push_back(property);
        newNode->SetProperties(properties);
        game->GetTreeBuilder()->InsertChild(currentNode, newNode, currentNode->GetFirstChild());
        currentNode = newNode;
        if (!gameHasNewMoves) {
            gameHasNewMoves = true;
            appendGameToDocument();
        }
    }
    else if (move == Move::RESIGN) {
        // Resignation: set RE property in root node (W+R or B+R)
        // Black resigning means White wins, and vice versa
        std::string result = (move.col == Color::BLACK) ? "W+R" : "B+R";
        auto value = vF->CreateSimpleTextPropertyValue(result);
        auto property = pF->CreateProperty(T::RE, value);

        // Replace existing properties, removing any old RE
        std::vector<std::shared_ptr<ISgfcProperty>> rootProps;
        for (const auto& prop : game->GetRootNode()->GetProperties()) {
            if (prop->GetPropertyType() != T::RE) {
                rootProps.push_back(prop);
            }
        }
        rootProps.push_back(property);
        game->GetRootNode()->SetProperties(rootProps);

        if (!gameHasNewMoves) {
            gameHasNewMoves = true;
            appendGameToDocument();
        }
        spdlog::info("Resignation recorded: {} ({})", result, move.col == Color::BLACK ? "black resigned" : "white resigned");
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

    // Check if day changed since app start (or last game) - update daily session file
    {
        std::time_t t = std::time(nullptr);
        std::tm time {};
        time = *std::localtime(&t);
        std::ostringstream ss;
        ss << std::put_time(&time, "%Y-%m-%d");
        std::string today = ss.str();

        // If defaultFileName doesn't contain today's date, update it
        if (defaultFileName.find(today) == std::string::npos) {
            std::string gamesPath = "./games";
            if (config && config->data.contains("sgf_dialog")) {
                gamesPath = config->data["sgf_dialog"].value("games_path", "./games");
            }
            defaultFileName = gamesPath + "/" + today + ".sgf";
            spdlog::info("Day changed, new session file: {}", defaultFileName);
        }
    }

    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyValueFactory> vF(F::CreatePropertyValueFactory());
    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyFactory> pF(F::CreatePropertyFactory());

    boardSize.Columns = boardSizeInt;
    boardSize.Rows = boardSizeInt;

    // Create fresh SGF game - this IS the reset (SGF tree is source of truth)
    game = F::CreateGame();
    currentNode = game->GetRootNode();
    gameHasNewMoves = false;
    
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
        ss << std::put_time(&time, "%Y-%m-%d %H:%M:%S");
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

void GameRecord::updatePlayers(const std::string& blackPlayer, const std::string& whitePlayer) {
    std::lock_guard<std::mutex> lock(mutex);

    if (!game) return;
    auto root = game->GetRootNode();
    if (!root) return;

    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyValueFactory> vF(F::CreatePropertyValueFactory());
    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyFactory> pF(F::CreatePropertyFactory());

    // Copy existing properties, replacing PB and PW
    std::vector<std::shared_ptr<LibSgfcPlusPlus::ISgfcProperty>> properties;
    for (const auto& prop : root->GetProperties()) {
        if (prop->GetPropertyType() != T::PB && prop->GetPropertyType() != T::PW) {
            properties.push_back(prop);
        }
    }

    // Add updated player names
    properties.push_back(pF->CreateProperty(T::PB, vF->CreateSimpleTextPropertyValue(blackPlayer)));
    properties.push_back(pF->CreateProperty(T::PW, vF->CreateSimpleTextPropertyValue(whitePlayer)));

    root->SetProperties(properties);
}

void GameRecord::finalizeGame(float scoreDelta) {
    using namespace LibSgfcPlusPlus;

    std::lock_guard<std::mutex> lock(mutex);

    // Check if there's already a resignation result - don't overwrite
    for (const auto& prop : game->GetRootNode()->GetProperties()) {
        if (prop->GetPropertyType() == T::RE) {
            auto val = prop->GetPropertyValue()->ToSingleValue()->GetRawValue();
            if (val.find("+R") != std::string::npos) {
                spdlog::debug("finalizeGame: resignation result already set ({}), skipping", val);
                return;
            }
        }
    }

    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyValueFactory> vF(F::CreatePropertyValueFactory());
    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyFactory> pF(F::CreatePropertyFactory());

    // Score-based result
    std::ostringstream ss;
    ss << (scoreDelta < 0.0 ? "W+" : "B+");
    ss << std::abs(scoreDelta);

    auto value(vF->CreateSimpleTextPropertyValue(ss.str()));

    // Copy existing properties, excluding any existing RE (result) property
    std::vector<std::shared_ptr<ISgfcProperty>> properties;
    for (const auto& prop : game->GetRootNode()->GetProperties()) {
        if (prop->GetPropertyType() != T::RE) {
            properties.push_back(prop);
        }
    }
    // Add the new result
    properties.push_back(pF->CreateProperty(T::RE, value));

    game->GetRootNode()->SetProperties(properties);
}

void GameRecord::appendGameToDocument() {

    if (game == nullptr) {
        return;
    }
    
    if (doc == nullptr) {
        doc = F::CreateDocument(game);
    } else {
        doc->AppendGame(game);
        ++numGames;
    }

}

void GameRecord::saveAs(const std::string& fileName) {
    std::lock_guard<std::mutex> lock(mutex);

    if(doc == nullptr || doc->GetGames().empty())
        return;

    // If game was modified, check if main line is still finished
    // If not, remove the RE (result) property to maintain invariant
    if (gameHasNewMoves && game) {
        bool mainLineFinished = isMainLineFinished();
        if (!mainLineFinished && hasGameResult()) {
            spdlog::info("Removing game result - main line is unfinished after modifications");
            removeGameResult();
        }
    }

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

    // SGF tree is source of truth - just move currentNode to parent
    spdlog::debug("GameRecord::undo() called, treeDepth={}, currentNode={}, HasParent={}",
        getTreeDepth(),
        currentNode ? "set" : "null",
        (currentNode && currentNode->HasParent()) ? "yes" : "no");

    if (currentNode && currentNode->HasParent()) {
        auto root = game ? game->GetRootNode() : nullptr;
        // Don't go past root
        if (currentNode != root) {
            currentNode = currentNode->GetParent();
            spdlog::debug("GameRecord::undo() moved to parent, new depth={}", getTreeDepth());
        }
    }
}

bool GameRecord::loadFromSGF(const std::string& fileName, SGFGameInfo& gameInfo, int gameIndex) {
    using namespace LibSgfcPlusPlus;

    std::lock_guard<std::mutex> lock(mutex);

    try {
        auto reader = F::CreateDocumentReader();
        auto loadedDoc = reader->ReadSgfFile(fileName);
        
        if (!loadedDoc || !loadedDoc->IsSgfDataValid()) {
            spdlog::error("Failed to load SGF file [{}] or no games found", fileName);
            return false;
        }

        auto games = loadedDoc->GetDocument()->GetGames();
        if (games.empty()) {
            spdlog::error("No games found in SGF file [{}]", fileName);
            return false;
        }
        
        if (gameIndex < 0 || gameIndex >= static_cast<int>(games.size())) {
            spdlog::error("Invalid game index {} for SGF file [{}] (contains {} games)", 
                         gameIndex, fileName, games.size());
            return false;
        }
        
        auto loadedGame = games[gameIndex];
        auto rootNode = loadedGame->GetRootNode();
        
        spdlog::info("Loading game {} of {} from SGF file [{}]", gameIndex + 1, games.size(), fileName);
        
        gameInfo.boardSize = 19;
        gameInfo.komi = 6.5f;
        gameInfo.handicap = 0;
        gameInfo.blackPlayer = "Black";
        gameInfo.whitePlayer = "White";
        gameInfo.handicapStones.clear();
        gameInfo.gameResult = LibSgfcPlusPlus::SgfcGameResult();

        for (auto property : rootNode->GetProperties()) {
            switch (property->GetPropertyType()) {
                case T::SZ: {
                    auto numberValue = std::dynamic_pointer_cast<ISgfcNumberPropertyValue>(property->GetPropertyValue());
                    if (numberValue) {
                        gameInfo.boardSize = numberValue->GetNumberValue();
                    }
                    break;
                }
                case T::KM: {
                    auto realValue = std::dynamic_pointer_cast<ISgfcRealPropertyValue>(property->GetPropertyValue());
                    if (realValue) {
                        gameInfo.komi = realValue->GetRealValue();
                    }
                    break;
                }
                case T::HA: {
                    auto numberValue = std::dynamic_pointer_cast<ISgfcNumberPropertyValue>(property->GetPropertyValue());
                    if (numberValue) {
                        gameInfo.handicap = numberValue->GetNumberValue();
                    }
                    break;
                }
                case T::PB: {
                    auto textValue = std::dynamic_pointer_cast<ISgfcSimpleTextPropertyValue>(property->GetPropertyValue());
                    if (textValue) {
                        gameInfo.blackPlayer = textValue->GetSimpleTextValue();
                    }
                    break;
                }
                case T::PW: {
                    auto textValue = std::dynamic_pointer_cast<ISgfcSimpleTextPropertyValue>(property->GetPropertyValue());
                    if (textValue) {
                        gameInfo.whitePlayer = textValue->GetSimpleTextValue();
                    }
                    break;
                }
                case T::AB: {
                    for (auto value : property->GetPropertyValues()) {
                        auto stoneValue = std::dynamic_pointer_cast<ISgfcStonePropertyValue>(value);
                        if (stoneValue) {
                            auto sgfPoint = stoneValue->GetStoneValue();
                            Position pos = Position::fromSgf(sgfPoint, gameInfo.boardSize);
                            gameInfo.handicapStones.push_back(pos);
                        }
                    }
                    break;
                }
                case T::RE: {
                    auto textValue = std::dynamic_pointer_cast<ISgfcSimpleTextPropertyValue>(property->GetPropertyValue());
                    if (textValue) {
                        gameInfo.gameResult = SgfcGameResult::FromPropertyValue(textValue->GetSimpleTextValue());
                    }
                    break;
                }
            }
        }

        // Keep the game tree (SGF is single source of truth)
        game = loadedGame;
        gameHasNewMoves = false;

        boardSize.Columns = gameInfo.boardSize;
        boardSize.Rows = gameInfo.boardSize;

        // Traverse to end of main line - currentNode becomes the last move
        currentNode = rootNode;
        auto node = rootNode->GetFirstChild();
        while (node) {
            currentNode = node;
            node = node->GetFirstChild();
        }

        spdlog::info("Successfully loaded SGF file [{}] with {} moves (navigation enabled)", fileName, getTreeDepth());
        return true;

    } catch (const std::exception& ex) {
        spdlog::error("Error loading SGF file [{}]: {}", fileName, ex.what());
        return false;
    }
}

bool GameRecord::hasGameResult() const {
    if (!game) return false;
    auto root = game->GetRootNode();
    if (!root) return false;

    for (const auto& prop : root->GetProperties()) {
        if (prop->GetPropertyType() == T::RE) {
            return true;
        }
    }
    return false;
}

bool GameRecord::isGameFinished() const {
    Move last = lastMove();

    // Game finished by resignation
    if (last == Move::RESIGN) {
        return true;
    }

    // Game finished by double pass
    if (last == Move::PASS && currentNode && currentNode->HasParent()) {
        auto parent = currentNode->GetParent();
        if (auto prevMove = extractMoveFromNode(parent, boardSize.Columns)) {
            if (*prevMove == Move::PASS) {
                return true;
            }
        }
    }

    return false;
}

bool GameRecord::isMainLineFinished() const {
    if (!game) return false;

    // Check if RE property indicates resignation (W+R or B+R)
    for (const auto& prop : game->GetRootNode()->GetProperties()) {
        if (prop->GetPropertyType() == T::RE) {
            auto val = prop->GetPropertyValue()->ToSingleValue()->GetRawValue();
            if (val.find("+R") != std::string::npos) {
                return true;  // Game ended by resignation
            }
        }
    }

    // Traverse main line (following first children) to find the end
    auto node = game->GetRootNode();
    std::shared_ptr<LibSgfcPlusPlus::ISgfcNode> lastMoveNode = nullptr;
    std::shared_ptr<LibSgfcPlusPlus::ISgfcNode> secondLastMoveNode = nullptr;

    while (node) {
        if (extractMoveFromNode(node, boardSize.Columns).has_value()) {
            secondLastMoveNode = lastMoveNode;
            lastMoveNode = node;
        }
        auto children = node->GetChildren();
        node = children.empty() ? nullptr : children[0];  // Follow first child (main line)
    }

    if (!lastMoveNode) {
        return false;  // No moves at all
    }

    auto lastMove = extractMoveFromNode(lastMoveNode, boardSize.Columns);
    if (!lastMove) {
        return false;
    }

    // Check for double pass
    if (*lastMove == Move::PASS && secondLastMoveNode) {
        auto secondLastMove = extractMoveFromNode(secondLastMoveNode, boardSize.Columns);
        if (secondLastMove && *secondLastMove == Move::PASS) {
            return true;
        }
    }

    return false;
}

bool GameRecord::isResignationResult() const {
    if (!game) return false;
    for (const auto& prop : game->GetRootNode()->GetProperties()) {
        if (prop->GetPropertyType() == T::RE) {
            auto val = prop->GetPropertyValue()->ToSingleValue()->GetRawValue();
            return val.find("+R") != std::string::npos;
        }
    }
    return false;
}

bool GameRecord::isAtFinishedGame() const {
    // Current position is a game-ending move (resign or double-pass)
    if (isGameFinished()) return true;

    // At end of game with any result
    // RE property is valid - it's cleared when creating new variations
    if (isAtEndOfNavigation() && hasGameResult()) return true;

    return false;
}

bool GameRecord::shouldShowTerritory() const {
    if (!isAtEndOfNavigation()) return false;

    // Double-pass at current position (not resign)
    if (isGameFinished() && lastMove() == Move::PASS) return true;

    // Has point result (not resignation)
    // RE property is valid - it's cleared when creating new variations
    if (hasGameResult() && !isResignationResult()) return true;

    return false;
}

GameState::Message GameRecord::getResultMessage() const {
    if (!game) return GameState::NONE;

    // Check RE property for result type
    for (const auto& prop : game->GetRootNode()->GetProperties()) {
        if (prop->GetPropertyType() == T::RE) {
            auto val = prop->GetPropertyValue()->ToSingleValue()->GetRawValue();
            if (val.empty() || val == "?" || val == "Void") {
                return GameState::NONE;
            }
            // Resignation: B+R or W+R
            if (val.find("+R") != std::string::npos) {
                // B+R means black resigned, white wins
                return (val[0] == 'W') ? GameState::BLACK_RESIGNED : GameState::WHITE_RESIGNED;
            }
            // Point result: B+5.5, W+3, etc.
            if (val[0] == 'B') {
                return GameState::BLACK_WON;
            } else if (val[0] == 'W') {
                return GameState::WHITE_WON;
            }
        }
    }

    // Fallback: check for double-pass at current position
    if (isGameFinished() && lastMove() == Move::PASS) {
        // Would need score to determine winner, return generic
        return GameState::NONE;
    }

    return GameState::NONE;
}

void GameRecord::removeGameResult() {
    if (!game) return;

    auto root = game->GetRootNode();
    if (!root) return;

    std::vector<std::shared_ptr<LibSgfcPlusPlus::ISgfcProperty>> properties;
    for (const auto& prop : root->GetProperties()) {
        if (prop->GetPropertyType() != T::RE) {
            properties.push_back(prop);
        }
    }
    root->SetProperties(properties);
}

Color GameRecord::getColorToMove() const {
    Move last = lastMove();
    if (last == Move::INVALID) {
        // No moves yet - check if there are handicap stones (AB property)
        if (game && game->GetRootNode()) {
            for (const auto& prop : game->GetRootNode()->GetProperties()) {
                if (prop->GetPropertyType() == T::AB) {
                    // Handicap stones present - White moves first
                    return Color::WHITE;
                }
            }
        }
        return Color::BLACK;  // Default: Black moves first
    }
    // Opposite of last move's color
    return (last.col == Color::BLACK) ? Color::WHITE : Color::BLACK;
}

size_t GameRecord::getLoadedMovesCount() const {
    // Total moves on main line from root to end
    if (!game) return 0;

    size_t count = 0;
    auto node = game->GetRootNode();
    while (node) {
        if (extractMoveFromNode(node, boardSize.Columns).has_value()) {
            count++;
        }
        auto children = node->GetChildren();
        node = children.empty() ? nullptr : children[0];
    }
    return count;
}

bool GameRecord::hasNextMove() const {
    if (!currentNode) return false;
    auto children = currentNode->GetChildren();
    return !children.empty();
}

Move GameRecord::getNextMove() const {
    if (!currentNode) return Move(Move::INVALID, Color::EMPTY);
    auto children = currentNode->GetChildren();
    if (children.empty()) return Move(Move::INVALID, Color::EMPTY);
    // Return first child's move (main line)
    if (auto move = extractMoveFromNode(children[0], boardSize.Columns)) {
        return *move;
    }
    return Move(Move::INVALID, Color::EMPTY);
}

std::vector<Move> GameRecord::getVariations() const {
    std::vector<Move> variations;

    if (!currentNode) {
        return variations;
    }

    // Get all children of current node
    auto children = currentNode->GetChildren();
    for (const auto& child : children) {
        if (auto move = extractMoveFromNode(child, boardSize.Columns)) {
            variations.push_back(*move);
        }
    }

    spdlog::debug("getVariations: found {} variations from current node", variations.size());
    return variations;
}

bool GameRecord::navigateToChild(const Move& targetMove) {
    if (!currentNode) {
        return false;
    }

    auto children = currentNode->GetChildren();
    for (const auto& child : children) {
        if (auto childMove = extractMoveFromNode(child, boardSize.Columns)) {
            // Check if this child matches the target move
            if (childMove->pos == targetMove.pos && childMove->col == targetMove.col) {
                currentNode = child;
                spdlog::debug("navigateToChild: moved to child node for move {}",
                    targetMove.toString());
                return true;
            }
        }
    }

    spdlog::debug("navigateToChild: no matching child found for move {}", targetMove.toString());
    return false;
}

std::string GameRecord::getComment() const {
    if (!currentNode) {
        return "";
    }

    for (const auto& property : currentNode->GetProperties()) {
        if (property->GetPropertyType() == T::C) {
            auto val = property->GetPropertyValue();
            if (val && val->ToSingleValue()) {
                return val->ToSingleValue()->GetRawValue();
            }
        }
    }
    return "";
}

std::vector<BoardMarkup> GameRecord::getMarkup() const {
    std::vector<BoardMarkup> result;
    if (!currentNode) {
        return result;
    }

    int boardSizeColumns = boardSize.Columns;

    for (const auto& property : currentNode->GetProperties()) {
        auto propType = property->GetPropertyType();
        MarkupType markupType;

        // Determine markup type from property type
        if (propType == T::LB) {
            markupType = MarkupType::LABEL;
        } else if (propType == T::TR) {
            markupType = MarkupType::TRIANGLE;
        } else if (propType == T::SQ) {
            markupType = MarkupType::SQUARE;
        } else if (propType == T::CR) {
            markupType = MarkupType::CIRCLE;
        } else if (propType == T::MA) {
            markupType = MarkupType::MARK;
        } else {
            continue;  // Not a markup property
        }

        // Process all values for this property
        for (const auto& value : property->GetPropertyValues()) {
            if (markupType == MarkupType::LABEL) {
                // LB has composed value: point:text
                if (value->IsComposedValue()) {
                    auto composed = value->ToComposedValue();
                    auto pointValue = composed->GetValue1();
                    auto textValue = composed->GetValue2();
                    if (pointValue && textValue) {
                        std::string pointStr = pointValue->GetRawValue();
                        std::string label = textValue->GetRawValue();
                        Position pos = Position::fromSgf(pointStr, boardSizeColumns);
                        if (pos) {
                            result.emplace_back(pos, markupType, label);
                        }
                    }
                }
            } else {
                // TR, SQ, CR, MA have simple point values
                if (value->ToSingleValue()) {
                    std::string pointStr = value->ToSingleValue()->GetRawValue();
                    Position pos = Position::fromSgf(pointStr, boardSizeColumns);
                    if (pos) {
                        result.emplace_back(pos, markupType);
                    }
                }
            }
        }
    }

    return result;
}