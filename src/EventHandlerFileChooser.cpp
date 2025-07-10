#include "EventHandlerFileChooser.h"
#include "ElementGame.h"
#include "FileChooserDataSource.h"
#include <Rocket/Core/Context.h>
#include <Rocket/Core/ElementDocument.h>
#include <Rocket/Core/Event.h>
#include <Rocket/Controls/ElementDataGrid.h>
#include <Rocket/Controls/ElementDataGridRow.h>

// Remove static context dependency - context will be passed as parameter

EventHandlerFileChooser::EventHandlerFileChooser() : dialogDocument(nullptr), dataSource(nullptr) {
}

EventHandlerFileChooser::~EventHandlerFileChooser() {
    if (dialogDocument) {
        spdlog::debug("Cleaning up dialog document in destructor");
        // Get context from the document before unloading
        auto context = dialogDocument->GetContext();
        if (context) {
            context->UnloadDocument(dialogDocument);
        }
        dialogDocument = nullptr;
    }

    // Clean up data source
    if (dataSource) {
        delete dataSource;
        dataSource = nullptr;
        spdlog::debug("Data source cleaned up in destructor");
    }
}

void EventHandlerFileChooser::ProcessEvent(Rocket::Core::Event& event, const Rocket::Core::String& value) {
    spdlog::debug("EventHandlerFileChooser::ProcessEvent called with value: '{}'", value.CString());
    spdlog::debug("Event type: '{}', target element: '{}'", event.GetType().CString(), 
                  event.GetTargetElement() ? event.GetTargetElement()->GetTagName().CString() : "null");
    
    if (!dataSource) {
        spdlog::error("FileChooserDataSource not initialized");
        return;
    }

    bool repaintNeeded = false;

    if (value == "load") {
        spdlog::debug("Showing file chooser dialog");
        showDialog();
    }
    else if (value == "cancel") {
        spdlog::debug("Cancel button clicked");
        hideDialog();
        repaintNeeded = true;
    }
    else if (value == "refresh") {
        spdlog::debug("Refresh button clicked");
        dataSource->RefreshFiles();
        
        // Clear selections when refreshing
        auto filesGrid = dialogDocument->GetElementById("files_grid");
        auto gamesGrid = dialogDocument->GetElementById("games_grid");
        if (filesGrid) clearGridSelection(filesGrid);
        if (gamesGrid) clearGridSelection(gamesGrid);
        
        updateCurrentPath();
        repaintNeeded = true;
    }
    else if (value == "select_file") {
        spdlog::debug("File selection event triggered");
        // Try to get the row index from the target element
        auto targetElement = event.GetTargetElement();
        if (targetElement) {
            spdlog::debug("Target element found: {}", targetElement->GetTagName().CString());
            // Find the data grid row that contains this element
            auto rowElement = targetElement;
            while (rowElement && rowElement->GetTagName() != "datagridrow") {
                rowElement = rowElement->GetParentNode();
            }
            
            if (rowElement) {
                spdlog::debug("Found datagrid row element");
                auto dataGridRow = dynamic_cast<Rocket::Controls::ElementDataGridRow*>(rowElement);
                if (dataGridRow) {
                    int selectedRow = dataGridRow->GetTableRelativeIndex();
                    spdlog::debug("Selected file row: {}", selectedRow);
                    
                    // Check if this is the ".." row (first row when we can navigate up)
                    if (selectedRow == 0 && dataSource->GetCurrentPath().has_parent_path()) {
                        spdlog::debug("Navigate up (..) row selected");
                        dataSource->NavigateUp();
                        
                        // Clear selections when navigating
                        auto filesGrid = dialogDocument->GetElementById("files_grid");
                        auto gamesGrid = dialogDocument->GetElementById("games_grid");
                        if (filesGrid) clearGridSelection(filesGrid);
                        if (gamesGrid) clearGridSelection(gamesGrid);
                        
                        updateCurrentPath();
                        return;
                    }
                    
                    // Adjust index for regular files (skip the ".." row if present)
                    int fileIndex = selectedRow;
                    if (dataSource->GetCurrentPath().has_parent_path()) {
                        fileIndex = selectedRow - 1; // Account for the ".." row
                    }
                    
                    // Convert page-relative row index to actual file index
                    int actualFileIndex;
                    if (dataSource->GetCurrentPath().has_parent_path()) {
                        // Page 1 has one less file slot due to ".." row
                        if (dataSource->GetFilesCurrentPage() == 1) {
                            actualFileIndex = fileIndex;
                        } else {
                            // For pages after 1, account for the reduced capacity of page 1
                            actualFileIndex = (dataSource->GetFilesPageSize() - 1) + 
                                           (dataSource->GetFilesCurrentPage() - 2) * dataSource->GetFilesPageSize() + fileIndex;
                        }
                    } else {
                        // Normal pagination when no ".." row
                        actualFileIndex = (dataSource->GetFilesCurrentPage() - 1) * dataSource->GetFilesPageSize() + fileIndex;
                    }
                    spdlog::debug("Converted to actual file index: {}", actualFileIndex);
                    
                    // Clear previous selection in files grid
                    auto filesGrid = dialogDocument->GetElementById("files_grid");
                    if (filesGrid) {
                        clearGridSelection(filesGrid);
                    }
                    
                    // Add selected class to clicked row
                    dataGridRow->SetClass("selected", true);
                    
                    dataSource->SelectFile(actualFileIndex);
                    updateCurrentPath();
                } else {
                    spdlog::debug("Failed to cast to ElementDataGridRow");
                }
            } else {
                spdlog::debug("No datagrid row element found");
            }
        } else {
            spdlog::debug("No target element found");
        }
        repaintNeeded = true;
    }
    else if (value == "select_game") {
        spdlog::debug("Game selection event triggered");
        // Try to get the row index from the target element
        auto targetElement = event.GetTargetElement();
        if (targetElement) {
            spdlog::debug("Target element found: {}", targetElement->GetTagName().CString());
            // Find the data grid row that contains this element
            auto rowElement = targetElement;
            while (rowElement && rowElement->GetTagName() != "datagridrow") {
                rowElement = rowElement->GetParentNode();
            }
            
            if (rowElement) {
                spdlog::debug("Found datagrid row element");
                auto dataGridRow = dynamic_cast<Rocket::Controls::ElementDataGridRow*>(rowElement);
                if (dataGridRow) {
                    int selectedRow = dataGridRow->GetTableRelativeIndex();
                    spdlog::debug("Selected game row: {}", selectedRow);
                    
                    // Convert page-relative row index to actual game index
                    int actualGameIndex = (dataSource->GetGamesCurrentPage() - 1) * dataSource->GetGamesPageSize() + selectedRow;
                    spdlog::debug("Converted to actual game index: {}", actualGameIndex);
                    
                    // Clear previous selection in games grid
                    auto gamesGrid = dialogDocument->GetElementById("games_grid");
                    if (gamesGrid) {
                        clearGridSelection(gamesGrid);
                    }
                    
                    // Add selected class to clicked row
                    dataGridRow->SetClass("selected", true);
                    
                    dataSource->SelectGame(actualGameIndex);
                } else {
                    spdlog::debug("Failed to cast to ElementDataGridRow");
                }
            } else {
                spdlog::debug("No datagrid row element found");
            }
        } else {
            spdlog::debug("No target element found");
        }
        repaintNeeded = true;
    }
    else if (value == "open_file") {
        spdlog::debug("Open file button clicked");
        std::string filePath = dataSource->GetSelectedFilePath();
        if (!filePath.empty()) {
            spdlog::debug("Loading SGF file: {}", filePath);
            // Get the game controller
            auto context = event.GetCurrentElement()->GetContext();
            auto mainDoc = context->GetDocument("game_window");
            if (mainDoc) {
                auto gameElement = dynamic_cast<ElementGame*>(mainDoc->GetElementById("game"));
                if (gameElement) {
                    GameThread& gameThread = gameElement->getGameThread();
                    
                    int gameIndex = dataSource->GetSelectedGameIndex();
                    spdlog::debug("Loading SGF file: {} with game index: {}", filePath, gameIndex);
                    if (gameThread.loadSGF(filePath, gameIndex)) {
                        spdlog::info("Successfully loaded SGF file: {} (game index: {})", filePath, gameIndex);
                        hideDialog();
                    } else {
                        spdlog::error("Failed to load SGF file: {} (game index: {})", filePath, gameIndex);
                    }
                }
            }
        } else {
            spdlog::warn("No file selected");
        }
        repaintNeeded = true;
    }
    else if (value == "files_prev_page") {
        spdlog::debug("Files previous page button clicked");
        if (dataSource->CanGoToFilesPrevPage()) {
            dataSource->SetFilesPage(dataSource->GetFilesCurrentPage() - 1);
            updatePaginationInfo();
        }
        repaintNeeded = true;
    }
    else if (value == "files_next_page") {
        spdlog::debug("Files next page button clicked");
        if (dataSource->CanGoToFilesNextPage()) {
            dataSource->SetFilesPage(dataSource->GetFilesCurrentPage() + 1);
            updatePaginationInfo();
        }
        repaintNeeded = true;
    }
    else if (value == "games_prev_page") {
        spdlog::debug("Games previous page button clicked");
        if (dataSource->CanGoToGamesPrevPage()) {
            dataSource->SetGamesPage(dataSource->GetGamesCurrentPage() - 1);
            updatePaginationInfo();
        }
        repaintNeeded = true;
    }
    else if (value == "games_next_page") {
        spdlog::debug("Games next page button clicked");
        if (dataSource->CanGoToGamesNextPage()) {
            dataSource->SetGamesPage(dataSource->GetGamesCurrentPage() + 1);
            updatePaginationInfo();
        }
        repaintNeeded = true;
    }
    else {
        spdlog::debug("Unknown event value: '{}'", value.CString());
    }

    if (value == "mouse_moved" || repaintNeeded) {
        requestRepaint();
    }

    event.StopPropagation();
}

