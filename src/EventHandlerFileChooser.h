#ifndef GOBAN_EVENTHANDLERFILECHOOSER_H
#define GOBAN_EVENTHANDLERFILECHOOSER_H

#include "EventHandler.h"
#include <RmlUi/Core/ElementDocument.h>
#include "spdlog/spdlog.h"

class FileChooserDataSource;

class EventHandlerFileChooser : public EventHandler {
public:
    EventHandlerFileChooser();
    virtual ~EventHandlerFileChooser();

    void ProcessEvent(Rml::Event& event, const Rml::String& value) override;

    // Instance methods for managing the dialog document
    void LoadDialog(Rml::Context* context);
    void UnloadDialog(Rml::Context* context);

private:
    void showDialog();
    void hideDialog();
    void updateCurrentPath();
    void updatePaginationInfo();
    void clearGridSelection(Rml::Element* grid);
    void requestRepaint();
    std::string getTemplateString(const char* templateId, const char* defaultValue);
    void initializeLocalization();

    Rml::ElementDocument* dialogDocument;
    FileChooserDataSource* dataSource;

    // Localized strings
    std::string strPageInfoFmt = "Page %d of %d";
};

#endif //GOBAN_EVENTHANDLERFILECHOOSER_H