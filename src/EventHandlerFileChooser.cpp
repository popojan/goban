#include "EventHandlerFileChooser.h"
#include "ElementGame.h"
#include "Event.h"
#include "EventManager.h"
#include "FileChooserDataSource.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/Input.h>

EventHandlerFileChooser::EventHandlerFileChooser() : dialogDocument(nullptr), dataSource(nullptr), keyListener(nullptr) {
    dataSource = new FileChooserDataSource();
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
    spdlog::info("EventHandlerFileChooser::ProcessEvent value: '{}' type: '{}'", value.c_str(), event.GetType().c_str());

    if (value == "load") {
        ShowDialog();
    }
    else if (value == "cancel") {
        HideDialog();
    }
    else if (value == "files_list_click") {
        // Event delegation - find which row was clicked
        auto target = event.GetTargetElement();
        auto filesList = dialogDocument->GetElementById("files_list");
        if (filesList && target) {
            // Walk up from target to find the row
            Rml::Element* row = target;
            while (row && row->GetParentNode() != filesList) {
                row = row->GetParentNode();
            }
            if (row && row->GetParentNode() == filesList) {
                // Find index of this row
                for (int i = 0; i < filesList->GetNumChildren(); ++i) {
                    if (filesList->GetChild(i) == row) {
                        spdlog::info("File row {} clicked", i);
                        handleFileSelection(i);
                        break;
                    }
                }
            }
        }
    }
    else if (value == "games_list_click") {
        // Event delegation - find which row was clicked
        auto target = event.GetTargetElement();
        auto gamesList = dialogDocument->GetElementById("games_list");
        if (gamesList && target) {
            // Walk up from target to find the row
            Rml::Element* row = target;
            while (row && row->GetParentNode() != gamesList) {
                row = row->GetParentNode();
            }
            if (row && row->GetParentNode() == gamesList) {
                // Find index of this row
                for (int i = 0; i < gamesList->GetNumChildren(); ++i) {
                    if (gamesList->GetChild(i) == row) {
                        spdlog::info("Game row {} clicked", i);
                        handleGameSelection(i);
                        break;
                    }
                }
            }
        }
    }
    else if (value == "refresh") {
        dataSource->RefreshFiles();
        populateFilesList();
        updatePaginationInfo();
    }
    else if (value == "open_file") {
        // Load the selected game
        const SGFGameInfo* game = dataSource->GetSelectedGame();
        if (game) {
            std::string filePath = dataSource->GetSelectedFilePath();
            int gameIndex = dataSource->GetSelectedGameIndex();
            spdlog::info("Opening SGF file: {} game index: {}", filePath, gameIndex);

            // Get the game element and load the SGF
            if (dialogDocument) {
                auto context = dialogDocument->GetContext();
                auto gameDoc = context->GetDocument("game_window");
                if (gameDoc) {
                    auto gameElement = dynamic_cast<ElementGame*>(gameDoc->GetElementById("game"));
                    if (gameElement) {
                        gameElement->getGameThread().loadSGF(filePath, gameIndex);
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

        // Add click listeners on the list divs for event delegation
        auto filesList = dialogDocument->GetElementById("files_list");
        if (filesList) {
            spdlog::info("Adding click listener to files_list");
            filesList->AddEventListener(Rml::EventId::Mouseup, new Event("files_list_click"));
        } else {
            spdlog::error("files_list not found!");
        }
        auto gamesList = dialogDocument->GetElementById("games_list");
        if (gamesList) {
            spdlog::info("Adding click listener to games_list");
            gamesList->AddEventListener(Rml::EventId::Mouseup, new Event("games_list_click"));
        } else {
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

void EventHandlerFileChooser::ShowDialog() {
    if (!dialogDocument) {
        spdlog::error("Dialog document not loaded");
        return;
    }

    // Save current selection before refresh
    std::string selectedFilePath = dataSource->GetSelectedFilePath();
    int selectedGameIdx = dataSource->GetSelectedGameIndex();

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
                // Update visual selection for game
                auto gamesList = dialogDocument->GetElementById("games_list");
                if (gamesList && selectedGameIdx < gamesList->GetNumChildren()) {
                    // Clear default selection from first item
                    for (int i = 0; i < gamesList->GetNumChildren(); ++i) {
                        gamesList->GetChild(i)->SetClass("selected", false);
                    }
                    gamesList->GetChild(selectedGameIdx)->SetClass("selected", true);
                }
            }
            // Update visual selection for file
            auto filesList = dialogDocument->GetElementById("files_list");
            if (filesList && newIndex < filesList->GetNumChildren()) {
                filesList->GetChild(newIndex)->SetClass("selected", true);
            }
        }
    }

    dialogDocument->Show(Rml::ModalFlag::Modal, Rml::FocusFlag::Auto);
    dialogDocument->PullToFront();
    spdlog::info("Dialog shown, IsModal={}", dialogDocument->IsModal());
    requestRepaint();
}

void EventHandlerFileChooser::HideDialog() {
    if (dialogDocument) {
        dialogDocument->Hide();
        requestRepaint();
    }
}

void EventHandlerFileChooser::populateFilesList() {
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
                if (i == dataSource->GetSelectedFileIndex()) {
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

    for (int i = 0; i < numRows; ++i) {
        std::vector<std::string> row;
        std::vector<std::string> columns = {"title", "players"};
        dataSource->GetRow(row, "games", i, columns);

        if (row.size() >= 2) {
            // Create row div element
            Rml::ElementPtr rowElement = Rml::Factory::InstanceElement(
                gamesList, "div", "div", Rml::XMLAttributes());

            if (rowElement) {
                rowElement->SetClass("game_row", true);
                rowElement->SetAttribute("data-index", std::to_string(i));

                // Mark first game as selected by default
                if (i == 0) {
                    rowElement->SetClass("selected", true);
                }

                // Create title span
                Rml::ElementPtr titleSpan = Rml::Factory::InstanceElement(
                    rowElement.get(), "span", "span", Rml::XMLAttributes());
                if (titleSpan) {
                    titleSpan->SetClass("game_title", true);
                    titleSpan->SetInnerRML(row[0].c_str());
                    rowElement->AppendChild(std::move(titleSpan));
                }

                // Create players span
                Rml::ElementPtr playersSpan = Rml::Factory::InstanceElement(
                    rowElement.get(), "span", "span", Rml::XMLAttributes());
                if (playersSpan) {
                    playersSpan->SetClass("game_players", true);
                    playersSpan->SetInnerRML(row[1].c_str());
                    rowElement->AppendChild(std::move(playersSpan));
                }

                // Add mousedown listener directly to row
                std::string eventValue = "select_game:" + std::to_string(i);
                rowElement->AddEventListener(Rml::EventId::Mousedown, new Event(eventValue));

                gamesList->AppendChild(std::move(rowElement));
            }
        }
    }
}

void EventHandlerFileChooser::handleFileSelection(int index) {
    if (!dialogDocument) return;

    // Clear previous selection
    auto filesList = dialogDocument->GetElementById("files_list");
    if (filesList) {
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

    dataSource->SelectFile(actualIndex);

    // Check if we navigated to a directory
    const FileEntry* file = dataSource->GetSelectedFile();
    if (file && file->isDirectory) {
        populateFilesList();
        updateCurrentPath();
        updatePaginationInfo();
    } else if (file && !file->isDirectory) {
        // It's an SGF file - populate games list
        populateGamesList();
        updatePaginationInfo();
    }
}

void EventHandlerFileChooser::handleGameSelection(int index) {
    if (!dialogDocument) return;

    // Clear previous selection
    auto gamesList = dialogDocument->GetElementById("games_list");
    if (gamesList) {
        for (int i = 0; i < gamesList->GetNumChildren(); ++i) {
            gamesList->GetChild(i)->SetClass("selected", false);
        }
        // Set new selection
        if (index >= 0 && index < gamesList->GetNumChildren()) {
            gamesList->GetChild(index)->SetClass("selected", true);
        }
    }

    // Calculate actual game index based on pagination
    int actualIndex = (dataSource->GetGamesCurrentPage() - 1) * FileChooserDataSource::GetGamesPageSize() + index;
    dataSource->SelectGame(actualIndex);
}

void EventHandlerFileChooser::updateCurrentPath() {
    if (!dialogDocument) return;

    auto pathElement = dialogDocument->GetElementById("current_path");
    if (pathElement) {
        pathElement->SetInnerRML(dataSource->GetCurrentPath().string().c_str());
    }
}

void EventHandlerFileChooser::updatePaginationInfo() {
    if (!dialogDocument) return;

    // Files pagination
    auto filesPageInfo = dialogDocument->GetElementById("files_page_info");
    if (filesPageInfo) {
        char buf[64];
        snprintf(buf, sizeof(buf), strPageInfoFmt.c_str(),
                 dataSource->GetFilesCurrentPage(), std::max(1, dataSource->GetFilesTotalPages()));
        filesPageInfo->SetInnerRML(buf);
    }

    auto filesPrev = dialogDocument->GetElementById("files_prev");
    if (filesPrev) {
        filesPrev->SetClass("disabled", !dataSource->CanGoToFilesPrevPage());
    }

    auto filesNext = dialogDocument->GetElementById("files_next");
    if (filesNext) {
        filesNext->SetClass("disabled", !dataSource->CanGoToFilesNextPage());
    }

    // Games pagination
    auto gamesPageInfo = dialogDocument->GetElementById("games_page_info");
    if (gamesPageInfo) {
        char buf[64];
        snprintf(buf, sizeof(buf), strPageInfoFmt.c_str(),
                 dataSource->GetGamesCurrentPage(), std::max(1, dataSource->GetGamesTotalPages()));
        gamesPageInfo->SetInnerRML(buf);
    }

    auto gamesPrev = dialogDocument->GetElementById("games_prev");
    if (gamesPrev) {
        gamesPrev->SetClass("disabled", !dataSource->CanGoToGamesPrevPage());
    }

    auto gamesNext = dialogDocument->GetElementById("games_next");
    if (gamesNext) {
        gamesNext->SetClass("disabled", !dataSource->CanGoToGamesNextPage());
    }
}

void EventHandlerFileChooser::clearGridSelection(Rml::Element* grid) {
    if (!grid) return;
    for (int i = 0; i < grid->GetNumChildren(); ++i) {
        grid->GetChild(i)->SetClass("selected", false);
    }
}

void EventHandlerFileChooser::requestRepaint() {
    if (dialogDocument) {
        auto context = dialogDocument->GetContext();
        auto gameDoc = context->GetDocument("game_window");
        if (gameDoc) {
            auto gameElement = dynamic_cast<ElementGame*>(gameDoc->GetElementById("game"));
            if (gameElement) {
                gameElement->requestRepaint();
            }
        }
    }
}

std::string EventHandlerFileChooser::getTemplateString(const char* templateId, const char* defaultValue) {
    if (!dialogDocument) return defaultValue;

    auto templateElement = dialogDocument->GetElementById(templateId);
    if (templateElement) {
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
}