void EventHandlerFileChooser::requestRepaint() {
    auto mainDoc = dialogDocument->GetContext()->GetDocument("game_window");
    if (mainDoc) {
        auto gameElement = dynamic_cast<ElementGame*>(mainDoc->GetElementById("game"));
        gameElement->requestRepaint();
    }
}

// Instance methods for dialog lifecycle management
void EventHandlerFileChooser::LoadDialog(Rocket::Core::Context* context) {
    spdlog::debug("LoadDialog() called");
    if (!context) {
        spdlog::error("Context is null in LoadDialog()");
        return;
    }
    
    if (dialogDocument) {
        spdlog::debug("Dialog already loaded");
        return;
    }
    
    // Create data source
    dataSource = new FileChooserDataSource();
    
    // Load the dialog document and keep it hidden initially
    spdlog::debug("Loading dialog document: data/gui/open.rml");
    dialogDocument = context->LoadDocument("data/gui/open.rml");
    if (dialogDocument) {
        spdlog::debug("Dialog document loaded successfully, ID: {}", dialogDocument->GetId().CString());
        dialogDocument->Hide(); // Keep it hidden initially
    } else {
        spdlog::error("Failed to load dialog document");
        // Clean up data source if dialog loading failed
        delete dataSource;
        dataSource = nullptr;
    }
}

