#ifndef GOBAN_EVENTHANDLERFILECHOOSER_H
#define GOBAN_EVENTHANDLERFILECHOOSER_H

#include "EventHandler.h"
#include <Rocket/Core/ElementDocument.h>
#include "spdlog/spdlog.h"

class FileChooserDataSource;

class EventHandlerFileChooser : public EventHandler {
public:
    EventHandlerFileChooser();
    virtual ~EventHandlerFileChooser();

    virtual void ProcessEvent(Rocket::Core::Event& event, const Rocket::Core::String& value);
    
    // Instance methods for managing the dialog document
    void LoadDialog(Rocket::Core::Context* context);
    void UnloadDialog(Rocket::Core::Context* context);

private:
    void showDialog();
    void hideDialog();
    void updateCurrentPath();
    void updatePaginationInfo();
    void clearGridSelection(Rocket::Core::Element* grid);
    void requestRepaint();
    std::string getTemplateString(const char* templateId, const char* defaultValue);
    void initializeLocalization();

    Rocket::Core::ElementDocument* dialogDocument;
    FileChooserDataSource* dataSource;

    // Localized strings
    std::string strPageInfoFmt = "Page %d of %d";
};

#endif //GOBAN_EVENTHANDLERFILECHOOSER_H