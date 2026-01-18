#ifndef GOBAN_FILECHOOSERDATASOURCE_H
#define GOBAN_FILECHOOSERDATASOURCE_H

// Note: RmlUi removed the DataSource/DataGrid system from Controls
// This class is temporarily stubbed - file chooser functionality disabled
#include <filesystem>
#include <vector>
#include <memory>
#include <string>

// Forward declaration
namespace LibSgfcPlusPlus {
    class ISgfcGame;
}

struct FileEntry {
    std::string name;
    std::string type;
    bool isDirectory;
    std::filesystem::path fullPath;
};

struct SGFGameInfo {
    std::string title;
    std::string players;
    std::string blackPlayer;
    std::string whitePlayer;
    std::string gameResult;
    std::string date;
    int moveCount;
    int boardSize;
    float komi;
    int gameIndex;
};

// Temporarily disabled - DataSource was removed from RmlUi
class FileChooserDataSource {
public:
    FileChooserDataSource(const std::string& gamesPath = "./games");
    ~FileChooserDataSource();

    // Legacy DataSource interface - stubbed out
    void GetRow(std::vector<std::string>& row, const std::string& table, int row_index, const std::vector<std::string>& columns);
    int GetNumRows(const std::string& table);

    // File navigation
    void SetCurrentPath(const std::filesystem::path& path);
    const std::filesystem::path& GetCurrentPath() const { return currentPath; }
    void NavigateUp();
    void RefreshFiles();
    
    // File selection
    void SelectFile(int fileIndex);
    const FileEntry* GetSelectedFile() const;
    int FindFileByPath(const std::string& path) const;
    const std::vector<FileEntry>& GetFiles() const { return files; }
    
    // Game selection
    void SelectGame(int gameIndex);
    const SGFGameInfo* GetSelectedGame() const;
    int GetSelectedGameIndex() const;
    int GetSelectedFileIndex() const { return selectedFileIndex; }
    
    // Get selected file path for loading
    std::string GetSelectedFilePath() const;
    
    // Explicitly load games for the selected SGF file
    void LoadSelectedFileGames();

    // Pagination methods
    void SetFilesPage(int page);
    void SetGamesPage(int page);
    int GetFilesCurrentPage() const { return filesCurrentPage; }
    int GetGamesCurrentPage() const { return gamesCurrentPage; }
    int GetFilesTotalPages() const;
    int GetGamesTotalPages() const;
    bool CanGoToFilesPrevPage() const { return filesCurrentPage > 1; }
    bool CanGoToFilesNextPage() const { return filesCurrentPage < GetFilesTotalPages(); }
    bool CanGoToGamesPrevPage() const { return gamesCurrentPage > 1; }
    bool CanGoToGamesNextPage() const { return gamesCurrentPage < GetGamesTotalPages(); }
    
    // Page size constants
    static int GetFilesPageSize() { return FILES_PAGE_SIZE; }
    static int GetGamesPageSize() { return GAMES_PAGE_SIZE; }

    // Localization
    void SetLocalizedStrings(const std::string& directory, const std::string& sgfFile,
                            const std::string& up, const std::string& gameInfoFmt);

private:
    // Localized strings (with defaults)
    std::string strDirectory = "Directory";
    std::string strSgfFile = "SGF File";
    std::string strUp = "Up";
    std::string strGameInfoFmt = "Game %d (%dx%d, %d moves)";
    static const int FILES_PAGE_SIZE = 10000;
    static const int GAMES_PAGE_SIZE = 10000;
    
    std::filesystem::path currentPath;
    std::vector<FileEntry> files;
    std::vector<SGFGameInfo> games;
    
    int selectedFileIndex;
    int selectedGameIndex;
    
    // Pagination state
    int filesCurrentPage;
    int gamesCurrentPage;
    
    void refreshFileList();
    void previewSGF(const std::string& filePath);
    std::vector<SGFGameInfo> parseSGFGames(const std::string& filePath);
    int countMovesInGame(std::shared_ptr<LibSgfcPlusPlus::ISgfcGame> game);
};

#endif // GOBAN_FILECHOOSERDATASOURCE_H