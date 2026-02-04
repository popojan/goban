#include "EventHandlerFileChooser.h"
#include "ElementGame.h"
#include "Event.h"
#include "EventManager.h"
#include "FileChooserDataSource.h"
#include "Configuration.h"
#include "UserSettings.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/Input.h>

extern std::shared_ptr<Configuration> config;

EventHandlerFileChooser::EventHandlerFileChooser() : dialogDocument(nullptr), dataSource(nullptr), keyListener(nullptr) {
    // Read games path from config
    std::string gamesPath = "./games";
    if (config && config->data.contains("sgf_dialog")) {
        gamesPath = config->data["sgf_dialog"].value("games_path", "./games");
    }
    dataSource = new FileChooserDataSource(gamesPath);
    keyListener = new DialogKeyListener(this);
}

// DialogKeyListener implementation
void DialogKeyListener::ProcessEvent(Rml::Event& event) {
    auto key = event.GetParameter<int>("key_identifier", 0);
    // ESC key in RmlUi is KI_ESCAPE = 81
    if (key == Rml::Input::KI_ESCAPE) {
        // Hide on keyup so dialog remains modal to consume both events
        if (event.GetType() == "keyup") {
            spdlog::info("ESC released, closing dialog");
            handler->HideDialog();
        }
        event.StopImmediatePropagation();
    }
}

EventHandlerFileChooser::~EventHandlerFileChooser() {
    if (dataSource) {
        delete dataSource;
        dataSource = nullptr;
    }
}

void EventHandlerFileChooser::ProcessEvent(Rml::Event& event, const Rml::String& value) {
    spdlog::debug("EventHandlerFileChooser::ProcessEvent value: '{}' type: '{}'", value.c_str(), event.GetType().c_str());

    if (value == "load") {
        ShowDialog();
    }
    else if (value == "cancel") {
        HideDialog();
    }
    else if (value == "refresh") {
        dataSource->RefreshFiles();
        populateFilesList();
        updatePaginationInfo();
    }
    else if (value == "open_file") {
        // Load the selected game
        if (dataSource->GetSelectedGame()) {
            std::string filePath = dataSource->GetSelectedFilePath();
            int gameIndex = dataSource->GetSelectedGameIndex();
            spdlog::info("Opening SGF file: {} game index: {}", filePath, gameIndex);

            // Get the game element and load the SGF
            if (dialogDocument) {
                auto context = dialogDocument->GetContext();
                if (auto gameDoc = context->GetDocument("game_window")) {
                    if (auto gameElement = dynamic_cast<ElementGame*>(gameDoc->GetElementById("game"))) {
                        bool tsumego = isTsumegoToggled();
                        gameElement->setTsumegoMode(tsumego);
                        gameElement->getGameThread().loadSGF(filePath, gameIndex, tsumego);
                        gameElement->refreshPlayerDropdowns();  // Update dropdowns with SGF player names
                        if (tsumego) {
                            gameElement->getGameThread().autoPlayTsumegoSetup();
                        }

                        gameElement->updateNavigationOverlay();  // Update markup and variation overlays
                        gameElement->requestRepaint();  // Ensure UI updates
                    }
                }
            }
            HideDialog();
        } else {
            spdlog::warn("No game selected");
        }
    }
    else if (value == "files_prev_page") {
        if (dataSource->CanGoToFilesPrevPage()) {
            dataSource->SetFilesPage(dataSource->GetFilesCurrentPage() - 1);
            populateFilesList();
            updatePaginationInfo();
        }
    }
    else if (value == "files_next_page") {
        if (dataSource->CanGoToFilesNextPage()) {
            dataSource->SetFilesPage(dataSource->GetFilesCurrentPage() + 1);
            populateFilesList();
            updatePaginationInfo();
        }
    }
    else if (value == "games_prev_page") {
        if (dataSource->CanGoToGamesPrevPage()) {
            dataSource->SetGamesPage(dataSource->GetGamesCurrentPage() - 1);
            populateGamesList();
            updatePaginationInfo();
        }
    }
    else if (value == "games_next_page") {
        if (dataSource->CanGoToGamesNextPage()) {
            dataSource->SetGamesPage(dataSource->GetGamesCurrentPage() + 1);
            populateGamesList();
            updatePaginationInfo();
        }
    }
    else if (value.substr(0, 11) == "select_file") {
        // Format: select_file:index
        size_t colonPos = value.find(':');
        if (colonPos != std::string::npos) {
            int index = std::stoi(value.substr(colonPos + 1).c_str());
            handleFileSelection(index);
            event.StopImmediatePropagation();
            return;
        }
    }
    else if (value.substr(0, 11) == "select_game") {
        // Format: select_game:index
        size_t colonPos = value.find(':');
        if (colonPos != std::string::npos) {
            int index = std::stoi(value.substr(colonPos + 1).c_str());
            handleGameSelection(index);
            event.StopImmediatePropagation();
            return;
        }
    }

    event.StopPropagation();
}

