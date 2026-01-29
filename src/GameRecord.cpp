#include "GameRecord.h"
#include "Configuration.h"
#include "UserSettings.h"
#include <ISgfcColorPropertyValue.h>
#include <iomanip>
#include <filesystem>
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
    return currentNode == game->GetRootNode();
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
            // Only decrement depth for nodes that have moves (matching getTreeDepth logic)
            depth--;
        }
        // Skip empty nodes (semicolons without B/W) without decrementing depth
        node = node->GetParent();
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
        gameHasNewMoves(false),
        gameInDocument(false)
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

    auto newNode(F::CreateNode());

    if (move == Move::NORMAL || move == Move::PASS) {
        std::vector<std::shared_ptr<ISgfcProperty> > properties;
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
        unsavedChanges = true;
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

        unsavedChanges = true;
        if (!gameHasNewMoves) {
            gameHasNewMoves = true;
            appendGameToDocument();
        }
        spdlog::info("Resignation recorded: {} ({})", result, move.col == Color::BLACK ? "black resigned" : "white resigned");
    }
}

void GameRecord::branchFromFinishedGame(const Move& move) {
    std::lock_guard<std::mutex> lock(mutex);

    if (!game || !currentNode) return;

    using namespace LibSgfcPlusPlus;

    spdlog::info("branchFromFinishedGame: creating fresh game copy from finished game");

    // Collect path from root to currentNode
    std::vector<std::shared_ptr<ISgfcNode>> path;
    auto node = currentNode;
    auto rootNode = game->GetRootNode();
    while (node && node != rootNode) {
        path.push_back(node);
        node = node->GetParent();
    }
    std::reverse(path.begin(), path.end());

    // Create new game
    auto newGame = F::CreateGame();
    auto newRoot = newGame->GetRootNode();

    // Copy root properties (except RE - this is a new unfinished game)
    std::vector<std::shared_ptr<ISgfcProperty>> rootProps;
    for (const auto& prop : rootNode->GetProperties()) {
        if (prop->GetPropertyType() == T::RE) continue;
        // Update date to now
        if (prop->GetPropertyType() == T::DT) {
            std::time_t t = std::time(nullptr);
            std::tm time {};
            time = *std::localtime(&t);
            char buf[64];
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &time);
            auto vF = F::CreatePropertyValueFactory();
            auto pF = F::CreatePropertyFactory();
            rootProps.push_back(pF->CreateProperty(T::DT, vF->CreateSimpleTextPropertyValue(buf)));
            continue;
        }
        rootProps.push_back(prop);
    }
    // Update game name
    {
        auto vF = F::CreatePropertyValueFactory();
        auto pF = F::CreatePropertyFactory();
        // Remove old GN if present
        rootProps.erase(std::remove_if(rootProps.begin(), rootProps.end(),
            [](const auto& p) { return p->GetPropertyType() == SgfcPropertyType::GN; }),
            rootProps.end());
        std::string gnStr = "Game #" + std::to_string(numGames + 1);
        rootProps.push_back(pF->CreateProperty(T::GN, vF->CreateSimpleTextPropertyValue(gnStr)));
    }
    newRoot->SetProperties(rootProps);

    // Copy moves along the path
    auto vF = F::CreatePropertyValueFactory();
    auto pF = F::CreatePropertyFactory();
    auto prevNode = newRoot;
    for (const auto& pathNode : path) {
        auto newMoveNode = F::CreateNode();
        // Copy move properties (B/W) and other properties from the path node
        std::vector<std::shared_ptr<ISgfcProperty>> nodeProps;
        for (const auto& prop : pathNode->GetProperties()) {
            nodeProps.push_back(prop);
        }
        newMoveNode->SetProperties(nodeProps);
        newGame->GetTreeBuilder()->AppendChild(prevNode, newMoveNode);
        prevNode = newMoveNode;
    }

    // Add the new move
    auto newMoveNode = F::CreateNode();
    {
        auto col = move.col == Color::BLACK ? SgfcColor::Black : SgfcColor::White;
        auto type = (move.col == Color::BLACK ? T::B : T::W);
        std::shared_ptr<ISgfcPropertyValue> value{nullptr};
        if (move == Move::NORMAL) {
            auto pos = move.pos.toSgf(boardSize.Columns);
            value = vF->CreateGoMovePropertyValue(pos, boardSize, col);
        } else {
            value = vF->CreateGoMovePropertyValue(col);
        }
        std::vector<std::shared_ptr<ISgfcProperty>> moveProps;
        moveProps.push_back(pF->CreateProperty(type, value));
        newMoveNode->SetProperties(moveProps);
    }
    newGame->GetTreeBuilder()->AppendChild(prevNode, newMoveNode);

    // Switch to the new game
    game = newGame;
    currentNode = newMoveNode;
    gameHasNewMoves = true;
    unsavedChanges = true;
    gameInDocument = false;
    appendGameToDocument();

    spdlog::info("branchFromFinishedGame: created new game with {} moves + new branch",
        path.size());
}