void EventHandlerFileChooser::UnloadDialog(Rocket::Core::Context* context) {
    spdlog::debug("UnloadDialog() called");
    if (!context) {
        spdlog::error("Context is null in UnloadDialog()");
        return;
    }
    
    if (dialogDocument) {
        spdlog::debug("Unloading dialog document");
        // Clear focus from any elements in the dialog before unloading
        auto focusedElement = context->GetFocusElement();
        if (focusedElement && focusedElement->GetOwnerDocument() == dialogDocument) {
            spdlog::debug("Clearing focus from dialog element");
            focusedElement->Blur();
        }
        
        // Explicitly clear all event handlers and references from the document
        spdlog::debug("Clearing all event handlers from dialog document");
        //dialogDocument->GetEventDispatcher()->DetachAllEvents();
        
        context->UnloadDocument(dialogDocument);
        dialogDocument->RemoveReference();
        dialogDocument = nullptr;
        spdlog::debug("Dialog document unloaded");
        
        // Force immediate context update to finalize document cleanup
        spdlog::debug("Forcing context update to finalize document cleanup");
        context->Update();
    } else {
        spdlog::debug("No dialog document to unload");
    }
    
    // Clean up data source
    if (dataSource) {
        delete dataSource;
        dataSource = nullptr;
        spdlog::debug("Data source cleaned up");
    }
}

void EventHandlerFileChooser::showDialog() {
    spdlog::debug("showDialog() called");
    if (!dialogDocument) {
        spdlog::error("Dialog document not loaded in showDialog()");
        return;
    }
    
    spdlog::debug("Showing dialog document");
    dialogDocument->Show();
    updateCurrentPath();
}