void EventHandlerFileChooser::LoadDialog(Rml::Context* context) {
    if (!context) return;

    std::string rmlPath = EventManager::GetPrefix() + "/open.rml";
    dialogDocument = context->LoadDocument(rmlPath.c_str());

    if (dialogDocument) {
        dialogDocument->Hide();
        // Add keyboard listener for ESC key (both down and up)
        dialogDocument->AddEventListener(Rml::EventId::Keydown, keyListener);
        dialogDocument->AddEventListener(Rml::EventId::Keyup, keyListener);

        // Verify list elements exist (mousedown listeners are added per-row in populate methods)
        if (!dialogDocument->GetElementById("files_list")) {
            spdlog::error("files_list not found!");
        }
        if (!dialogDocument->GetElementById("games_list")) {
            spdlog::error("games_list not found!");
        }

        initializeLocalization();
        spdlog::info("File chooser dialog loaded from {}", rmlPath);
    } else {
        spdlog::error("Failed to load file chooser dialog from {}", rmlPath);
    }
}

void EventHandlerFileChooser::UnloadDialog(Rml::Context* context) {
    if (dialogDocument && context) {
        context->UnloadDocument(dialogDocument);
        dialogDocument = nullptr;
    }
}

void EventHandlerFileChooser::ShowDialog(const std::string& currentFile, int currentGameIndex) {
    if (!dialogDocument) {
        spdlog::error("Dialog document not loaded");
        return;
    }

    // Remember board's current file/game for re-selection when navigating back
    boardFile = currentFile;
    boardGameIndex = currentGameIndex;

    // Save current selection before refresh
    std::string selectedFilePath = dataSource->GetSelectedFilePath();
    int selectedGameIdx = dataSource->GetSelectedGameIndex();

    // Fall back to current game if no previous dialog selection
    if (selectedFilePath.empty() && !currentFile.empty()) {
        selectedFilePath = currentFile;
        selectedGameIdx = currentGameIndex;
    }

    // Refresh file list
    dataSource->RefreshFiles();
    populateFilesList();
    updateCurrentPath();
    updatePaginationInfo();

    // Restore selection if file still exists
    if (!selectedFilePath.empty()) {
        int newIndex = dataSource->FindFileByPath(selectedFilePath);
        if (newIndex >= 0) {
            dataSource->SelectFile(newIndex);
            dataSource->LoadSelectedFileGames();
            populateGamesList();
            // Restore game selection
            if (selectedGameIdx >= 0 && selectedGameIdx < dataSource->GetNumRows("games")) {
                dataSource->SelectGame(selectedGameIdx);
                // Update visual selection for game (account for header row at index 0)
                auto gamesList = dialogDocument->GetElementById("games_list");
                int elementIndex = selectedGameIdx + 1;  // +1 for header row
                if (gamesList && elementIndex < gamesList->GetNumChildren()) {
                    // Clear default selection from first data item
                    for (int i = 1; i < gamesList->GetNumChildren(); ++i) {
                        gamesList->GetChild(i)->SetClass("selected", false);
                    }
                    gamesList->GetChild(elementIndex)->SetClass("selected", true);
                }
            }
            // Update visual selection for file
            auto filesList = dialogDocument->GetElementById("files_list");
            int rowIndex = newIndex;
            if (dataSource->GetCurrentPath().has_parent_path()) {
                rowIndex += 1;  // Account for ".." row at index 0
            }
            if (filesList && rowIndex < filesList->GetNumChildren()) {
                filesList->GetChild(rowIndex)->SetClass("selected", true);
            }
        }
    }

    dialogDocument->Show(Rml::ModalFlag::Modal, Rml::FocusFlag::Auto);
    dialogDocument->PullToFront();
    spdlog::info("Dialog shown, IsModal={}", dialogDocument->IsModal());
    requestRepaint();
}

