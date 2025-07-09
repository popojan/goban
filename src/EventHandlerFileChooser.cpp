#include "EventHandlerFileChooser.h"
#include "ElementGame.h"
#include "FileChooserDataSource.h"
#include <Rocket/Core/Context.h>
#include <Rocket/Core/ElementDocument.h>
#include <Rocket/Core/Event.h>
#include <Rocket/Controls/ElementDataGrid.h>
#include <Rocket/Controls/ElementDataGridRow.h>

// Remove static context dependency - context will be passed as parameter

EventHandlerFileChooser::EventHandlerFileChooser() : dialogDocument(nullptr) {
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
}

void EventHandlerFileChooser::ProcessEvent(Rocket::Core::Event& event, const Rocket::Core::String& value) {
    spdlog::debug("EventHandlerFileChooser::ProcessEvent called with value: '{}'", value.CString());
    spdlog::debug("Event type: '{}', target element: '{}'", event.GetType().CString(), 
                  event.GetTargetElement() ? event.GetTargetElement()->GetTagName().CString() : "null");
    
    auto dataSource = FileChooserDataSource::GetInstance();
    if (!dataSource) {
        spdlog::error("FileChooserDataSource not initialized");
        return;
    }
    
    if (value == "load") {
        spdlog::debug("Showing file chooser dialog");
        showDialog();
    }
    else if (value == "cancel") {
        spdlog::debug("Cancel button clicked");
        hideDialog();
    }
    else if (value == "refresh") {
        spdlog::debug("Refresh button clicked");
        dataSource->RefreshFiles();
        updateCurrentPath();
    }
    else if (value == "navigate_up") {
        spdlog::debug("Navigate up button clicked");
        dataSource->NavigateUp();
        updateCurrentPath();
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
                    dataSource->SelectFile(selectedRow);
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
                    dataSource->SelectGame(selectedRow);
                } else {
                    spdlog::debug("Failed to cast to ElementDataGridRow");
                }
            } else {
                spdlog::debug("No datagrid row element found");
            }
        } else {
            spdlog::debug("No target element found");
        }
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
                    
                    if (gameThread.loadSGF(filePath)) {
                        spdlog::info("Successfully loaded SGF file: {}", filePath);
                        hideDialog();
                    } else {
                        spdlog::error("Failed to load SGF file: {}", filePath);
                    }
                }
            }
        } else {
            spdlog::warn("No file selected");
        }
    }
    else {
        spdlog::debug("Unknown event value: '{}'", value.CString());
    }
    
    event.StopPropagation();
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
    
    // Load the dialog document and keep it hidden initially
    spdlog::debug("Loading dialog document: data/gui/open.rml");
    dialogDocument = context->LoadDocument("data/gui/open.rml");
    if (dialogDocument) {
        spdlog::debug("Dialog document loaded successfully, ID: {}", dialogDocument->GetId().CString());
        dialogDocument->Hide(); // Keep it hidden initially
    } else {
        spdlog::error("Failed to load dialog document");
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
        
        context->UnloadDocument(dialogDocument);
        dialogDocument = nullptr;
        spdlog::debug("Dialog document unloaded");
    } else {
        spdlog::debug("No dialog document to unload");
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
}

void EventHandlerFileChooser::updateCurrentPath() {
    spdlog::debug("updateCurrentPath() called");
    if (!dialogDocument) {
        spdlog::debug("No dialog document in updateCurrentPath()");
        return;
    }
    
    auto pathDisplay = dialogDocument->GetElementById("current_path");
    if (pathDisplay) {
        auto dataSource = FileChooserDataSource::GetInstance();
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
}