void EventHandlerFileChooser::hideDialog() {
    spdlog::debug("hideDialog() called");
    if (!dialogDocument) {
        spdlog::error("Dialog document not loaded in hideDialog()");
        return;
    }
    
    spdlog::debug("Hiding dialog document");
    // Clear focus from any elements in the dialog before hiding
    auto context = dialogDocument->GetContext();
    auto focusedElement = context->GetFocusElement();
    if (focusedElement && focusedElement->GetOwnerDocument() == dialogDocument) {
        spdlog::debug("Clearing focus from dialog element");
        focusedElement->Blur();
    }
    
    dialogDocument->Hide();
    
    // Restore focus to the main game element to ensure keyboard shortcuts work
    auto mainDoc = context->GetDocument("game_window");
    if (mainDoc) {
        auto gameElement = mainDoc->GetElementById("game");
        if (gameElement) {
            spdlog::debug("Restoring focus to main game element");
            gameElement->Focus();
        } else {
            spdlog::debug("Game element not found, focusing on main document");
            mainDoc->Focus();
        }
    }
}

void EventHandlerFileChooser::updateCurrentPath() {
    spdlog::debug("updateCurrentPath() called");
    if (!dialogDocument) {
        spdlog::debug("No dialog document in updateCurrentPath()");
        return;
    }
    
    auto pathDisplay = dialogDocument->GetElementById("current_path");
    if (pathDisplay) {
        if (dataSource) {
            std::string currentPath = dataSource->GetCurrentPath().string();
            spdlog::debug("Updating path display to: {}", currentPath);
            pathDisplay->SetInnerRML(currentPath.c_str());
        } else {
            spdlog::error("DataSource is null in updateCurrentPath()");
        }
    } else {
        spdlog::debug("current_path element not found in dialog");
    }
    
    // Also update pagination info when path changes
    updatePaginationInfo();
}

void EventHandlerFileChooser::updatePaginationInfo() {
    spdlog::debug("updatePaginationInfo() called");
    if (!dialogDocument || !dataSource) {
        spdlog::debug("No dialog document or data source in updatePaginationInfo()");
        return;
    }
    
    // Update files pagination info
    auto filesPageInfo = dialogDocument->GetElementById("files_page_info");
    if (filesPageInfo) {
        int currentPage = dataSource->GetFilesCurrentPage();
        int totalPages = dataSource->GetFilesTotalPages();
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "Page %d of %d", currentPage, totalPages);
        filesPageInfo->SetInnerRML(buffer);
        spdlog::debug("Updated files pagination info: {}", buffer);
    }
    
    // Update files pagination buttons
    auto filesPrev = dialogDocument->GetElementById("files_prev");
    auto filesNext = dialogDocument->GetElementById("files_next");
    if (filesPrev) {
        filesPrev->SetClass("disabled", !dataSource->CanGoToFilesPrevPage());
    }
    if (filesNext) {
        filesNext->SetClass("disabled", !dataSource->CanGoToFilesNextPage());
    }
    
    // Update games pagination info
    auto gamesPageInfo = dialogDocument->GetElementById("games_page_info");
    if (gamesPageInfo) {
        int currentPage = dataSource->GetGamesCurrentPage();
        int totalPages = dataSource->GetGamesTotalPages();
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "Page %d of %d", currentPage, totalPages);
        gamesPageInfo->SetInnerRML(buffer);
        spdlog::debug("Updated games pagination info: {}", buffer);
    }
    
    // Update games pagination buttons
    auto gamesPrev = dialogDocument->GetElementById("games_prev");
    auto gamesNext = dialogDocument->GetElementById("games_next");
    if (gamesPrev) {
        gamesPrev->SetClass("disabled", !dataSource->CanGoToGamesPrevPage());
    }
    if (gamesNext) {
        gamesNext->SetClass("disabled", !dataSource->CanGoToGamesNextPage());
    }
}

void EventHandlerFileChooser::clearGridSelection(Rocket::Core::Element* grid) {
    if (!grid) return;
    
    // Find all datagridrow elements in the grid and remove selected class
    Rocket::Core::ElementList rows;
    grid->GetElementsByTagName(rows, Rocket::Core::String("datagridrow"));
    for (int i = 0; i < rows.size(); ++i) {
        auto row = rows[i];
        if (row->IsClassSet("selected")) {
            row->SetClass("selected", false);
        }
    }

}