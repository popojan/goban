#ifndef GOBAN_EVENTHANDLERFILECHOOSER_H
#define GOBAN_EVENTHANDLERFILECHOOSER_H

#include "EventHandler.h"
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/EventListener.h>
#include "spdlog/spdlog.h"
#include <vector>
#include <map>
#include <string>

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

    // --- Scripting seam ------------------------------------------------------
    // The dialog is where a good share of this project's bugs have lived, and
    // none of it was reachable from a scenario: every action was bound to an
    // RmlUi element. These are the same operations the widgets invoke, exposed
    // so `chooser_*` commands can drive the real dialog rather than a stub. The
    // accessors read the data source, not the widgets, so they report what the
    // dialog *is* rather than what it has managed to render.
    bool OpenSelected();                       ///< The Open button.
    bool IsDialogVisible() const;
    void SetPath(const std::string& path);
    void NavigateUp();
    bool SelectFileByName(const std::string& name);
    bool SelectGameByIndex(int index);
    bool StepFilesPage(int delta);
    bool StepGamesPage(int delta);
    int GetFileCount() const;
    int GetGameCount() const;
    std::string GetCurrentPath() const;
    std::string GetSelectedFileName() const;
    int GetSelectedGameIndex() const;
    bool IsTsumegoSelected() const;
    void SetTsumegoSelected(bool enabled);

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
