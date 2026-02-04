#ifndef GOBAN_EVENTHANDLERFILECHOOSER_H
#define GOBAN_EVENTHANDLERFILECHOOSER_H

#include "EventHandler.h"
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/EventListener.h>
#include "spdlog/spdlog.h"
#include <vector>
#include <map>

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
    ~EventHandlerFileChooser() override;

    void ProcessEvent(Rml::Event& event, const Rml::String& value) override;

    // Instance methods for managing the dialog document
    void LoadDialog(Rml::Context* context);
    void UnloadDialog(Rml::Context* context);

    // Public method to show the dialog (called from GobanControl)
    void ShowDialog(const std::string& currentFile = "", int currentGameIndex = -1);

    // Public method to hide the dialog (called from DialogKeyListener)
    void HideDialog() const;

private:
    void populateFilesList() const;
    void populateGamesList();
    void handleFileSelection(int index);
    void handleGameSelection(int index) const;
    void updateCurrentPath() const;
    void updatePaginationInfo() const;

    static void clearGridSelection(const Rml::Element* grid);
    void requestRepaint() const;
    void setTsumegoToggle(bool enabled) const;
    bool isTsumegoToggled() const;
    std::string getTemplateString(const char* templateId, const char* defaultValue) const;
    void initializeLocalization();

    Rml::ElementDocument* dialogDocument;
    FileChooserDataSource* dataSource;
    DialogKeyListener* keyListener;

    // Board's current file/game (for re-selecting when navigating back to the file)
    std::string boardFile;
    int boardGameIndex = -1;

    // Localized strings
    std::string strPageInfoFmt = "Page %d of %d";

    // Configurable game columns
    std::vector<std::string> gameColumns;
    std::map<std::string, std::string> columnHeaders;
    void loadGameColumnsConfig();
    void createGameHeaderRow(Rml::Element* gamesList);

    static Rml::ElementPtr createColumnSpan(Rml::Element* parent, const std::string& colType, const std::string& text);
};

#endif //GOBAN_EVENTHANDLERFILECHOOSER_H