void EventHandlerFileChooser::HideDialog() const {
    if (dialogDocument) {
        dialogDocument->Hide();
        requestRepaint();
    }
}

void EventHandlerFileChooser::populateFilesList() const {
    if (!dialogDocument) return;

    auto filesList = dialogDocument->GetElementById("files_list");
    if (!filesList) return;

    // Clear existing items
    while (filesList->GetNumChildren() > 0) {
        filesList->RemoveChild(filesList->GetChild(0));
    }

    int numRows = dataSource->GetNumRows("files");

    for (int i = 0; i < numRows; ++i) {
        std::vector<std::string> row;
        std::vector<std::string> columns = {"name", "type"};
        dataSource->GetRow(row, "files", i, columns);

        if (row.size() >= 2) {
            // Create row div element
            Rml::ElementPtr rowElement = Rml::Factory::InstanceElement(
                filesList, "div", "div", Rml::XMLAttributes());

            if (rowElement) {
                rowElement->SetClass("file_row", true);
                rowElement->SetAttribute("data-index", std::to_string(i));

                // Mark as selected if this is the currently selected file
                // Account for ".." row offset: row index != file index when parent exists
                int expectedRowIndex = dataSource->GetSelectedFileIndex();
                if (dataSource->GetCurrentPath().has_parent_path() && expectedRowIndex >= 0) {
                    expectedRowIndex += 1; // ".." is row 0, first file is row 1
                }
                if (i == expectedRowIndex) {
                    rowElement->SetClass("selected", true);
                }

                // Create name span
                Rml::ElementPtr nameSpan = Rml::Factory::InstanceElement(
                    rowElement.get(), "span", "span", Rml::XMLAttributes());
                if (nameSpan) {
                    nameSpan->SetClass("file_name", true);
                    nameSpan->SetInnerRML(row[0].c_str());
                    rowElement->AppendChild(std::move(nameSpan));
                }

                // Create type span
                Rml::ElementPtr typeSpan = Rml::Factory::InstanceElement(
                    rowElement.get(), "span", "span", Rml::XMLAttributes());
                if (typeSpan) {
                    typeSpan->SetClass("file_type", true);
                    typeSpan->SetInnerRML(row[1].c_str());
                    rowElement->AppendChild(std::move(typeSpan));
                }

                // Add mousedown listener directly to row
                std::string eventValue = "select_file:" + std::to_string(i);
                rowElement->AddEventListener(Rml::EventId::Mousedown, new Event(eventValue));

                filesList->AppendChild(std::move(rowElement));
            }
        }
    }
}