void GameRecord::annotate(const std::string& comment) const {
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

    if (stones.empty())
        return;

    if (!game) return;
    auto root = game->GetRootNode();
    if (!root) return;

    // Check if AB property already exists on root (e.g., from loaded SGF)
    for (const auto& prop : root->GetProperties()) {
        if (prop->GetPropertyType() == T::AB) {
            spdlog::debug("setHandicapStones: AB property already exists on root, skipping");
            return;  // Don't duplicate
        }
    }

    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyValueFactory> vF(F::CreatePropertyValueFactory());
    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyFactory> pF(F::CreatePropertyFactory());

    // Add AB property to ROOT node so buildBoardFromMoves finds it
    std::shared_ptr<ISgfcProperty> abProperty(pF->CreateProperty(T::AB));
    std::vector<std::shared_ptr<ISgfcPropertyValue>> values;
    for (const auto& stone : stones) {
        auto pos = stone.toSgf(boardSize.Columns);
        values.push_back(vF->CreateStonePropertyValue(pos));
    }
    abProperty->SetPropertyValues(values);

    // Add to root node's existing properties
    auto existingProps = root->GetProperties();
    existingProps.push_back(abProperty);
    root->SetProperties(existingProps);

    spdlog::debug("setHandicapStones: added {} stones as AB property to root node", stones.size());
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
            // Day changed - save current session to OLD filename before switching
            // This ensures games from previous day stay in their session file
            if (doc != nullptr && !doc->GetGames().empty()) {
                spdlog::info("Day changed - saving previous session to: {}", defaultFileName);
                saveAsInternal(defaultFileName);
            }

            // Clear the session so new day starts fresh
            doc = nullptr;
            numGames = 0;
            // Note: gameInDocument will be set to false below when creating new game

            std::string gamesPath = "./games";
            if (config && config->data.contains("sgf_dialog")) {
                gamesPath = config->data["sgf_dialog"].value("games_path", "./games");
            }
            defaultFileName = gamesPath + "/" + today + ".sgf";

            // Clear archived session from user.json - we're on a new day
            UserSettings::instance().setLastSgfPath("");

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
    unsavedChanges = false;
    gameInDocument = false;
    
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

void GameRecord::updateKomi(float komi) {
    std::lock_guard<std::mutex> lock(mutex);

    if (!game) return;
    auto root = game->GetRootNode();
    if (!root) return;

    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyValueFactory> vF(F::CreatePropertyValueFactory());
    std::shared_ptr<LibSgfcPlusPlus::ISgfcPropertyFactory> pF(F::CreatePropertyFactory());

    // Copy existing properties, replacing KM
    std::vector<std::shared_ptr<LibSgfcPlusPlus::ISgfcProperty>> properties;
    for (const auto& prop : root->GetProperties()) {
        if (prop->GetPropertyType() != T::KM) {
            properties.push_back(prop);
        }
    }

    // Add updated komi
    properties.push_back(pF->CreateProperty(T::KM, vF->CreateRealPropertyValue(komi)));

    root->SetProperties(properties);
    spdlog::debug("Updated game record komi to {}", komi);
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
        spdlog::warn("appendGameToDocument: game is null, skipping");
        return;
    }

    if (gameInDocument) {
        spdlog::debug("appendGameToDocument: game already tracked in doc, skipping");
        return;
    }

    if (doc == nullptr) {
        // Check if daily session file already exists - load it to preserve previous games
        if (std::filesystem::exists(defaultFileName)) {
            try {
                auto reader = F::CreateDocumentReader();
                auto readResult = reader->ReadSgfFile(defaultFileName);
                if (readResult && readResult->IsSgfDataValid()) {
                    doc = readResult->GetDocument();
                    numGames = doc->GetGames().size();
                    spdlog::info("appendGameToDocument: loaded existing session file with {} games", numGames);
                } else {
                    spdlog::warn("appendGameToDocument: existing session file invalid, creating new doc");
                    doc = F::CreateDocument(game);
                    numGames = 0;
                }
            } catch (const std::exception& e) {
                spdlog::error("appendGameToDocument: failed to load existing session: {}", e.what());
                doc = F::CreateDocument(game);
                numGames = 0;
            }
        } else {
            doc = F::CreateDocument(game);
            numGames = 0;
            spdlog::info("appendGameToDocument: no existing session file, created new doc");
        }
    }

    // Safety check: verify game isn't already in the document
    auto existingGames = doc->GetGames();
    for (const auto& g : existingGames) {
        if (g == game) {
            spdlog::warn("appendGameToDocument: game already in doc, skipping");
            gameInDocument = true;
            return;
        }
    }

    doc->AppendGame(game);
    ++numGames;
    gameInDocument = true;
    spdlog::info("appendGameToDocument: appended game #{} to doc (total: {})", numGames, numGames);
}

