#ifndef CORE85_MAINWINDOW_H
#define CORE85_MAINWINDOW_H

#include <QHash>
#include <QMainWindow>
#include <QSet>
#include <QTextDocument>
#include <QThread>

#include "core/assembler.h"
#include "gui/emulator_types.h"

class QAction;
class QActionGroup;
class QComboBox;
class QDockWidget;
class QFileSystemModel;
class QLabel;
class QMenu;
class QModelIndex;
class QPlainTextEdit;
class QStackedWidget;
class QTabWidget;
class QToolBar;
class QTreeView;

namespace Core85::Gui {
class CodeEditor;
class EmulatorController;
class FindReplaceBar;
class IOPanel;
class MemoryViewer;
class ProblemsPanel;
class RegisterViewer;
class WelcomePage;
}  // namespace Core85::Gui

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void newAssemblyFile();
    void assembleAndLoad();
    void openSourceFile();
    void openWorkspaceFolder();
    void openSampleProgram();
    void openRecentFile();
    void openRecentFolder();
    void saveSourceFile();
    void saveAllFiles();
    void saveSourceFileAs();
    void loadHexFile();
    void handleSnapshot(const Core85::Gui::EmulatorSnapshot& snapshot);
    void handleExecutionStateChanged(Core85::Gui::ExecutionState state);
    void handleEmulatorError(const QString& message);
    void handleBreakpointToggled(int line, bool enabled);
    void handleRegisterEdit(const QString& field, quint16 value);
    void handleMemoryEdit(quint16 address, quint8 value);
    void handleInvalidRegisterInput(const QPoint& globalPos, const QString& message);
    void handleInvalidMemoryAddress(const QString& message);
    void handleProjectMetadataChanged(const Core85::Gui::ProjectMetadata& metadata);
    void handleExplorerActivated(const QModelIndex& index);
    void handleCurrentTabChanged(int index);
    void handleTabCloseRequested(int index);
    void handleEditorModificationChanged(bool modified);
    void handleProblemActivated(int line);
    void showFindBar();
    void showReplaceBar();
    void hideFindBar();
    void findNext(const QString& text, bool caseSensitive, bool wholeWord);
    void findPrevious(const QString& text, bool caseSensitive, bool wholeWord);
    void replaceOne(const QString& findText,
                    const QString& replaceText,
                    bool caseSensitive,
                    bool wholeWord);
    void replaceAll(const QString& findText,
                    const QString& replaceText,
                    bool caseSensitive,
                    bool wholeWord);
    void goToLine(int line);
    void showTabContextMenu(const QPoint& pos);
    void duplicateCurrentTab();
    void closeOtherTabs();
    void revealCurrentFileInExplorer();
    void applyLightTheme();
    void applyDarkTheme();
    void showKeyboardShortcuts();