void EventHandlerFileChooser::populateGamesList() {
    if (!dialogDocument) return;

    auto gamesList = dialogDocument->GetElementById("games_list");
    if (!gamesList) return;

    // Clear existing items
    while (gamesList->GetNumChildren() > 0) {
        gamesList->RemoveChild(gamesList->GetChild(0));
    }

    int numRows = dataSource->GetNumRows("games");
    if (numRows == 0) return;

    // Create header row
    createGameHeaderRow(gamesList);

    // Map config column names to data source column names
    std::map<std::string, std::string> colNameMapping = {
        {"index", "title"},  // We'll extract index from title for now, or use a special column
        {"board", "board_size"},
        {"black", "black"},
        {"white", "white"},
        {"result", "result"},
        {"moves", "moves"},
        {"komi", "komi"},
        {"players", "players"}
    };

    for (int i = 0; i < numRows; ++i) {
        // Build the columns list based on gameColumns config (excluding "index" which is generated)
        std::vector<std::string> requestedColumns;
        for (const auto& col : gameColumns) {
            if (col == "index") continue;  // Index is generated, not from data source
            auto it = colNameMapping.find(col);
            if (it != colNameMapping.end()) {
                requestedColumns.push_back(it->second);
            }
        }

        std::vector<std::string> row;
        dataSource->GetRow(row, "games", i, requestedColumns);

        // Create row div element
        Rml::ElementPtr rowElement = Rml::Factory::InstanceElement(
            gamesList, "div", "div", Rml::XMLAttributes());

        if (rowElement) {
            rowElement->SetClass("game_row", true);
            rowElement->SetAttribute("data-index", std::to_string(i));

            // Mark selected game (defaults to first if no override)
            int actualGameIndex = (dataSource->GetGamesCurrentPage() - 1) * FileChooserDataSource::GetGamesPageSize() + i;
            if (actualGameIndex == dataSource->GetSelectedGameIndex()) {
                rowElement->SetClass("selected", true);
            }

            // Create spans for each configured column
            size_t rowIdx = 0;
            for (const auto& colType : gameColumns) {
                std::string cellText;

                if (colType == "index") {
                    // Generate 1-based index
                    cellText = std::to_string(i + 1);
                } else if (rowIdx < row.size()) {
                    cellText = row[rowIdx];
                    rowIdx++;
                }

                if (auto span = createColumnSpan(rowElement.get(), colType, cellText)) {
                    rowElement->AppendChild(std::move(span));
                }
            }

            // Add mousedown listener directly to row
            std::string eventValue = "select_game:" + std::to_string(i);
            rowElement->AddEventListener(Rml::EventId::Mousedown, new Event(eventValue));

            gamesList->AppendChild(std::move(rowElement));
        }
    }
}

void EventHandlerFileChooser::handleFileSelection(int index) {
    if (!dialogDocument) return;

    // Clear previous selection
    if (auto filesList = dialogDocument->GetElementById("files_list")) {
        for (int i = 0; i < filesList->GetNumChildren(); ++i) {
            filesList->GetChild(i)->SetClass("selected", false);
        }
        // Set new selection
        if (index >= 0 && index < filesList->GetNumChildren()) {
            filesList->GetChild(index)->SetClass("selected", true);
        }
    }

    // Handle the selection in data source
    // Check if it's the ".." entry (navigate up)
    if (index == 0 && dataSource->GetCurrentPath().has_parent_path()) {
        dataSource->NavigateUp();
        populateFilesList();
        updateCurrentPath();
        updatePaginationInfo();
        return;
    }

    // Adjust index for actual file (accounting for ".." row if present)
    int fileIndex = index;
    if (dataSource->GetCurrentPath().has_parent_path()) {
        fileIndex = index - 1;
    }

    // Calculate actual file index based on pagination
    int actualIndex;
    if (dataSource->GetCurrentPath().has_parent_path()) {
        actualIndex = (dataSource->GetFilesCurrentPage() - 1) * (FileChooserDataSource::GetFilesPageSize() - 1) + fileIndex;
    } else {
        actualIndex = (dataSource->GetFilesCurrentPage() - 1) * FileChooserDataSource::GetFilesPageSize() + fileIndex;
    }

    // Check what type of file before selecting (SelectFile may navigate and clear selection)
    const auto& files = dataSource->GetFiles();
    bool isDirectory = (actualIndex >= 0 && actualIndex < static_cast<int>(files.size()) && files[actualIndex].isDirectory);

    dataSource->SelectFile(actualIndex);

    if (isDirectory) {
        // Directory navigation happened - refresh file list
        populateFilesList();
        updateCurrentPath();
        updatePaginationInfo();
    } else {
        // It's an SGF file - populate games list
        // Re-select board's game if navigating back to the same file
        if (!boardFile.empty() && boardGameIndex >= 0) {
            std::error_code ec1, ec2;
            auto selectedPath = std::filesystem::weakly_canonical(dataSource->GetSelectedFilePath(), ec1);
            auto boardPath = std::filesystem::weakly_canonical(boardFile, ec2);
            if (!ec1 && !ec2 && selectedPath == boardPath) {
                dataSource->SelectGame(boardGameIndex);
            }
        }
        populateGamesList();
        updatePaginationInfo();
        // Auto-detect tsumego based on first game's properties
        setTsumegoToggle(dataSource->isTsumegoDetected());
    }
}