void GameRecord::clearSession() {
    std::lock_guard<std::mutex> lock(mutex);
    doc = nullptr;
    numGames = 0;
    gameInDocument = false;  // Allow current game to be added to new session
    spdlog::info("Cleared session document - new session will start fresh");
}

void GameRecord::saveAsInternal(const std::string& fileName) {
    // Internal save - caller must hold mutex lock

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

    // Rotating backup system: keep .bak (latest) and .bak.old (previous)
    // This protects against both disk failures AND logic bugs in save code
    if (std::filesystem::exists(fn)) {
        std::string backupPath = fn + ".bak";
        std::string oldBackupPath = fn + ".bak.old";

        try {
            // Rotate: .bak → .bak.old (keep previous backup)
            if (std::filesystem::exists(backupPath)) {
                std::filesystem::copy_file(backupPath, oldBackupPath,
                    std::filesystem::copy_options::overwrite_existing);
                spdlog::debug("Rotated backup: {} → {}", backupPath, oldBackupPath);
            }

            // Create new backup: file → .bak
            std::filesystem::copy_file(fn, backupPath,
                std::filesystem::copy_options::overwrite_existing);
            spdlog::debug("Created backup: {}", backupPath);
        } catch (const std::filesystem::filesystem_error& e) {
            spdlog::warn("Could not create backup {}: {}", backupPath, e.what());
            // Continue with save anyway - backup failure shouldn't block save
        }
    }

    const std::shared_ptr writer(F::CreateDocumentWriter());
    try {
        (void) writer->WriteSgfFile(doc, fn);
        unsavedChanges = false;
        spdlog::info("Writing sgf file [{}] success!", fn);
    } catch (std::exception& ex) {
        spdlog::error("Writing sgf file [{}] failed: {}", fn, ex.what());
    }
}

