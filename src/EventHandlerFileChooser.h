#ifndef GOBAN_EVENTHANDLERFILECHOOSER_H
#define GOBAN_EVENTHANDLERFILECHOOSER_H

#include "EventHandler.h"
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/EventListener.h>
#include "spdlog/spdlog.h"

class FileChooserDataSource;

// Keyboard listener for dialog (ESC to close)
class DialogKeyListener : public Rml::EventListener {
public:
    DialogKeyListener(class EventHandlerFileChooser* handler) : handler(handler) {}
    void ProcessEvent(Rml::Event& event) override;
private:
    class EventHandlerFileChooser* handler;
};

class EventHandlerFileChooser : public EventHandler {
public:
    EventHandlerFileChooser();
    virtual ~EventHandlerFileChooser();

    void ProcessEvent(Rml::Event& event, const Rml::String& value) override;

    // Instance methods for managing the dialog document
    void LoadDialog(Rml::Context* context);
    void UnloadDialog(Rml::Context* context);

    // Public method to show the dialog (called from GobanControl)
    void ShowDialog();

    // Public method to hide the dialog (called from DialogKeyListener)
    void HideDialog();

private:
    void populateFilesList();
    void populateGamesList();
    void handleFileSelection(int index);
    void handleGameSelection(int index);
    void updateCurrentPath();
    void updatePaginationInfo();
    void clearGridSelection(Rml::Element* grid);
    void requestRepaint();
    std::string getTemplateString(const char* templateId, const char* defaultValue);
    void initializeLocalization();

    Rml::ElementDocument* dialogDocument;
    FileChooserDataSource* dataSource;
    DialogKeyListener* keyListener;

    // Localized strings
    std::string strPageInfoFmt = "Page %d of %d";
};

#endif //GOBAN_EVENTHANDLERFILECHOOSER_H