void EventHandlerFileChooser::handleGameSelection(int index) const {
    if (!dialogDocument) return;

    // Clear previous selection (skip header row at index 0)
    if (auto gamesList = dialogDocument->GetElementById("games_list")) {
        for (int i = 1; i < gamesList->GetNumChildren(); ++i) {
            gamesList->GetChild(i)->SetClass("selected", false);
        }
        // Set new selection (index is 0-based for data, but element is +1 due to header)
        int elementIndex = index + 1;  // Account for header row
        if (elementIndex >= 1 && elementIndex < gamesList->GetNumChildren()) {
            gamesList->GetChild(elementIndex)->SetClass("selected", true);
        }
    }

    // Calculate actual game index based on pagination
    int actualIndex = (dataSource->GetGamesCurrentPage() - 1) * FileChooserDataSource::GetGamesPageSize() + index;
    dataSource->SelectGame(actualIndex);
}

void EventHandlerFileChooser::updateCurrentPath() const {
    if (!dialogDocument) return;

    if (auto pathElement = dialogDocument->GetElementById("current_path")) {
        pathElement->SetInnerRML(dataSource->GetCurrentPath().u8string().c_str());
    }
}

void EventHandlerFileChooser::updatePaginationInfo() const {
    if (!dialogDocument) return;

    // Files pagination
    if (auto filesPageInfo = dialogDocument->GetElementById("files_page_info")) {
        char buf[64];
        snprintf(buf, sizeof(buf), strPageInfoFmt.c_str(),
                 dataSource->GetFilesCurrentPage(), std::max(1, dataSource->GetFilesTotalPages()));
        filesPageInfo->SetInnerRML(buf);
    }

    if (auto filesPrev = dialogDocument->GetElementById("files_prev")) {
        filesPrev->SetClass("disabled", !dataSource->CanGoToFilesPrevPage());
    }

    if (auto filesNext = dialogDocument->GetElementById("files_next")) {
        filesNext->SetClass("disabled", !dataSource->CanGoToFilesNextPage());
    }

    // Games pagination
    if (auto gamesPageInfo = dialogDocument->GetElementById("games_page_info")) {
        char buf[64];
        snprintf(buf, sizeof(buf), strPageInfoFmt.c_str(),
                 dataSource->GetGamesCurrentPage(), std::max(1, dataSource->GetGamesTotalPages()));
        gamesPageInfo->SetInnerRML(buf);
    }

    if (auto gamesPrev = dialogDocument->GetElementById("games_prev")) {
        gamesPrev->SetClass("disabled", !dataSource->CanGoToGamesPrevPage());
    }

    if (auto gamesNext = dialogDocument->GetElementById("games_next")) {
        gamesNext->SetClass("disabled", !dataSource->CanGoToGamesNextPage());
    }
}

void EventHandlerFileChooser::clearGridSelection(const Rml::Element* grid) {
    if (!grid) return;
    for (int i = 0; i < grid->GetNumChildren(); ++i) {
        grid->GetChild(i)->SetClass("selected", false);
    }
}

void EventHandlerFileChooser::requestRepaint() const {
    if (dialogDocument) {
        const auto context = dialogDocument->GetContext();
        if (auto gameDoc = context->GetDocument("game_window")) {
            if (auto gameElement = dynamic_cast<ElementGame*>(gameDoc->GetElementById("game"))) {
                gameElement->requestRepaint();
            }
        }
    }
}

