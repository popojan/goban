#include "FileChooserDataSource.h"
#include "GameRecord.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <fstream>
#include "SGF.h"

FileChooserDataSource::FileChooserDataSource(const std::string& gamesPath)
    : currentPath(gamesPath)
    , selectedFileIndex(-1)
    , selectedGameIndex(-1)
    , filesCurrentPage(1)
    , gamesCurrentPage(1) {
    
    RefreshFiles();
}

FileChooserDataSource::~FileChooserDataSource() {
    spdlog::debug("FileChooserDataSource destructor called");
    // Clear containers to free memory
    files.clear();
    games.clear();
}

void FileChooserDataSource::GetRow(std::vector<std::string>& row, const std::string& table, int row_index, const std::vector<std::string>& columns) const {
    if (table == "files") {
        // Check if this is the first row and we can navigate up
        if (row_index == 0 && currentPath.has_parent_path()) {
            // Show ".." row for navigating up
            for (size_t i = 0; i < columns.size(); ++i) {
                if (columns[i] == "name") {
                    row.push_back(std::string(".."));
                } else if (columns[i] == "type") {
                    row.push_back(std::string(strUp.c_str()));
                } else if (columns[i] == "path") {
                    row.push_back(std::string(currentPath.parent_path().u8string().c_str()));
                }
            }
            return;
        }
        
        // Adjust index for regular files (skip the ".." row if present)
        int fileIndex = row_index;
        if (currentPath.has_parent_path()) {
            fileIndex = row_index - 1; // Account for the ".." row
        }
        
        // Map file index to actual file index based on current page
        int actualIndex;
        if (currentPath.has_parent_path()) {
            // Every page shows ".." row, so every page has (FILES_PAGE_SIZE - 1) actual files
            actualIndex = (filesCurrentPage - 1) * (FILES_PAGE_SIZE - 1) + fileIndex;
        } else {
            // Normal pagination when no ".." row
            actualIndex = (filesCurrentPage - 1) * FILES_PAGE_SIZE + fileIndex;
        }
        if (actualIndex < 0 || actualIndex >= static_cast<int>(files.size())) {
            return;
        }
        
        const FileEntry& file = files[actualIndex];
        
        for (size_t i = 0; i < columns.size(); ++i) {
            if (columns[i] == "name") {
                row.push_back(std::string(file.name.c_str()));
            } else if (columns[i] == "type") {
                row.push_back(std::string(file.type.c_str()));
            } else if (columns[i] == "path") {
                row.push_back(std::string(file.fullPath.u8string().c_str()));
            }
        }
    }
    else if (table == "games") {
        // Map row_index to actual game index based on current page
        int actualIndex = (gamesCurrentPage - 1) * GAMES_PAGE_SIZE + row_index;
        if (actualIndex < 0 || actualIndex >= static_cast<int>(games.size())) {
            return;
        }
        
        const SGFGameInfo& game = games[actualIndex];
        
        for (size_t i = 0; i < columns.size(); ++i) {
            if (columns[i] == "title") {
                row.push_back(std::string(game.title.c_str()));
            } else if (columns[i] == "players") {
                row.push_back(std::string(game.players.c_str()));
            } else if (columns[i] == "black") {
                row.push_back(std::string(game.blackPlayer.c_str()));
            } else if (columns[i] == "white") {
                row.push_back(std::string(game.whitePlayer.c_str()));
            } else if (columns[i] == "result") {
                row.push_back(std::string(game.gameResult.c_str()));
            } else if (columns[i] == "moves") {
                // Calculate move count on demand if not already calculated
                if (game.moveCount == -1) {
                    row.push_back(std::string("-"));
                } else {
                    row.push_back(std::to_string(game.moveCount));
                }
            } else if (columns[i] == "board_size") {
                // Format as "N×N" (using Unicode multiplication sign)
                std::string sizeStr = std::to_string(game.boardSize) + "×" + std::to_string(game.boardSize);
                row.push_back(sizeStr);
            } else if (columns[i] == "komi") {
                char buf[16];
                snprintf(buf, sizeof(buf), "%.1f", game.komi);
                row.push_back(std::string(buf));
            }
        }
    }
}