private:
    void buildUi();
    void createMenus();
    void createToolbar();
    void createDocks();
    void wireController();
    void refreshStatusBar();
    void updateExecutionActions();
    void clearAssemblerErrors();
    void clearExecutionLineHighlights();
    void showAssemblerErrors(const std::vector<Core85::AssemblerError>& errors);
    void rebuildSourceMappings(const std::vector<Core85::SourceMappingEntry>& sourceMap);
    void syncBreakpointAddresses();
    void requestControllerSnapshot();
    void setCurrentFilePath(const QString& path);
    void updateWindowTitle();
    void refreshActionIcons();
    void setWelcomeVisible(bool visible);
    bool saveSourceToPath(const QString& path);
    bool saveEditorToPath(Core85::Gui::CodeEditor* editor, const QString& path);
    bool saveEditorAs(Core85::Gui::CodeEditor* editor);
    bool maybeSaveEditor(Core85::Gui::CodeEditor* editor);
    bool maybeSaveAllEditors();
    void openTextFileInTab(const QString& path, bool activate = true);
    void createEditorTab(Core85::Gui::CodeEditor* editor, bool activate = true);
    void loadHexFromPath(const QString& path);
    void setWorkspaceRoot(const QString& path);
    void createUntitledTab(const QString& content = QString());
    int findEditorTabByPath(const QString& path) const;
    Core85::Gui::CodeEditor* currentEditor() const;
    Core85::Gui::CodeEditor* editorAt(int index) const;
    QString editorFilePath(const Core85::Gui::CodeEditor* editor) const;
    void setEditorFilePath(Core85::Gui::CodeEditor* editor, const QString& path);
    QString currentEditorFilePath() const;
    QString editorDisplayName(const Core85::Gui::CodeEditor* editor) const;
    void updateTabTitle(Core85::Gui::CodeEditor* editor);
    QString projectMetadataPathForFile(const QString& filePath) const;
    void loadProjectMetadataForFile(const QString& filePath);
    void saveProjectMetadataForFile(const QString& filePath) const;
    void applyTheme(const QString& themeName);
    void loadSettings();
    void saveSettings() const;
    void recordRecentFile(const QString& path);
    void recordRecentFolder(const QString& path);
    void refreshRecentMenus();
    QStringList openTabFilePaths() const;
    QSet<QString> diffRegisterFields(const Core85::CPUState& previous,
                                     const Core85::CPUState& current) const;
    QSet<quint16> diffMemory(const QByteArray& previous, const QByteArray& current) const;
    QList<quint16> currentBreakpointAddresses() const;
    QTextDocument::FindFlags buildFindFlags(bool backward, bool caseSensitive, bool wholeWord) const;

    Core85::Assembler assembler_{};
    QThread emulatorThread_{};
    Core85::Gui::EmulatorController* emulatorController_ = nullptr;
    Core85::Gui::CodeEditor* loadedEditor_ = nullptr;
    Core85::Gui::RegisterViewer* registerViewer_ = nullptr;
    Core85::Gui::MemoryViewer* memoryViewer_ = nullptr;
    Core85::Gui::IOPanel* ioPanel_ = nullptr;
    Core85::Gui::ProblemsPanel* problemsPanel_ = nullptr;
    Core85::Gui::FindReplaceBar* findReplaceBar_ = nullptr;
    Core85::Gui::WelcomePage* welcomePage_ = nullptr;
    QPlainTextEdit* errorConsole_ = nullptr;
    QFileSystemModel* fileSystemModel_ = nullptr;
    QStackedWidget* centerStack_ = nullptr;
    QStackedWidget* explorerStack_ = nullptr;
    QTreeView* explorerTree_ = nullptr;
    QTabWidget* editorTabs_ = nullptr;
    QMenu* viewMenu_ = nullptr;
    QMenu* recentFilesMenu_ = nullptr;
    QMenu* recentFoldersMenu_ = nullptr;
    QActionGroup* themeGroup_ = nullptr;
    QToolBar* executionToolBar_ = nullptr;
    QDockWidget* explorerDock_ = nullptr;
    QDockWidget* registersDock_ = nullptr;
    QDockWidget* memoryDock_ = nullptr;
    QDockWidget* ioDock_ = nullptr;
    QDockWidget* errorDock_ = nullptr;
    QAction* assembleAction_ = nullptr;
    QAction* runAction_ = nullptr;
    QAction* pauseAction_ = nullptr;
    QAction* stepIntoAction_ = nullptr;
    QAction* stepOverAction_ = nullptr;
    QAction* resetAction_ = nullptr;
    QAction* autoFollowAction_ = nullptr;
    QAction* openFolderAction_ = nullptr;
    QAction* saveAllAction_ = nullptr;
    QAction* lightThemeAction_ = nullptr;
    QAction* darkThemeAction_ = nullptr;
    QComboBox* speedCombo_ = nullptr;
    QLabel* pcStatusLabel_ = nullptr;
    QLabel* cyclesStatusLabel_ = nullptr;
    QLabel* stateStatusLabel_ = nullptr;
    QLabel* messageStatusLabel_ = nullptr;
    Core85::Gui::ExecutionState executionState_ = Core85::Gui::ExecutionState::Paused;
    Core85::Gui::ProjectMetadata projectMetadata_{};
    Core85::Gui::EmulatorSnapshot lastSnapshot_{};
    bool hasSnapshot_ = false;
    bool hasLoadedProgram_ = false;
    QString currentFilePath_{};
    QString currentTheme_{QStringLiteral("dark")};
    QString workspaceRootPath_{};
    QStringList recentFiles_{};
    QStringList recentFolders_{};
    QHash<quint16, int> addressToLine_{};
    QHash<int, quint16> lineToAddress_{};
    int untitledCounter_ = 1;
};

#endif
