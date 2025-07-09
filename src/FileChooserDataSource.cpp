#include "FileChooserDataSource.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <fstream>
#include <sstream>

FileChooserDataSource* FileChooserDataSource::instance = nullptr;

FileChooserDataSource::FileChooserDataSource() 
    : Rocket::Controls::DataSource("file_chooser")
    , currentPath("data/sgf")
    , selectedFileIndex(-1)
    , selectedGameIndex(-1) {
    
    instance = this;
    RefreshFiles();
}

FileChooserDataSource::~FileChooserDataSource() {
    instance = nullptr;
}

void FileChooserDataSource::Initialize() {
    new FileChooserDataSource();
}

void FileChooserDataSource::Shutdown() {
    delete instance;
}

FileChooserDataSource* FileChooserDataSource::GetInstance() {
    return instance;
}

void FileChooserDataSource::GetRow(Rocket::Core::StringList& row, const Rocket::Core::String& table, int row_index, const Rocket::Core::StringList& columns) {
    if (table == "files") {
        if (row_index < 0 || row_index >= static_cast<int>(files.size())) {
            return;
        }
        
        const FileEntry& file = files[row_index];
        
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
        if (row_index < 0 || row_index >= static_cast<int>(games.size())) {
            return;
        }
        
        const SGFGameInfo& game = games[row_index];
        
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
                row.push_back(Rocket::Core::String(8, "%d", game.moveCount));
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
        return static_cast<int>(files.size());
    } else if (table == "games") {
        return static_cast<int>(games.size());
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
    
    // Store old sizes for proper notification
    int oldFileCount = static_cast<int>(files.size());
    int oldGameCount = static_cast<int>(games.size());
    
    refreshFileList();
    
    // Clear games when changing directories
    games.clear();
    selectedFileIndex = -1;
    selectedGameIndex = -1;
    
    // Notify about row changes using proper RocketLib pattern
    spdlog::debug("Notifying file table change: {} -> {}", oldFileCount, GetNumRows("files"));
    NotifyRowChange("files");  // Refresh entire files table
    
    spdlog::debug("Notifying games table change: {} -> {}", oldGameCount, GetNumRows("games"));
    NotifyRowChange("games");  // Refresh entire games table
}

void FileChooserDataSource::refreshFileList() {
    files.clear();
    
    try {
        if (std::filesystem::exists(currentPath) && std::filesystem::is_directory(currentPath)) {
            for (const auto& entry : std::filesystem::directory_iterator(currentPath)) {
                FileEntry fileEntry;
                fileEntry.name = entry.path().filename().string();
                fileEntry.fullPath = entry.path();
                fileEntry.isDirectory = entry.is_directory();
                
                if (fileEntry.isDirectory) {
                    fileEntry.type = "Directory";
                } else if (fileEntry.name.find(".sgf") != std::string::npos) {
                    fileEntry.type = "SGF File";
                } else {
                    // Skip non-SGF files
                    continue;
                }
                
                files.push_back(fileEntry);
                if (files.size() > 3) {
                    break;
                }
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
        // Preview SGF file
        //previewSGF(file.fullPath.string());
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

std::string FileChooserDataSource::GetSelectedFilePath() const {
    const FileEntry* file = GetSelectedFile();
    if (file && !file->isDirectory) {
        return file->fullPath.string();
    }
    return "";
}

void FileChooserDataSource::previewSGF(const std::string& filePath) {
    spdlog::debug("previewSGF() called for: {}", filePath);
    
    int oldGameCount = static_cast<int>(games.size());
    games = parseSGFGames(filePath);
    selectedGameIndex = games.empty() ? -1 : 0;
    
    spdlog::debug("Parsed {} games from SGF file", games.size());
    
    // Notify that games table has changed using proper pattern
    spdlog::debug("Notifying games table change: {} -> {}", oldGameCount, GetNumRows("games"));
    NotifyRowChange("games");  // Refresh entire games table
}

std::vector<SGFGameInfo> FileChooserDataSource::parseSGFGames(const std::string& filePath) {
    std::vector<SGFGameInfo> gameList;
    
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            spdlog::error("Cannot open SGF file: {}", filePath);
            return gameList;
        }
        
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        
        // Simple SGF parsing - look for game trees
        size_t pos = 0;
        int gameIndex = 0;
        
        while ((pos = content.find("(;", pos)) != std::string::npos) {
            SGFGameInfo gameInfo;
            gameInfo.blackPlayer = "Unknown";
            gameInfo.whitePlayer = "Unknown";
            gameInfo.boardSize = 19;
            gameInfo.komi = 0.0f;
            gameInfo.moveCount = 0;
            gameInfo.gameIndex = gameIndex++;
            
            // Find the end of this game
            size_t gameEnd = content.find(";)", pos);
            if (gameEnd == std::string::npos) {
                gameEnd = content.length();
            }
            
            std::string gameContent = content.substr(pos, gameEnd - pos);
            
            // Parse basic properties
            auto extractProperty = [&](const std::string& prop) -> std::string {
                std::string pattern = prop + "[";
                size_t start = gameContent.find(pattern);
                if (start != std::string::npos) {
                    start += pattern.length();
                    size_t end = gameContent.find("]", start);
                    if (end != std::string::npos) {
                        return gameContent.substr(start, end - start);
                    }
                }
                return "";
            };
            
            std::string bp = extractProperty("PB");
            std::string wp = extractProperty("PW");
            std::string sz = extractProperty("SZ");
            std::string km = extractProperty("KM");
            std::string re = extractProperty("RE");
            std::string dt = extractProperty("DT");
            
            if (!bp.empty()) gameInfo.blackPlayer = bp;
            if (!wp.empty()) gameInfo.whitePlayer = wp;
            if (!sz.empty()) gameInfo.boardSize = std::stoi(sz);
            if (!km.empty()) gameInfo.komi = std::stof(km);
            if (!re.empty()) gameInfo.gameResult = re;
            if (!dt.empty()) gameInfo.date = dt;
            
            // Count moves (simple approach - count ;B[ and ;W[ occurrences)
            size_t movePos = 0;
            while ((movePos = gameContent.find(";B[", movePos)) != std::string::npos) {
                gameInfo.moveCount++;
                movePos += 3;
            }
            movePos = 0;
            while ((movePos = gameContent.find(";W[", movePos)) != std::string::npos) {
                gameInfo.moveCount++;
                movePos += 3;
            }
            
            // Create display strings
            char titleBuffer[64];
            snprintf(titleBuffer, sizeof(titleBuffer), "Game %d (%dx%d, %d moves)", 
                    gameIndex, gameInfo.boardSize, gameInfo.boardSize, gameInfo.moveCount);
            gameInfo.title = std::string(titleBuffer);
            gameInfo.players = gameInfo.blackPlayer + " vs " + gameInfo.whitePlayer;
            
            gameList.push_back(gameInfo);
            pos = gameEnd + 1;
        }
        
    } catch (const std::exception& e) {
        spdlog::error("Error parsing SGF file: {}", e.what());
    }
    
    return gameList;
}