void EventHandlerFileChooser::setTsumegoToggle(bool enabled) const {
    if (!dialogDocument) return;
    if (auto el = dialogDocument->GetElementById("cmdTsumego")) {
        if (enabled)
            el->SetAttribute("checked", "");
        else
            el->RemoveAttribute("checked");
    }
}

bool EventHandlerFileChooser::isTsumegoToggled() const {
    if (!dialogDocument) return false;
    if (auto el = dialogDocument->GetElementById("cmdTsumego")) {
        return el->HasAttribute("checked");
    }
    return false;
}

std::string EventHandlerFileChooser::getTemplateString(const char* templateId, const char* defaultValue) const {
    if (!dialogDocument) return defaultValue;

    if (auto templateElement = dialogDocument->GetElementById(templateId)) {
        return templateElement->GetInnerRML().c_str();
    }
    return defaultValue;
}

void EventHandlerFileChooser::initializeLocalization() {
    if (!dialogDocument) return;

    strPageInfoFmt = getTemplateString("tplPageInfo", "Page %d of %d");

    // Set localized strings in data source
    std::string strDirectory = getTemplateString("tplDirectory", "Directory");
    std::string strSgfFile = getTemplateString("tplSgfFile", "SGF File");
    std::string strUp = getTemplateString("tplUp", "Up");
    std::string strGameInfoFmt = getTemplateString("tplGameInfo", "Game %d (%dx%d, %d moves)");

    dataSource->SetLocalizedStrings(strDirectory, strSgfFile, strUp, strGameInfoFmt);

    // Load column header localized strings
    columnHeaders["index"] = getTemplateString("tplColIndex", "#");
    columnHeaders["board"] = getTemplateString("tplColBoard", "Board");
    columnHeaders["black"] = getTemplateString("tplColBlack", "Black");
    columnHeaders["white"] = getTemplateString("tplColWhite", "White");
    columnHeaders["result"] = getTemplateString("tplColResult", "Result");
    columnHeaders["moves"] = getTemplateString("tplColMoves", "Moves");
    columnHeaders["komi"] = getTemplateString("tplColKomi", "Komi");
    columnHeaders["players"] = getTemplateString("tplColPlayers", "Players");

    // Load game columns configuration
    loadGameColumnsConfig();
}

void EventHandlerFileChooser::loadGameColumnsConfig() {
    using nlohmann::json;

    // Default columns if not configured
    gameColumns = {"index", "board", "black", "white", "result"};

    if (config && config->data.contains("sgf_dialog")) {
        auto sgfDialog = config->data["sgf_dialog"];
        if (sgfDialog.contains("game_columns") && sgfDialog["game_columns"].is_array()) {
            gameColumns.clear();
            for (const auto& col : sgfDialog["game_columns"]) {
                if (col.is_string()) {
                    gameColumns.push_back(col.get<std::string>());
                }
            }
            spdlog::debug("Loaded {} game columns from config", gameColumns.size());
        }
    }
}

void EventHandlerFileChooser::createGameHeaderRow(Rml::Element* gamesList) {
    if (!gamesList) return;

    Rml::ElementPtr headerRow = Rml::Factory::InstanceElement(
        gamesList, "div", "div", Rml::XMLAttributes());

    if (headerRow) {
        headerRow->SetClass("game_header", true);

        for (const auto& colType : gameColumns) {
            auto it = columnHeaders.find(colType);
            std::string headerText = (it != columnHeaders.end()) ? it->second : colType;
            if (auto span = createColumnSpan(headerRow.get(), colType, headerText)) {
                headerRow->AppendChild(std::move(span));
            }
        }

        gamesList->AppendChild(std::move(headerRow));
    }
}

Rml::ElementPtr EventHandlerFileChooser::createColumnSpan(Rml::Element* parent, const std::string& colType, const std::string& text) {
    Rml::ElementPtr span = Rml::Factory::InstanceElement(
        parent, "span", "span", Rml::XMLAttributes());

    if (span) {
        span->SetClass(("col-" + colType).c_str(), true);
        span->SetInnerRML(text.c_str());
    }

    return span;
}