int FileChooserDataSource::GetNumRows(const std::string& table) const {
    if (table == "files") {
        // Calculate base number of files on current page
        int totalFiles = static_cast<int>(files.size());
        
        if (currentPath.has_parent_path()) {
            // Every page shows ".." row plus (FILES_PAGE_SIZE - 1) actual files
            int startIndex = (filesCurrentPage - 1) * (FILES_PAGE_SIZE - 1);
            int endIndex = std::min(startIndex + (FILES_PAGE_SIZE - 1), totalFiles);
            int fileRows = std::max(0, endIndex - startIndex);
            return fileRows + 1; // +1 for the ".." row
        } else {
            // No ".." row, normal pagination
            int startIndex = (filesCurrentPage - 1) * FILES_PAGE_SIZE;
            int endIndex = std::min(startIndex + FILES_PAGE_SIZE, totalFiles);
            return std::max(0, endIndex - startIndex);
        }
    } else if (table == "games") {
        // Return the number of items on the current page
        int totalGames = static_cast<int>(games.size());
        int startIndex = (gamesCurrentPage - 1) * GAMES_PAGE_SIZE;
        int endIndex = std::min(startIndex + GAMES_PAGE_SIZE, totalGames);
        return std::max(0, endIndex - startIndex);
    }
    return 0;
}

void FileChooserDataSource::SetCurrentPath(const std::filesystem::path& path) {
    if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
        currentPath = path;
        RefreshFiles();
    }
}

void FileChooserDataSource::NavigateUp() {
    if (currentPath.has_parent_path()) {
        currentPath = currentPath.parent_path();
        RefreshFiles();
    }
}

void FileChooserDataSource::RefreshFiles() {
    spdlog::debug("RefreshFiles() called");
    
    refreshFileList();
    
    // Clear games when changing directories
    games.clear();
    selectedFileIndex = -1;
    selectedGameIndex = -1;
    
    // Reset pagination to first page
    filesCurrentPage = 1;
    gamesCurrentPage = 1;
    
    // File chooser is disabled - DataSource functionality not available
    spdlog::debug("Files list updated (data source disabled)");
}

void FileChooserDataSource::refreshFileList() {
    files.clear();

    try {
        if (std::filesystem::exists(currentPath) && std::filesystem::is_directory(currentPath)) {
            for (const auto& entry : std::filesystem::directory_iterator(currentPath)) {
                FileEntry fileEntry;
                fileEntry.name = entry.path().filename().u8string();
                fileEntry.fullPath = entry.path();
                
                // Use status() once instead of multiple calls
                std::error_code ec;
                auto status = entry.status(ec);
                if (ec) {
                    spdlog::debug("Error getting file status for {}: {}", fileEntry.name, ec.message());
                    continue;
                }
                
                fileEntry.isDirectory = std::filesystem::is_directory(status);
                
                if (fileEntry.isDirectory) {
                    fileEntry.type = strDirectory;
                } else if (fileEntry.fullPath.extension() == ".sgf") {
                    fileEntry.type = strSgfFile;
                } else {
                    // Skip non-SGF files
                    continue;
                }
                
                files.push_back(fileEntry);
            }
            
            // Sort: directories first, then files
            std::sort(files.begin(), files.end(), [](const FileEntry& a, const FileEntry& b) {
                if (a.isDirectory && !b.isDirectory) return true;
                if (!a.isDirectory && b.isDirectory) return false;
                return a.name < b.name;
            });
        }
    } catch (const std::exception& e) {
        spdlog::error("Error reading directory: {}", e.what());
    }
}

void FileChooserDataSource::SelectFile(int fileIndex) {
    if (fileIndex < 0 || fileIndex >= static_cast<int>(files.size())) {
        return;
    }
    
    selectedFileIndex = fileIndex;
    const FileEntry& file = files[fileIndex];
    
    if (file.isDirectory) {
        // Navigate to directory
        SetCurrentPath(file.fullPath);
    } else if (file.fullPath.extension() == ".sgf") {
        // Load games for the selected SGF file
        LoadSelectedFileGames();
    }
}

const FileEntry* FileChooserDataSource::GetSelectedFile() const {
    if (selectedFileIndex >= 0 && selectedFileIndex < static_cast<int>(files.size())) {
        return &files[selectedFileIndex];
    }
    return nullptr;
}

void FileChooserDataSource::SelectGame(int gameIndex) {
    if (gameIndex >= 0 && gameIndex < static_cast<int>(games.size())) {
        selectedGameIndex = gameIndex;
    }
}

const SGFGameInfo* FileChooserDataSource::GetSelectedGame() const {
    if (selectedGameIndex >= 0 && selectedGameIndex < static_cast<int>(games.size())) {
        return &games[selectedGameIndex];
    }
    return nullptr;
}

int FileChooserDataSource::GetSelectedGameIndex() const {
    return selectedGameIndex;
}

std::string FileChooserDataSource::GetSelectedFilePath() const {
    const FileEntry* file = GetSelectedFile();
    if (file && !file->isDirectory) {
        return file->fullPath.u8string();
    }
    return "";
}

