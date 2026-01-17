#include "EventHandlerFileChooser.h"
#include "ElementGame.h"
#include "EventManager.h"
#include "FileChooserDataSource.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>

// Note: DataGrid/DataSource was removed from RmlUi
// File chooser functionality is temporarily disabled

EventHandlerFileChooser::EventHandlerFileChooser() : dialogDocument(nullptr), dataSource(nullptr) {
    spdlog::warn("File chooser is temporarily disabled - DataGrid not available in RmlUi");
}

EventHandlerFileChooser::~EventHandlerFileChooser() {
    if (dataSource) {
        delete dataSource;
        dataSource = nullptr;
    }
}

void EventHandlerFileChooser::ProcessEvent(Rml::Event& event, const Rml::String& value) {
    spdlog::debug("EventHandlerFileChooser::ProcessEvent - disabled, value: '{}'", value.c_str());

    if (value == "load") {
        spdlog::warn("File chooser dialog is disabled - DataGrid not available in RmlUi");
    }

    event.StopPropagation();
}

void EventHandlerFileChooser::LoadDialog(Rml::Context* context) {
    spdlog::debug("LoadDialog() - file chooser disabled");
    (void)context;
}

void EventHandlerFileChooser::UnloadDialog(Rml::Context* context) {
    spdlog::debug("UnloadDialog() - file chooser disabled");
    (void)context;
}

void EventHandlerFileChooser::showDialog() {
    spdlog::debug("showDialog() - file chooser disabled");
}

void EventHandlerFileChooser::hideDialog() {
    spdlog::debug("hideDialog() - file chooser disabled");
}

void EventHandlerFileChooser::updateCurrentPath() {
    // Disabled
}

void EventHandlerFileChooser::updatePaginationInfo() {
    // Disabled
}

void EventHandlerFileChooser::clearGridSelection(Rml::Element* grid) {
    (void)grid;
    // Disabled
}

void EventHandlerFileChooser::requestRepaint() {
    // Disabled
}

std::string EventHandlerFileChooser::getTemplateString(const char* templateId, const char* defaultValue) {
    (void)templateId;
    return defaultValue;
}

void EventHandlerFileChooser::initializeLocalization() {
    // Disabled
}
