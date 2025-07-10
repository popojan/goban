#include "FileChooserDataSource.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include "SGF.h"

FileChooserDataSource::FileChooserDataSource() 
    : Rocket::Controls::DataSource("file_chooser")
    , currentPath("data/sgf")
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

void FileChooserDataSource::GetRow(Rocket::Core::StringList& row, const Rocket::Core::String& table, int row_index, const Rocket::Core::StringList& columns) {
    if (table == "files") {
        // Check if this is the first row and we can navigate up
        if (row_index == 0 && currentPath.has_parent_path()) {
            // Show ".." row for navigating up
            for (size_t i = 0; i < columns.size(); ++i) {
                if (columns[i] == "name") {
                    row.push_back(Rocket::Core::String(".."));
                } else if (columns[i] == "type") {
                    row.push_back(Rocket::Core::String("Up"));
                } else if (columns[i] == "path") {
                    row.push_back(Rocket::Core::String(currentPath.parent_path().string().c_str()));
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
                row.push_back(Rocket::Core::String(file.name.c_str()));
            } else if (columns[i] == "type") {
                row.push_back(Rocket::Core::String(file.type.c_str()));
            } else if (columns[i] == "path") {
                row.push_back(Rocket::Core::String(file.fullPath.string().c_str()));
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
                row.push_back(Rocket::Core::String(game.title.c_str()));
            } else if (columns[i] == "players") {
                row.push_back(Rocket::Core::String(game.players.c_str()));
            } else if (columns[i] == "black") {
                row.push_back(Rocket::Core::String(game.blackPlayer.c_str()));
            } else if (columns[i] == "white") {
                row.push_back(Rocket::Core::String(game.whitePlayer.c_str()));
            } else if (columns[i] == "result") {
                row.push_back(Rocket::Core::String(game.gameResult.c_str()));
            } else if (columns[i] == "moves") {
                // Calculate move count on demand if not already calculated
                if (game.moveCount == -1) {
                    // This is a bit of a hack - we need to modify the game object
                    // In a real implementation, we might want to cache this differently
                    row.push_back(Rocket::Core::String("-"));
                } else {
                    row.push_back(Rocket::Core::String(8, "%d", game.moveCount));
                }
            } else if (columns[i] == "board_size") {
                row.push_back(Rocket::Core::String(8, "%d", game.boardSize));
            } else if (columns[i] == "komi") {
                row.push_back(Rocket::Core::String(8, "%.1f", game.komi));
            }
        }
    }
}

int FileChooserDataSource::GetNumRows(const Rocket::Core::String& table) {
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
    
    // Use NotifyRowChange for complete table refresh (preferred pattern)
    spdlog::debug("Notifying complete files table refresh");
    NotifyRowChange("files");

    spdlog::debug("Notifying complete games table refresh");
    NotifyRowChange("games");
}

void FileChooserDataSource::refreshFileList() {
    files.clear();

    try {
        if (std::filesystem::exists(currentPath) && std::filesystem::is_directory(currentPath)) {
            for (const auto& entry : std::filesystem::directory_iterator(currentPath)) {
                FileEntry fileEntry;
                fileEntry.name = entry.path().filename().string();
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
                    fileEntry.type = "Directory";
                } else if (fileEntry.name.find(".sgf") != std::string::npos) {
                    fileEntry.type = "SGF File";
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
    } else if (file.name.find(".sgf") != std::string::npos) {
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
        return file->fullPath.string();
    }
    return "";
}

void FileChooserDataSource::LoadSelectedFileGames() {
    const FileEntry* file = GetSelectedFile();
    if (file && !file->isDirectory && file->name.find(".sgf") != std::string::npos) {
        previewSGF(file->fullPath.string());
    }
}

void FileChooserDataSource::previewSGF(const std::string& filePath) {
    spdlog::debug("previewSGF() called for: {}", filePath);
    
    games = parseSGFGames(filePath);
    selectedGameIndex = games.empty() ? -1 : 0;
    
    // Reset games pagination to first page
    gamesCurrentPage = 1;
    
    spdlog::debug("Parsed {} games from SGF file", games.size());
    
    // Use NotifyRowChange for complete table refresh
    spdlog::debug("Notifying complete games table refresh");
    NotifyRowChange("games");
}

std::vector<SGFGameInfo> FileChooserDataSource::parseSGFGames(const std::string& filePath) {
    using namespace LibSgfcPlusPlus;
    
    std::vector<SGFGameInfo> gameList;
    
    try {
        auto reader = SgfcPlusPlusFactory::CreateDocumentReader();
        auto result = reader->ReadSgfFile(filePath);
        
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
                    default:
                        break;
                }
            }
            
            // Count moves by traversing the game tree
            gameInfo.moveCount = countMovesInGame(game);
            
            // Create display strings with move count
            char titleBuffer[64];
            snprintf(titleBuffer, sizeof(titleBuffer), "Game %d (%dx%d, %d moves)", 
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

int FileChooserDataSource::countMovesInGame(std::shared_ptr<LibSgfcPlusPlus::ISgfcGame> game) {
    using namespace LibSgfcPlusPlus;
    
    int moveCount = 0;
    auto currentNode = game->GetRootNode();
    
    // Traverse the game tree to count move nodes
    std::function<void(std::shared_ptr<ISgfcNode>)> traverseNode = [&](std::shared_ptr<ISgfcNode> node) {
        if (!node) return;
        
        // Check if this node contains a move property
        auto properties = node->GetProperties();
        for (const auto& property : properties) {
            auto propertyType = property->GetPropertyType();
            if (propertyType == SgfcPropertyType::B || propertyType == SgfcPropertyType::W) {
                moveCount++;
                break; // Only count once per node
            }
        }
        
        // Traverse children (we'll just follow the main line for move counting)
        auto children = node->GetChildren();
        if (!children.empty()) {
            traverseNode(children[0]); // Follow main line
        }
    };
    
    traverseNode(currentNode);
    return moveCount;
}

void FileChooserDataSource::SetFilesPage(int page) {
    int totalPages = GetFilesTotalPages();
    if (page >= 1 && page <= totalPages) {
        filesCurrentPage = page;
        // Refresh the files table
        NotifyRowChange("files");
    }
}

void FileChooserDataSource::SetGamesPage(int page) {
    int totalPages = GetGamesTotalPages();
    if (page >= 1 && page <= totalPages) {
        gamesCurrentPage = page;
        // Refresh the games table
        NotifyRowChange("games");
    }
}

int FileChooserDataSource::GetFilesTotalPages() const {
    int totalFiles = static_cast<int>(files.size());
    
    // If we can navigate up, we need to account for the ".." row
    if (currentPath.has_parent_path()) {
        totalFiles += 1; // Add 1 for the ".." row
    }
    
    return (totalFiles + FILES_PAGE_SIZE - 1) / FILES_PAGE_SIZE; // Ceiling division
}

int FileChooserDataSource::GetGamesTotalPages() const {
    int totalGames = static_cast<int>(games.size());
    return (totalGames + GAMES_PAGE_SIZE - 1) / GAMES_PAGE_SIZE; // Ceiling division
}