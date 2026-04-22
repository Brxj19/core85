#ifndef CORE85_MAINWINDOW_H
#define CORE85_MAINWINDOW_H

#include <QHash>
#include <QMainWindow>
#include <QSet>
#include <QThread>

#include "core/assembler.h"
#include "gui/emulator_types.h"

class QAction;
class QComboBox;
class QDockWidget;
class QLabel;
class QMenu;
class QPlainTextEdit;
class QToolBar;

namespace Core85::Gui {
class CodeEditor;
class EmulatorController;
class IOPanel;
class MemoryViewer;
class RegisterViewer;
}  // namespace Core85::Gui

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void assembleAndLoad();
    void openSourceFile();
    void saveSourceFile();
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
    void applyLightTheme();
    void applyDarkTheme();

private:
    void buildUi();
    void createMenus();
    void createToolbar();
    void createDocks();
    void wireController();
    void refreshStatusBar();
    void clearAssemblerErrors();
    void showAssemblerErrors(const std::vector<Core85::AssemblerError>& errors);
    void rebuildSourceMappings(const std::vector<Core85::SourceMappingEntry>& sourceMap);
    void syncBreakpointAddresses();
    void requestControllerSnapshot();
    void setCurrentFilePath(const QString& path);
    bool saveSourceToPath(const QString& path);
    void loadSourceFromPath(const QString& path);
    void loadHexFromPath(const QString& path);
    QString projectMetadataPath() const;
    void loadProjectMetadata();
    void saveProjectMetadata() const;
    void applyTheme(const QString& themeName);
    void loadSettings();
    void saveSettings() const;
    QSet<QString> diffRegisterFields(const Core85::CPUState& previous,
                                     const Core85::CPUState& current) const;
    QSet<quint16> diffMemory(const QByteArray& previous, const QByteArray& current) const;
    QList<quint16> currentBreakpointAddresses() const;

    Core85::Assembler assembler_{};
    QThread emulatorThread_{};
    Core85::Gui::EmulatorController* emulatorController_ = nullptr;
    Core85::Gui::CodeEditor* codeEditor_ = nullptr;
    Core85::Gui::RegisterViewer* registerViewer_ = nullptr;
    Core85::Gui::MemoryViewer* memoryViewer_ = nullptr;
    Core85::Gui::IOPanel* ioPanel_ = nullptr;
    QPlainTextEdit* errorConsole_ = nullptr;
    QMenu* viewMenu_ = nullptr;
    QToolBar* executionToolBar_ = nullptr;
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
    QComboBox* speedCombo_ = nullptr;
    QLabel* pcStatusLabel_ = nullptr;
    QLabel* cyclesStatusLabel_ = nullptr;
    QLabel* stateStatusLabel_ = nullptr;
    QLabel* messageStatusLabel_ = nullptr;
    Core85::Gui::ExecutionState executionState_ = Core85::Gui::ExecutionState::Paused;
    Core85::Gui::ProjectMetadata projectMetadata_{};
    Core85::Gui::EmulatorSnapshot lastSnapshot_{};
    bool hasSnapshot_ = false;
    QString currentFilePath_{};
    QString currentTheme_{QStringLiteral("dark")};
    QHash<quint16, int> addressToLine_{};
    QHash<int, quint16> lineToAddress_{};
};

#endif
