#ifndef GOBAN_FILECHOOSERDATASOURCE_H
#define GOBAN_FILECHOOSERDATASOURCE_H

#include <Rocket/Controls/DataSource.h>
#include <filesystem>
#include <vector>
#include <memory>

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

class FileChooserDataSource : public Rocket::Controls::DataSource {
public:
    FileChooserDataSource();
    virtual ~FileChooserDataSource();

    // DataSource interface
    virtual void GetRow(Rocket::Core::StringList& row, const Rocket::Core::String& table, int row_index, const Rocket::Core::StringList& columns) override;
    virtual int GetNumRows(const Rocket::Core::String& table) override;

    // File navigation
    void SetCurrentPath(const std::filesystem::path& path);
    const std::filesystem::path& GetCurrentPath() const { return currentPath; }
    void NavigateUp();
    void RefreshFiles();
    
    // File selection
    void SelectFile(int fileIndex);
    const FileEntry* GetSelectedFile() const;
    
    // Game selection
    void SelectGame(int gameIndex);
    const SGFGameInfo* GetSelectedGame() const;
    int GetSelectedGameIndex() const;
    
    // Get selected file path for loading
    std::string GetSelectedFilePath() const;
    
    // Explicitly load games for the selected SGF file
    void LoadSelectedFileGames();

private:
    std::filesystem::path currentPath;
    std::vector<FileEntry> files;
    std::vector<SGFGameInfo> games;
    
    int selectedFileIndex;
    int selectedGameIndex;
    
    void refreshFileList();
    void previewSGF(const std::string& filePath);
    std::vector<SGFGameInfo> parseSGFGames(const std::string& filePath);
    int countMovesInGame(std::shared_ptr<LibSgfcPlusPlus::ISgfcGame> game);
};

#endif // GOBAN_FILECHOOSERDATASOURCE_H