int FileChooserDataSource::FindFileByPath(const std::string& path) const {
    for (size_t i = 0; i < files.size(); ++i) {
        if (files[i].fullPath.u8string() == path) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void FileChooserDataSource::LoadSelectedFileGames() {
    const FileEntry* file = GetSelectedFile();
    if (file && !file->isDirectory && file->fullPath.extension() == ".sgf") {
        previewSGF(file->fullPath.u8string());
    }
}

void FileChooserDataSource::previewSGF(const std::string& filePath) {
    spdlog::debug("previewSGF() called for: {}", filePath);
    
    games = parseSGFGames(filePath);
    selectedGameIndex = games.empty() ? -1 : 0;
    
    // Reset games pagination to first page
    gamesCurrentPage = 1;
    
    spdlog::debug("Parsed {} games from SGF file", games.size());
    
    // File chooser is disabled - DataSource functionality not available
    spdlog::debug("Games list updated (data source disabled)");
}

std::vector<SGFGameInfo> FileChooserDataSource::parseSGFGames(const std::string& filePath) const {
    using namespace LibSgfcPlusPlus;
    
    std::vector<SGFGameInfo> gameList;
    
    try {
        auto sgfContent = GameRecord::readFileContent(filePath);
        if (!sgfContent) {
            spdlog::error("Failed to read SGF file: {}", filePath);
            return gameList;
        }
        auto reader = SgfcPlusPlusFactory::CreateDocumentReader();
        auto result = reader->ReadSgfContent(*sgfContent);

        if (!result || !result->IsSgfDataValid()) {
            spdlog::error("Failed to parse SGF file: {}", filePath);
            return gameList;
        }
        
        auto document = result->GetDocument();
        auto games = document->GetGames();
        
        spdlog::debug("Found {} games in SGF file", games.size());
        
        for (size_t gameIndex = 0; gameIndex < games.size(); ++gameIndex) {
            auto game = games[gameIndex];
            auto rootNode = game->GetRootNode();
            
            SGFGameInfo gameInfo;
            gameInfo.blackPlayer = "Unknown";
            gameInfo.whitePlayer = "Unknown";
            gameInfo.boardSize = 19;
            gameInfo.komi = 0.0f;
            gameInfo.moveCount = 0;
            gameInfo.gameIndex = static_cast<int>(gameIndex);
            
            // Extract game properties from root node
            auto properties = rootNode->GetProperties();
            for (const auto& property : properties) {
                auto propertyType = property->GetPropertyType();
                auto propertyValues = property->GetPropertyValues();
                
                if (propertyValues.empty()) continue;
                
                switch (propertyType) {
                    case SgfcPropertyType::PB: // Black player
                        if (auto textValue = std::dynamic_pointer_cast<ISgfcSimpleTextPropertyValue>(propertyValues[0])) {
                            gameInfo.blackPlayer = textValue->GetSimpleTextValue();
                        }
                        break;
                    case SgfcPropertyType::PW: // White player
                        if (auto textValue = std::dynamic_pointer_cast<ISgfcSimpleTextPropertyValue>(propertyValues[0])) {
                            gameInfo.whitePlayer = textValue->GetSimpleTextValue();
                        }
                        break;
                    case SgfcPropertyType::SZ: // Board size
                        if (auto numberValue = std::dynamic_pointer_cast<ISgfcNumberPropertyValue>(propertyValues[0])) {
                            gameInfo.boardSize = static_cast<int>(numberValue->GetNumberValue());
                        }
                        break;
                    case SgfcPropertyType::KM: // Komi
                        if (auto realValue = std::dynamic_pointer_cast<ISgfcRealPropertyValue>(propertyValues[0])) {
                            gameInfo.komi = static_cast<float>(realValue->GetRealValue());
                        }
                        break;
                    case SgfcPropertyType::RE: // Result
                        if (auto textValue = std::dynamic_pointer_cast<ISgfcSimpleTextPropertyValue>(propertyValues[0])) {
                            gameInfo.gameResult = textValue->GetSimpleTextValue();
                        }
                        break;
                    case SgfcPropertyType::DT: // Date
                        if (auto textValue = std::dynamic_pointer_cast<ISgfcSimpleTextPropertyValue>(propertyValues[0])) {
                            gameInfo.date = textValue->GetSimpleTextValue();
                        }
                        break;
                    case SgfcPropertyType::AB: // Setup black stones
                        gameInfo.hasSetupStones = true;
                        break;
                    case SgfcPropertyType::AW: // Setup white stones
                        gameInfo.hasSetupStones = true;
                        gameInfo.hasSetupWhiteStones = true;
                        break;
                    default:
                        break;
                }
            }
            
            // FF[3] compat: setup stones may be on first child instead of root
            if (!gameInfo.hasSetupStones && rootNode->HasChildren()) {
                auto firstChild = rootNode->GetFirstChild();
                for (const auto& property : firstChild->GetProperties()) {
                    auto pt = property->GetPropertyType();
                    if (pt == SgfcPropertyType::AB || pt == SgfcPropertyType::AW) {
                        gameInfo.hasSetupStones = true;
                        if (pt == SgfcPropertyType::AW)
                            gameInfo.hasSetupWhiteStones = true;
                    }
                }
            }

            // Count moves by traversing the game tree
            gameInfo.moveCount = countMovesInGame(game);
            
            // Create display strings with move count
            char titleBuffer[128];
            snprintf(titleBuffer, sizeof(titleBuffer), strGameInfoFmt.c_str(),
                    static_cast<int>(gameIndex + 1), gameInfo.boardSize, gameInfo.boardSize, gameInfo.moveCount);
            gameInfo.title = std::string(titleBuffer);
            gameInfo.players = gameInfo.blackPlayer + " vs " + gameInfo.whitePlayer;
            
            gameList.push_back(gameInfo);
        }
        
    } catch (const std::exception& e) {
        spdlog::error("Error parsing SGF file: {}", e.what());
    }
    
    return gameList;
}

int FileChooserDataSource::countMovesInGame(const std::shared_ptr<LibSgfcPlusPlus::ISgfcGame> game) {
    using namespace LibSgfcPlusPlus;

    int moveCount = 0;
    int nodeCount = 0;
    auto rootNode = game->GetRootNode();

    // Traverse the game tree to count move nodes (main line only)
    std::function<void(std::shared_ptr<ISgfcNode>)> traverseNode = [&](std::shared_ptr<ISgfcNode> node) {
        if (!node) return;
        nodeCount++;

        // Check if this node contains a move property (B or W, not AB/AW)
        auto properties = node->GetProperties();
        for (const auto& property : properties) {
            auto propertyType = property->GetPropertyType();
            if (propertyType == SgfcPropertyType::B || propertyType == SgfcPropertyType::W) {
                moveCount++;
                spdlog::trace("countMovesInGame: node {} has {} move, total={}", nodeCount,
                    propertyType == SgfcPropertyType::B ? "B" : "W", moveCount);
                break; // Only count once per node
            }
        }

        // Traverse children (follow main line only)
        auto children = node->GetChildren();
        if (!children.empty()) {
            traverseNode(children[0]);
        }
    };

    traverseNode(rootNode);
    spdlog::debug("countMovesInGame: {} nodes traversed, {} moves counted", nodeCount, moveCount);
    return moveCount;
}

bool FileChooserDataSource::isTsumegoDetected() const {
    if (games.empty()) return false;
    // Require white setup stones (AW) in most games (>= 80%)
    // This distinguishes tsumego (AB+AW) from handicap games (AB only)
    int setupCount = 0;
    int resultCount = 0;
    int totalMoves = 0;
    for (const auto& g : games) {
        if (g.hasSetupWhiteStones) setupCount++;
        if (!g.gameResult.empty()) resultCount++;
        totalMoves += g.moveCount;
    }
    // Games with results are completed games, not tsumego collections
    if (resultCount > 0) return false;
    float setupRatio = static_cast<float>(setupCount) / static_cast<float>(games.size());
    if (setupRatio < 0.8f) return false;
    // Average moves per game: tsumego typically < 50, full games typically > 150
    float avgMoves = static_cast<float>(totalMoves) / static_cast<float>(games.size());
    return avgMoves <= 50;
}

void FileChooserDataSource::SetFilesPage(int page) {
    int totalPages = GetFilesTotalPages();
    if (page >= 1 && page <= totalPages) {
        filesCurrentPage = page;
        // File chooser disabled - DataSource not available
    }
}

void FileChooserDataSource::SetGamesPage(int page) {
    int totalPages = GetGamesTotalPages();
    if (page >= 1 && page <= totalPages) {
        gamesCurrentPage = page;
        // File chooser disabled - DataSource not available
    }
}

int FileChooserDataSource::GetFilesTotalPages() const {
    int totalFiles = static_cast<int>(files.size());
    return (totalFiles + FILES_PAGE_SIZE - 1) / (FILES_PAGE_SIZE - 1); // Ceiling division
}

int FileChooserDataSource::GetGamesTotalPages() const {
    int totalGames = static_cast<int>(games.size());
    return (totalGames + GAMES_PAGE_SIZE) / GAMES_PAGE_SIZE; // Ceiling division
}

void FileChooserDataSource::SetLocalizedStrings(const std::string& directory, const std::string& sgfFile,
                                                const std::string& up, const std::string& gameInfoFmt) {
    strDirectory = directory;
    strSgfFile = sgfFile;
    strUp = up;
    strGameInfoFmt = gameInfoFmt;
}