void GameRecord::saveAs(const std::string& fileName) {
    std::lock_guard<std::mutex> lock(mutex);
    saveAsInternal(fileName);
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
        
        // Handle special index -1 as "last game"
        if (gameIndex < 0) {
            gameIndex = static_cast<int>(games.size()) - 1;
        }
        if (gameIndex >= static_cast<int>(games.size())) {
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
        gameInfo.setupBlackStones.clear();
        gameInfo.gameResult = LibSgfcPlusPlus::SgfcGameResult();

        for (auto property : rootNode->GetProperties()) {
            switch (property->GetPropertyType()) {
                case T::SZ: {
                    if (auto numberValue = std::dynamic_pointer_cast<ISgfcNumberPropertyValue>(property->GetPropertyValue())) {
                        gameInfo.boardSize = numberValue->GetNumberValue();
                    }
                    break;
                }
                case T::KM: {
                    if (auto realValue = std::dynamic_pointer_cast<ISgfcRealPropertyValue>(property->GetPropertyValue())) {
                        gameInfo.komi = realValue->GetRealValue();
                    }
                    break;
                }
                case T::HA: {
                    if (auto numberValue = std::dynamic_pointer_cast<ISgfcNumberPropertyValue>(property->GetPropertyValue())) {
                        gameInfo.handicap = numberValue->GetNumberValue();
                    }
                    break;
                }
                case T::PB: {
                    if (auto textValue = std::dynamic_pointer_cast<ISgfcSimpleTextPropertyValue>(property->GetPropertyValue())) {
                        gameInfo.blackPlayer = textValue->GetSimpleTextValue();
                    }
                    break;
                }
                case T::PW: {
                    if (auto textValue = std::dynamic_pointer_cast<ISgfcSimpleTextPropertyValue>(property->GetPropertyValue())) {
                        gameInfo.whitePlayer = textValue->GetSimpleTextValue();
                    }
                    break;
                }
                case T::AB: {
                    for (auto value : property->GetPropertyValues()) {
                        if (auto stoneValue = std::dynamic_pointer_cast<ISgfcStonePropertyValue>(value)) {
                            auto sgfPoint = stoneValue->GetStoneValue();
                            Position pos = Position::fromSgf(sgfPoint, gameInfo.boardSize);
                            gameInfo.setupBlackStones.push_back(pos);
                        }
                    }
                    break;
                }
                case T::AW: {
                    for (auto value : property->GetPropertyValues()) {
                        if (auto stoneValue = std::dynamic_pointer_cast<ISgfcStonePropertyValue>(value)) {
                            auto sgfPoint = stoneValue->GetStoneValue();
                            Position pos = Position::fromSgf(sgfPoint, gameInfo.boardSize);
                            gameInfo.setupWhiteStones.push_back(pos);
                        }
                    }
                    break;
                }
                case T::RE: {
                    if (auto textValue = std::dynamic_pointer_cast<ISgfcSimpleTextPropertyValue>(property->GetPropertyValue())) {
                        gameInfo.gameResult = SgfcGameResult::FromPropertyValue(textValue->GetSimpleTextValue());
                    }
                    break;
                }
                default:
                    ;
            }
        }

        // Keep the game tree (SGF is single source of truth)
        game = loadedGame;
        gameHasNewMoves = false;
        unsavedChanges = false;
        gameInDocument = false;

        // Only preserve doc when loading daily session file (for appending)
        // External SGFs are ephemeral - if modified, they become part of daily session
        spdlog::debug("loadFromSGF: comparing fileName='{}' with defaultFileName='{}'",
            fileName, defaultFileName);
        if (fileName == defaultFileName && doc == nullptr) {
            doc = loadedDoc->GetDocument();
            numGames = games.size();
            gameInDocument = true;  // Game is already in doc, prevent re-append
            spdlog::debug("loadFromSGF: paths match - preserving doc with {} games", numGames);
        } else {
            // keep existing doc (daily session), game is just for viewing/navigation
            spdlog::debug("loadFromSGF: paths differ - external SGF viewing (doc unchanged)");
        }

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

std::pair<std::string, std::string> GameRecord::getPlayerNames() const {
    std::string black = "Black";
    std::string white = "White";

    if (!game) return {black, white};
    auto root = game->GetRootNode();
    if (!root) return {black, white};

    for (const auto& prop : root->GetProperties()) {
        if (prop->GetPropertyType() == T::PB) {
            if (auto textValue = std::dynamic_pointer_cast<ISgfcSimpleTextPropertyValue>(prop->GetPropertyValue())) {
                black = textValue->GetSimpleTextValue();
            }
        } else if (prop->GetPropertyType() == T::PW) {
            if (auto textValue = std::dynamic_pointer_cast<ISgfcSimpleTextPropertyValue>(prop->GetPropertyValue())) {
                white = textValue->GetSimpleTextValue();
            }
        }
    }
    return {black, white};
}

int GameRecord::countStoneMoves(const Color& color) const {
    auto path = getPathFromRoot();
    int count = 0;
    for (const auto& move : path) {
        // Count only stone placements (NORMAL moves), not passes
        if (move == Move::NORMAL && move.col == color) {
            count++;
        }
    }
    return count;
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
    bool atEnd = isAtEndOfNavigation();
    bool finished = isGameFinished();
    Move last = lastMove();
    bool hasResult = hasGameResult();
    bool isResign = isResignationResult();

    spdlog::debug("shouldShowTerritory: atEnd={}, finished={}, lastMove={}, hasResult={}, isResign={}",
        atEnd, finished, last == Move::PASS ? "PASS" : (last == Move::RESIGN ? "RESIGN" : "other"),
        hasResult, isResign);

    if (!atEnd) return false;

    // Double-pass at current position (not resign)
    if (finished && last == Move::PASS) return true;

    // Has point result (not resignation)
    // RE property is valid - it's cleared when creating new variations
    if (hasResult && !isResign) return true;

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

void GameRecord::removeGameResult() const {
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
        // No moves yet - determine who plays first from SGF root properties.
        // Priority: PL (explicit) > HA without AW (handicap convention) > default (black)
        bool hasHandicap = false;
        bool hasWhiteSetup = false;
        if (game && game->GetRootNode()) {
            for (const auto& prop : game->GetRootNode()->GetProperties()) {
                if (prop->GetPropertyType() == T::PL) {
                    // PL property explicitly sets player to move - always wins
                    if (auto colorVal = std::dynamic_pointer_cast<ISgfcColorPropertyValue>(prop->GetPropertyValue())) {
                        return colorVal->GetColorValue() == SgfcColor::Black
                            ? Color::BLACK : Color::WHITE;
                    }
                }
                if (prop->GetPropertyType() == T::HA) {
                    if (auto numVal = std::dynamic_pointer_cast<ISgfcNumberPropertyValue>(prop->GetPropertyValue())) {
                        hasHandicap = numVal->GetNumberValue() > 0;
                    }
                }
                if (prop->GetPropertyType() == T::AW) {
                    hasWhiteSetup = true;
                }
            }
        }
        // HA without AW: standard handicap, white moves first.
        // HA with AW or no HA: ambiguous/setup position, default to black.
        return (hasHandicap && !hasWhiteSetup) ? Color::WHITE : Color::BLACK;
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

bool GameRecord::navigateToChild(const Move& targetMove, bool promoteToMainLine) {
    if (!currentNode) {
        return false;
    }

    auto children = currentNode->GetChildren();
    for (size_t idx = 0; idx < children.size(); ++idx) {
        const auto& child = children[idx];
        if (auto childMove = extractMoveFromNode(child, boardSize.Columns)) {
            // Check if this child matches the target move
            if (childMove->pos == targetMove.pos && childMove->col == targetMove.col) {
                // Promote to first child if requested, user has new moves, and not already first
                if (promoteToMainLine && gameHasNewMoves && idx > 0 && game) {
                    game->GetTreeBuilder()->InsertChild(currentNode, child, children[0]);
                    spdlog::debug("navigateToChild: promoted variation to main line");
                }
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

void GameRecord::promoteCurrentPathToMainLine() const {
    if (!game || !currentNode) return;

    auto treeBuilder = game->GetTreeBuilder();
    auto rootNode = game->GetRootNode();

    // Collect path from root to currentNode
    std::vector<std::shared_ptr<LibSgfcPlusPlus::ISgfcNode>> path;
    auto node = currentNode;
    while (node && node != rootNode) {
        path.push_back(node);
        node = node->GetParent();
    }

    // Promote each node to be the first child of its parent
    for (auto it = path.rbegin(); it != path.rend(); ++it) {
        auto child = *it;
        auto parent = child->GetParent();
        if (!parent) continue;
        auto siblings = parent->GetChildren();
        if (siblings.empty() || siblings[0] == child) continue;  // Already first
        treeBuilder->InsertChild(parent, child, siblings[0]);
        spdlog::debug("promoteCurrentPathToMainLine: promoted node to first child");
    }
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
                        if (Position pos = Position::fromSgf(pointStr, boardSizeColumns)) {
                            result.emplace_back(pos, markupType, label);
                        }
                    }
                }
            } else {
                // TR, SQ, CR, MA have simple point values
                if (value->ToSingleValue()) {
                    std::string pointStr = value->ToSingleValue()->GetRawValue();
                    if (Position pos = Position::fromSgf(pointStr, boardSizeColumns)) {
                        result.emplace_back(pos, markupType);
                    }
                }
            }
        }
    }

    return result;
}

int GameRecord::peekBoardSize(const std::string& fileName) {
    using namespace LibSgfcPlusPlus;

    if (!std::filesystem::exists(fileName)) {
        return -1;
    }

    try {
        auto reader = SgfcPlusPlusFactory::CreateDocumentReader();
        auto loadedDoc = reader->ReadSgfFile(fileName);

        if (!loadedDoc || !loadedDoc->IsSgfDataValid()) {
            return -1;
        }

        auto games = loadedDoc->GetDocument()->GetGames();
        if (games.empty()) {
            return -1;
        }

        // Check last game (consistent with loadFromSGF with gameIndex=-1)
        auto lastGame = games.back();
        auto rootNode = lastGame->GetRootNode();

        for (auto property : rootNode->GetProperties()) {
            if (property->GetPropertyType() == SgfcPropertyType::SZ) {
                if (auto numberValue = std::dynamic_pointer_cast<ISgfcNumberPropertyValue>(property->GetPropertyValue())) {
                    return numberValue->GetNumberValue();
                }
            }
        }

        return 19;  // Default board size if SZ not specified
    } catch (const std::exception& e) {
        spdlog::warn("peekBoardSize failed for {}: {}", fileName, e.what());
        return -1;
    }
}

void GameRecord::buildBoardFromMoves(Board& outBoard, Position& koPosition) const {
    outBoard.clear(boardSize.Columns);
    koPosition = Position(-1, -1);

    // First, place any handicap stones from AB property in root node
    if (game && game->GetRootNode()) {
        for (const auto& prop : game->GetRootNode()->GetProperties()) {
            if (prop->GetPropertyType() == T::AB) {
                for (const auto& value : prop->GetPropertyValues()) {
                    if (auto stoneValue = std::dynamic_pointer_cast<LibSgfcPlusPlus::ISgfcStonePropertyValue>(value)) {
                        auto sgfPoint = stoneValue->GetStoneValue();
                        Position pos = Position::fromSgf(sgfPoint, boardSize.Columns);
                        if (pos.col() >= 0 && pos.row() >= 0) {
                            outBoard[pos].stone = Color::BLACK;
                        }
                    }
                }
            }
            // Also handle AW (added white) if present
            if (prop->GetPropertyType() == T::AW) {
                for (const auto& value : prop->GetPropertyValues()) {
                    if (auto stoneValue = std::dynamic_pointer_cast<LibSgfcPlusPlus::ISgfcStonePropertyValue>(value)) {
                        auto sgfPoint = stoneValue->GetStoneValue();
                        Position pos = Position::fromSgf(sgfPoint, boardSize.Columns);
                        if (pos.col() >= 0 && pos.row() >= 0) {
                            outBoard[pos].stone = Color::WHITE;
                        }
                    }
                }
            }
        }
    }

    // Replay moves from root to current position
    auto path = getPathFromRoot();
    outBoard.replayMoves(path);
    koPosition = outBoard.getKoPosition();

    spdlog::debug("buildBoardFromMoves: replayed {} moves, {} black stones, {} white stones on board",
        path.size(), outBoard.stonesOnBoard(Color::BLACK), outBoard.stonesOnBoard(Color::WHITE));
}