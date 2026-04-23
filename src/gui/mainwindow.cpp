#include "mainwindow.h"

#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextStream>
#include <QThread>
#include <QToolBar>
#include <QToolTip>
#include <QTreeView>
#include <QVBoxLayout>

#include "gui/codeeditor.h"
#include "gui/emulatorcontroller.h"
#include "gui/findreplacebar.h"
#include "gui/iopanel.h"
#include "gui/memoryviewer.h"
#include "gui/problemspanel.h"
#include "gui/registerviewer.h"
#include "gui/value_utils.h"
#include "gui/welcomepage.h"

namespace {

enum class TransportIconKind {
    Run,
    Pause,
    StepInto,
    StepOver,
};

QString fileDialogRoot(const QString& workspaceRoot, const QString& currentPath) {
    if (!currentPath.isEmpty()) {
        return QFileInfo(currentPath).absolutePath();
    }
    if (!workspaceRoot.isEmpty()) {
        return workspaceRoot;
    }
    return QDir::currentPath();
}

QIcon makeTransportIcon(TransportIconKind kind, const QColor& color) {
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);

    switch (kind) {
        case TransportIconKind::Run:
            painter.drawPolygon(
                QPolygonF{QPointF(7.0, 5.0), QPointF(19.0, 12.0), QPointF(7.0, 19.0)});
            break;
        case TransportIconKind::Pause:
            painter.drawRoundedRect(QRectF(6.0, 5.0, 4.5, 14.0), 1.5, 1.5);
            painter.drawRoundedRect(QRectF(13.5, 5.0, 4.5, 14.0), 1.5, 1.5);
            break;
        case TransportIconKind::StepInto:
            painter.drawRoundedRect(QRectF(5.0, 4.0, 2.5, 16.0), 1.0, 1.0);
            painter.drawPolygon(
                QPolygonF{QPointF(10.0, 5.0), QPointF(19.0, 12.0), QPointF(10.0, 19.0)});
            break;
        case TransportIconKind::StepOver:
            painter.drawPolygon(
                QPolygonF{QPointF(5.0, 5.0), QPointF(14.0, 12.0), QPointF(5.0, 19.0)});
            painter.drawRoundedRect(QRectF(16.5, 4.0, 2.5, 16.0), 1.0, 1.0);
            break;
    }

    return QIcon(pixmap);
}

QString sampleProgramText() {
    return QStringLiteral(
        "; Sample: mirror switches to LEDs\n"
        ".ORG 0000H\n\n"
        "START:  IN 00H\n"
        "        OUT 01H\n"
        "        JMP START\n\n"
        "END\n");
}

}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    resize(1520, 920);

    buildUi();
    wireController();
    loadSettings();
    refreshRecentMenus();
    setWelcomeVisible(editorTabs_->count() == 0);
    refreshStatusBar();
    requestControllerSnapshot();
}

MainWindow::~MainWindow() {
    saveSettings();
    emulatorThread_.quit();
    emulatorThread_.wait();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (!maybeSaveAllEditors()) {
        event->ignore();
        return;
    }

    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::newAssemblyFile() {
    createUntitledTab();
}

void MainWindow::assembleAndLoad() {
    auto* editor = currentEditor();
    if (editor == nullptr) {
        messageStatusLabel_->setText(QStringLiteral("Open or create an assembly file first."));
        return;
    }

    clearAssemblerErrors();
    const Core85::AssemblyResult result =
        assembler_.assemble(editor->toPlainText().toStdString());

    if (!result.success) {
        showAssemblerErrors(result.errors);
        return;
    }

    QByteArray programBytes;
    programBytes.resize(static_cast<int>(result.bytes.size()));
    for (std::size_t index = 0; index < result.bytes.size(); ++index) {
        programBytes[static_cast<int>(index)] = static_cast<char>(result.bytes[index]);
    }

    loadedEditor_ = editor;
    hasLoadedProgram_ = true;
    rebuildSourceMappings(result.sourceMap);
    syncBreakpointAddresses();
    clearExecutionLineHighlights();

    QMetaObject::invokeMethod(
        emulatorController_,
        [controller = emulatorController_, origin = result.origin, programBytes]() {
            controller->loadProgram(origin, programBytes);
        },
        Qt::QueuedConnection);

    messageStatusLabel_->setText(
        QStringLiteral("Assembled %1 byte(s) at %2")
            .arg(result.bytes.size())
            .arg(Core85::Gui::formatHex16(result.origin)));
    updateExecutionActions();
}

void MainWindow::openSourceFile() {
    const QStringList paths = QFileDialog::getOpenFileNames(
        this,
        QStringLiteral("Open Source Files"),
        fileDialogRoot(workspaceRootPath_, currentEditorFilePath()),
        QStringLiteral("Assembly and Text Files (*.asm *.inc *.txt *.hex);;All Files (*)"));

    for (const QString& path : paths) {
        if (!path.isEmpty()) {
            openTextFileInTab(path);
        }
    }
}

void MainWindow::openWorkspaceFolder() {
    const QString path = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("Open Workspace Folder"),
        workspaceRootPath_.isEmpty() ? QDir::currentPath() : workspaceRootPath_);
    if (!path.isEmpty()) {
        setWorkspaceRoot(path);
    }
}

void MainWindow::openSampleProgram() {
    createUntitledTab(sampleProgramText());
    if (auto* editor = currentEditor()) {
        editor->document()->setModified(false);
        updateTabTitle(editor);
    }
    messageStatusLabel_->setText(QStringLiteral("Opened a sample program. Assemble it with Ctrl+Shift+B."));
}

void MainWindow::openRecentFile() {
    const auto* action = qobject_cast<QAction*>(sender());
    if (action == nullptr) {
        return;
    }
    const QString path = action->data().toString();
    if (!path.isEmpty() && QFileInfo::exists(path)) {
        openTextFileInTab(path);
    }
}

void MainWindow::openRecentFolder() {
    const auto* action = qobject_cast<QAction*>(sender());
    if (action == nullptr) {
        return;
    }
    const QString path = action->data().toString();
    if (!path.isEmpty() && QFileInfo(path).isDir()) {
        setWorkspaceRoot(path);
    }
}

void MainWindow::saveSourceFile() {
    auto* editor = currentEditor();
    if (editor == nullptr) {
        return;
    }

    const QString path = editorFilePath(editor);
    if (path.isEmpty()) {
        saveSourceFileAs();
        return;
    }

    saveEditorToPath(editor, path);
}

void MainWindow::saveAllFiles() {
    for (int index = 0; index < editorTabs_->count(); ++index) {
        auto* editor = editorAt(index);
        if (editor == nullptr || !editor->document()->isModified()) {
            continue;
        }

        const QString path = editorFilePath(editor);
        if (path.isEmpty()) {
            editorTabs_->setCurrentIndex(index);
            if (!saveEditorAs(editor)) {
                return;
            }
        } else if (!saveEditorToPath(editor, path)) {
            return;
        }
    }

    messageStatusLabel_->setText(QStringLiteral("All open files saved"));
}

void MainWindow::saveSourceFileAs() {
    saveEditorAs(currentEditor());
}

void MainWindow::loadHexFile() {
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Load Intel HEX"),
        fileDialogRoot(workspaceRootPath_, currentEditorFilePath()),
        QStringLiteral("Intel HEX Files (*.hex);;All Files (*)"));
    if (!path.isEmpty()) {
        loadHexFromPath(path);
    }
}

void MainWindow::handleSnapshot(const Core85::Gui::EmulatorSnapshot& snapshot) {
    const QSet<QString> changedRegisters =
        hasSnapshot_ ? diffRegisterFields(lastSnapshot_.cpuState, snapshot.cpuState) : QSet<QString>{};
    const QSet<quint16> changedMemory =
        hasSnapshot_ ? diffMemory(lastSnapshot_.memory, snapshot.memory) : QSet<quint16>{};

    lastSnapshot_ = snapshot;
    hasSnapshot_ = true;

    registerViewer_->setState(snapshot.cpuState, changedRegisters);
    registerViewer_->setInteractive(executionState_ != Core85::Gui::ExecutionState::Running);
    memoryViewer_->setMemorySnapshot(snapshot.memory, changedMemory);
    memoryViewer_->setRegisterAnchors(snapshot.cpuState.pc, snapshot.cpuState.sp, snapshot.cpuState.hl);
    memoryViewer_->setInteractive(executionState_ != Core85::Gui::ExecutionState::Running);
    ioPanel_->setPortSnapshots(snapshot.inputPorts, snapshot.outputPorts);

    clearExecutionLineHighlights();
    if (loadedEditor_ != nullptr && addressToLine_.contains(snapshot.cpuState.pc)) {
        loadedEditor_->setCurrentExecutionLine(addressToLine_.value(snapshot.cpuState.pc));
    }

    if (autoFollowAction_->isChecked()) {
        memoryViewer_->scrollToAddress(snapshot.cpuState.pc);
    }

    refreshStatusBar();
}

void MainWindow::handleExecutionStateChanged(Core85::Gui::ExecutionState state) {
    executionState_ = state;
    for (int index = 0; index < editorTabs_->count(); ++index) {
        if (auto* editor = editorAt(index)) {
            editor->setReadOnly(state == Core85::Gui::ExecutionState::Running);
        }
    }
    registerViewer_->setInteractive(state != Core85::Gui::ExecutionState::Running);
    memoryViewer_->setInteractive(state != Core85::Gui::ExecutionState::Running);
    refreshStatusBar();
}

void MainWindow::handleEmulatorError(const QString& message) {
    errorConsole_->appendPlainText(message);
    messageStatusLabel_->setText(message);
}

void MainWindow::handleBreakpointToggled(int line, bool enabled) {
    Q_UNUSED(line);
    Q_UNUSED(enabled);

    if (sender() == loadedEditor_) {
        syncBreakpointAddresses();
    }
}

void MainWindow::handleRegisterEdit(const QString& field, quint16 value) {
    QMetaObject::invokeMethod(
        emulatorController_,
        [controller = emulatorController_, field, value]() {
            controller->setRegisterValue(field, value);
        },
        Qt::QueuedConnection);
}

void MainWindow::handleMemoryEdit(quint16 address, quint8 value) {
    QMetaObject::invokeMethod(
        emulatorController_,
        [controller = emulatorController_, address, value]() {
            controller->writeMemory(address, value);
        },
        Qt::QueuedConnection);
}

void MainWindow::handleInvalidRegisterInput(const QPoint& globalPos, const QString& message) {
    QToolTip::showText(globalPos, message, registerViewer_);
    messageStatusLabel_->setText(message);
}

void MainWindow::handleInvalidMemoryAddress(const QString& message) {
    messageStatusLabel_->setText(message);
}

void MainWindow::handleProjectMetadataChanged(const Core85::Gui::ProjectMetadata& metadata) {
    projectMetadata_ = metadata;
    saveProjectMetadataForFile(currentEditorFilePath());
}

void MainWindow::handleExplorerActivated(const QModelIndex& index) {
    if (!index.isValid()) {
        return;
    }

    const QString path = fileSystemModel_->filePath(index);
    if (QFileInfo(path).isDir()) {
        explorerTree_->setRootIndex(index);
        return;
    }

    openTextFileInTab(path);
}

void MainWindow::handleCurrentTabChanged(int index) {
    auto* editor = editorAt(index);
    setCurrentFilePath(editor == nullptr ? QString() : editorFilePath(editor));
    loadProjectMetadataForFile(currentEditorFilePath());
    setWelcomeVisible(editor == nullptr);
    refreshStatusBar();
}

void MainWindow::handleTabCloseRequested(int index) {
    auto* editor = editorAt(index);
    if (editor == nullptr) {
        return;
    }

    if (!maybeSaveEditor(editor)) {
        return;
    }

    if (editor == loadedEditor_) {
        loadedEditor_ = nullptr;
        addressToLine_.clear();
        lineToAddress_.clear();
    }

    editorTabs_->removeTab(index);
    editor->deleteLater();

    if (editorTabs_->count() == 0) {
        setCurrentFilePath(QString());
        setWelcomeVisible(true);
    }
}

void MainWindow::handleEditorModificationChanged(bool modified) {
    Q_UNUSED(modified);
    auto* document = qobject_cast<QTextDocument*>(sender());
    if (document == nullptr) {
        return;
    }

    auto* editor = qobject_cast<Core85::Gui::CodeEditor*>(document->parent());
    if (editor != nullptr) {
        updateTabTitle(editor);
    }
}

void MainWindow::handleProblemActivated(int line) {
    if (auto* editor = currentEditor()) {
        editor->goToLine(line);
    }
}

void MainWindow::showFindBar() {
    if (findReplaceBar_ == nullptr) {
        return;
    }

    if (auto* editor = currentEditor()) {
        const QString selectedText = editor->textCursor().selectedText();
        if (!selectedText.isEmpty()) {
            findReplaceBar_->setFindText(selectedText);
        }
    }
    findReplaceBar_->show();
    findReplaceBar_->focusFind();
}

void MainWindow::showReplaceBar() {
    showFindBar();
}

void MainWindow::hideFindBar() {
    if (findReplaceBar_ != nullptr) {
        findReplaceBar_->hide();
    }
}

void MainWindow::findNext(const QString& text, bool caseSensitive, bool wholeWord) {
    auto* editor = currentEditor();
    if (editor == nullptr || text.isEmpty()) {
        return;
    }

    if (!editor->find(text, buildFindFlags(false, caseSensitive, wholeWord))) {
        QTextCursor cursor = editor->textCursor();
        cursor.movePosition(QTextCursor::Start);
        editor->setTextCursor(cursor);
        editor->find(text, buildFindFlags(false, caseSensitive, wholeWord));
    }
}

void MainWindow::findPrevious(const QString& text, bool caseSensitive, bool wholeWord) {
    auto* editor = currentEditor();
    if (editor == nullptr || text.isEmpty()) {
        return;
    }

    if (!editor->find(text, buildFindFlags(true, caseSensitive, wholeWord))) {
        QTextCursor cursor = editor->textCursor();
        cursor.movePosition(QTextCursor::End);
        editor->setTextCursor(cursor);
        editor->find(text, buildFindFlags(true, caseSensitive, wholeWord));
    }
}

void MainWindow::replaceOne(const QString& findText,
                            const QString& replaceText,
                            bool caseSensitive,
                            bool wholeWord) {
    auto* editor = currentEditor();
    if (editor == nullptr || findText.isEmpty()) {
        return;
    }

    QTextCursor cursor = editor->textCursor();
    if (!cursor.hasSelection() || cursor.selectedText() != findText) {
        findNext(findText, caseSensitive, wholeWord);
        cursor = editor->textCursor();
    }

    if (cursor.hasSelection()) {
        cursor.insertText(replaceText);
    }
    findNext(findText, caseSensitive, wholeWord);
}

void MainWindow::replaceAll(const QString& findText,
                            const QString& replaceText,
                            bool caseSensitive,
                            bool wholeWord) {
    auto* editor = currentEditor();
    if (editor == nullptr || findText.isEmpty()) {
        return;
    }

    QTextCursor cursor = editor->textCursor();
    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::Start);
    editor->setTextCursor(cursor);

    int replacementCount = 0;
    while (editor->find(findText, buildFindFlags(false, caseSensitive, wholeWord))) {
        QTextCursor matchCursor = editor->textCursor();
        matchCursor.insertText(replaceText);
        ++replacementCount;
    }

    cursor = editor->textCursor();
    cursor.endEditBlock();
    messageStatusLabel_->setText(QStringLiteral("Replaced %1 occurrence(s)").arg(replacementCount));
}

void MainWindow::goToLine(int line) {
    if (auto* editor = currentEditor()) {
        editor->goToLine(line);
    }
}

void MainWindow::showTabContextMenu(const QPoint& pos) {
    QMenu menu(this);
    menu.addAction(QStringLiteral("Close"), this, [this]() {
        handleTabCloseRequested(editorTabs_->currentIndex());
    });
    menu.addAction(QStringLiteral("Close Others"), this, &MainWindow::closeOtherTabs);
    menu.addAction(QStringLiteral("Duplicate Tab"), this, &MainWindow::duplicateCurrentTab);
    menu.addSeparator();
    menu.addAction(QStringLiteral("Reveal in Explorer"), this, &MainWindow::revealCurrentFileInExplorer);
    menu.exec(editorTabs_->tabBar()->mapToGlobal(pos));
}

void MainWindow::duplicateCurrentTab() {
    auto* editor = currentEditor();
    if (editor == nullptr) {
        return;
    }

    createUntitledTab(editor->toPlainText());
    if (auto* duplicated = currentEditor()) {
        duplicated->document()->setModified(true);
        updateTabTitle(duplicated);
    }
}

void MainWindow::closeOtherTabs() {
    auto* keep = currentEditor();
    if (keep == nullptr) {
        return;
    }

    for (int index = editorTabs_->count() - 1; index >= 0; --index) {
        auto* editor = editorAt(index);
        if (editor != nullptr && editor != keep) {
            editorTabs_->setCurrentIndex(index);
            handleTabCloseRequested(index);
        }
    }
    editorTabs_->setCurrentWidget(keep);
}

void MainWindow::revealCurrentFileInExplorer() {
    const QString path = currentEditorFilePath();
    if (path.isEmpty()) {
        messageStatusLabel_->setText(QStringLiteral("Only saved files can be revealed in the explorer."));
        return;
    }

    QFileInfo info(path);
    if (workspaceRootPath_.isEmpty() || !info.absoluteFilePath().startsWith(workspaceRootPath_)) {
        setWorkspaceRoot(info.absolutePath());
    }

    const QModelIndex index = fileSystemModel_->index(path);
    if (index.isValid()) {
        explorerTree_->expand(index.parent());
        explorerTree_->setCurrentIndex(index);
        explorerTree_->scrollTo(index);
        explorerDock_->raise();
    }
}

void MainWindow::applyLightTheme() {
    applyTheme(QStringLiteral("light"));
}

void MainWindow::applyDarkTheme() {
    applyTheme(QStringLiteral("dark"));
}

void MainWindow::showKeyboardShortcuts() {
    QMessageBox::information(
        this,
        QStringLiteral("Keyboard Shortcuts"),
        QStringLiteral(
            "Ctrl+Shift+B Assemble and Load\n"
            "F5 Run\n"
            "Shift+F5 Pause\n"
            "F11 Step Into\n"
            "F10 Step Over\n"
            "Ctrl+Shift+R Reset\n"
            "Ctrl+F Find\n"
            "Ctrl+H Replace\n"
            "Ctrl+G Go to Line\n"
            "Ctrl+L Toggle Follow PC"));
}

void MainWindow::buildUi() {
    createMenus();
    createToolbar();
    createDocks();

    pcStatusLabel_ = new QLabel(QStringLiteral("PC: 0x0000"), this);
    cyclesStatusLabel_ = new QLabel(QStringLiteral("T-States: 0"), this);
    stateStatusLabel_ = new QLabel(QStringLiteral("State: Paused"), this);
    messageStatusLabel_ = new QLabel(QStringLiteral("Welcome to Core85"), this);
    statusBar()->addWidget(messageStatusLabel_, 1);
    statusBar()->addPermanentWidget(pcStatusLabel_);
    statusBar()->addPermanentWidget(cyclesStatusLabel_);
    statusBar()->addPermanentWidget(stateStatusLabel_);
}

void MainWindow::createMenus() {
    auto* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));

    auto* newAction = fileMenu->addAction(style()->standardIcon(QStyle::SP_FileIcon),
                                          QStringLiteral("&New Assembly File"),
                                          this,
                                          &MainWindow::newAssemblyFile);
    newAction->setShortcut(QKeySequence::New);
    newAction->setToolTip(QStringLiteral("Create a new assembly source file (Ctrl+N)"));

    auto* openAction = fileMenu->addAction(style()->standardIcon(QStyle::SP_DialogOpenButton),
                                           QStringLiteral("&Open Files..."),
                                           this,
                                           &MainWindow::openSourceFile);
    openAction->setShortcut(QKeySequence::Open);
    openAction->setToolTip(QStringLiteral("Open one or more assembly files (Ctrl+O)"));

    openFolderAction_ = fileMenu->addAction(style()->standardIcon(QStyle::SP_DirOpenIcon),
                                            QStringLiteral("Open &Folder..."),
                                            this,
                                            &MainWindow::openWorkspaceFolder);
    openFolderAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+O")));
    openFolderAction_->setToolTip(QStringLiteral("Open a workspace folder in the explorer (Ctrl+Alt+O)"));

    fileMenu->addAction(style()->standardIcon(QStyle::SP_ComputerIcon),
                        QStringLiteral("Open &Sample Program"),
                        this,
                        &MainWindow::openSampleProgram);

    recentFilesMenu_ = fileMenu->addMenu(QStringLiteral("Open Recent &Files"));
    recentFoldersMenu_ = fileMenu->addMenu(QStringLiteral("Open Recent F&olders"));

    fileMenu->addSeparator();

    auto* saveAction = fileMenu->addAction(style()->standardIcon(QStyle::SP_DialogSaveButton),
                                           QStringLiteral("&Save"),
                                           this,
                                           &MainWindow::saveSourceFile);
    saveAction->setShortcut(QKeySequence::Save);
    saveAction->setToolTip(QStringLiteral("Save the active file (Ctrl+S)"));

    saveAllAction_ = fileMenu->addAction(style()->standardIcon(QStyle::SP_DialogSaveButton),
                                         QStringLiteral("Save A&ll"),
                                         this,
                                         &MainWindow::saveAllFiles);
    saveAllAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")));

    auto* saveAsAction =
        fileMenu->addAction(QStringLiteral("Save &As..."), this, &MainWindow::saveSourceFileAs);
    saveAsAction->setShortcut(QKeySequence::SaveAs);

    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("Load &HEX..."), this, &MainWindow::loadHexFile);
    fileMenu->addSeparator();

    auto* exitAction = fileMenu->addAction(style()->standardIcon(QStyle::SP_DialogCloseButton),
                                           QStringLiteral("E&xit"),
                                           this,
                                           &QWidget::close);
    exitAction->setShortcut(QKeySequence::Quit);

    auto* editMenu = menuBar()->addMenu(QStringLiteral("&Edit"));
    auto* findAction = editMenu->addAction(QStringLiteral("&Find"), this, &MainWindow::showFindBar);
    findAction->setShortcut(QKeySequence::Find);
    auto* replaceAction =
        editMenu->addAction(QStringLiteral("&Replace"), this, &MainWindow::showReplaceBar);
    replaceAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+H")));
    auto* gotoAction = editMenu->addAction(QStringLiteral("&Go to Line"), this, [this]() {
        showFindBar();
        if (findReplaceBar_ != nullptr) {
            findReplaceBar_->focusGoToLine();
        }
    });
    gotoAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+G")));

    viewMenu_ = menuBar()->addMenu(QStringLiteral("&View"));

    auto* preferencesMenu = menuBar()->addMenu(QStringLiteral("&Preferences"));
    themeGroup_ = new QActionGroup(this);
    themeGroup_->setExclusive(true);
    lightThemeAction_ =
        preferencesMenu->addAction(QStringLiteral("&Light Theme"), this, &MainWindow::applyLightTheme);
    darkThemeAction_ =
        preferencesMenu->addAction(QStringLiteral("&Dark Theme"), this, &MainWindow::applyDarkTheme);
    lightThemeAction_->setCheckable(true);
    darkThemeAction_->setCheckable(true);
    themeGroup_->addAction(lightThemeAction_);
    themeGroup_->addAction(darkThemeAction_);
    darkThemeAction_->setChecked(true);

    auto* helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));
    helpMenu->addAction(QStringLiteral("Keyboard &Shortcuts"), this, &MainWindow::showKeyboardShortcuts);
}

void MainWindow::createToolbar() {
    executionToolBar_ = addToolBar(QStringLiteral("Execution Controls"));
    executionToolBar_->setObjectName(QStringLiteral("execution_toolbar"));
    executionToolBar_->setMovable(false);
    executionToolBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    executionToolBar_->setIconSize(QSize(18, 18));

    if (viewMenu_ != nullptr) {
        viewMenu_->addAction(executionToolBar_->toggleViewAction());
        viewMenu_->addSeparator();
    }

    assembleAction_ = executionToolBar_->addAction(style()->standardIcon(QStyle::SP_DialogApplyButton),
                                                   QStringLiteral("Assemble & Load"),
                                                   this,
                                                   &MainWindow::assembleAndLoad);
    assembleAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+B")));
    assembleAction_->setToolTip(QStringLiteral("Assemble and load the active file (Ctrl+Shift+B)"));

    runAction_ = executionToolBar_->addAction(QIcon(),
                                              QStringLiteral("Run"),
                                              [this]() {
                                                  QMetaObject::invokeMethod(
                                                      emulatorController_,
                                                      &Core85::Gui::EmulatorController::run,
                                                      Qt::QueuedConnection);
                                              });
    runAction_->setShortcut(QKeySequence(Qt::Key_F5));
    runAction_->setToolTip(QStringLiteral("Run the loaded program (F5)"));

    pauseAction_ = executionToolBar_->addAction(QIcon(),
                                                QStringLiteral("Pause"),
                                                [this]() {
                                                    QMetaObject::invokeMethod(
                                                        emulatorController_,
                                                        &Core85::Gui::EmulatorController::pause,
                                                        Qt::QueuedConnection);
                                                });
    pauseAction_->setShortcut(QKeySequence(QStringLiteral("Shift+F5")));
    pauseAction_->setToolTip(QStringLiteral("Pause execution (Shift+F5)"));

    stepIntoAction_ = executionToolBar_->addAction(QIcon(),
                                                   QStringLiteral("Step Into"),
                                                   [this]() {
                                                       QMetaObject::invokeMethod(
                                                           emulatorController_,
                                                           &Core85::Gui::EmulatorController::stepInto,
                                                           Qt::QueuedConnection);
                                                   });
    stepIntoAction_->setShortcut(QKeySequence(Qt::Key_F11));
    stepIntoAction_->setToolTip(QStringLiteral("Execute the next instruction (F11)"));

    stepOverAction_ = executionToolBar_->addAction(QIcon(),
                                                   QStringLiteral("Step Over"),
                                                   [this]() {
                                                       QMetaObject::invokeMethod(
                                                           emulatorController_,
                                                           &Core85::Gui::EmulatorController::stepOver,
                                                           Qt::QueuedConnection);
                                                   });
    stepOverAction_->setShortcut(QKeySequence(Qt::Key_F10));
    stepOverAction_->setToolTip(QStringLiteral("Step over CALL instructions (F10)"));

    resetAction_ = executionToolBar_->addAction(style()->standardIcon(QStyle::SP_BrowserReload),
                                                QStringLiteral("Reset"),
                                                [this]() {
                                                    QMetaObject::invokeMethod(
                                                        emulatorController_,
                                                        &Core85::Gui::EmulatorController::resetToLoadedProgram,
                                                        Qt::QueuedConnection);
                                                });
    resetAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+R")));
    resetAction_->setToolTip(QStringLiteral("Reset CPU state to the last loaded program (Ctrl+Shift+R)"));

    executionToolBar_->addSeparator();
    autoFollowAction_ = executionToolBar_->addAction(style()->standardIcon(QStyle::SP_FileDialogDetailedView),
                                                     QStringLiteral("Follow PC"));
    autoFollowAction_->setCheckable(true);
    autoFollowAction_->setChecked(true);
    autoFollowAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+L")));
    autoFollowAction_->setToolTip(QStringLiteral("Keep the memory view focused on the PC (Ctrl+L)"));

    executionToolBar_->addSeparator();
    executionToolBar_->addWidget(new QLabel(QStringLiteral("Speed"), executionToolBar_));
    speedCombo_ = new QComboBox(executionToolBar_);
    speedCombo_->addItems({QStringLiteral("Manual"),
                           QStringLiteral("Slow"),
                           QStringLiteral("Medium"),
                           QStringLiteral("Fast"),
                           QStringLiteral("Max")});
    speedCombo_->setCurrentIndex(4);
    speedCombo_->setToolTip(QStringLiteral("Choose how quickly the emulator runs between UI updates."));
    executionToolBar_->addWidget(speedCombo_);
    refreshActionIcons();
}

void MainWindow::createDocks() {
    centerStack_ = new QStackedWidget(this);
    welcomePage_ = new Core85::Gui::WelcomePage(centerStack_);
    auto* editorPage = new QWidget(centerStack_);
    auto* editorLayout = new QVBoxLayout(editorPage);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);

    findReplaceBar_ = new Core85::Gui::FindReplaceBar(editorPage);
    findReplaceBar_->hide();
    editorLayout->addWidget(findReplaceBar_);

    editorTabs_ = new QTabWidget(editorPage);
    editorTabs_->setDocumentMode(true);
    editorTabs_->setMovable(true);
    editorTabs_->setTabsClosable(true);
    editorTabs_->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    editorLayout->addWidget(editorTabs_, 1);

    centerStack_->addWidget(welcomePage_);
    centerStack_->addWidget(editorPage);
    setCentralWidget(centerStack_);

    explorerDock_ = new QDockWidget(QStringLiteral("Workspace"), this);
    explorerDock_->setObjectName(QStringLiteral("explorer_dock"));
    explorerStack_ = new QStackedWidget(explorerDock_);
    fileSystemModel_ = new QFileSystemModel(explorerDock_);
    fileSystemModel_->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    fileSystemModel_->setRootPath(QDir::currentPath());
    explorerTree_ = new QTreeView(explorerDock_);
    explorerTree_->setModel(fileSystemModel_);
    explorerTree_->setHeaderHidden(false);
    explorerTree_->setAnimated(true);
    explorerTree_->setSortingEnabled(true);
    explorerTree_->sortByColumn(0, Qt::AscendingOrder);
    explorerTree_->header()->setStretchLastSection(false);
    explorerTree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < fileSystemModel_->columnCount(); ++column) {
        explorerTree_->hideColumn(column);
    }

    auto* placeholder = new QWidget(explorerStack_);
    auto* placeholderLayout = new QVBoxLayout(placeholder);
    placeholderLayout->setContentsMargins(24, 24, 24, 24);
    placeholderLayout->setSpacing(14);
    placeholderLayout->addStretch();

    auto* placeholderIcon = new QLabel(placeholder);
    placeholderIcon->setAlignment(Qt::AlignCenter);
    placeholderIcon->setPixmap(style()->standardIcon(QStyle::SP_DirOpenIcon).pixmap(52, 52));
    placeholderLayout->addWidget(placeholderIcon);

    auto* placeholderTitle = new QLabel(QStringLiteral("Open a workspace folder"), placeholder);
    placeholderTitle->setAlignment(Qt::AlignCenter);
    placeholderTitle->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 700;"));
    placeholderLayout->addWidget(placeholderTitle);

    auto* placeholderText = new QLabel(
        QStringLiteral("Start writing immediately, or open a directory to browse files, demos, and saved projects."),
        placeholder);
    placeholderText->setAlignment(Qt::AlignCenter);
    placeholderText->setWordWrap(true);
    placeholderLayout->addWidget(placeholderText);

    auto* openFolderButton = new QPushButton(style()->standardIcon(QStyle::SP_DirOpenIcon),
                                             QStringLiteral("Open Folder"),
                                             placeholder);
    openFolderButton->setMinimumHeight(38);
    openFolderButton->setCursor(Qt::PointingHandCursor);
    placeholderLayout->addWidget(openFolderButton, 0, Qt::AlignHCenter);
    placeholderLayout->addStretch();

    explorerStack_->addWidget(placeholder);
    explorerStack_->addWidget(explorerTree_);
    explorerStack_->setCurrentIndex(0);
    explorerDock_->setWidget(explorerStack_);
    addDockWidget(Qt::LeftDockWidgetArea, explorerDock_);

    registersDock_ = new QDockWidget(QStringLiteral("Registers"), this);
    registersDock_->setObjectName(QStringLiteral("registers_dock"));
    registerViewer_ = new Core85::Gui::RegisterViewer(registersDock_);
    registersDock_->setWidget(registerViewer_);
    addDockWidget(Qt::RightDockWidgetArea, registersDock_);

    memoryDock_ = new QDockWidget(QStringLiteral("Memory"), this);
    memoryDock_->setObjectName(QStringLiteral("memory_dock"));
    memoryViewer_ = new Core85::Gui::MemoryViewer(memoryDock_);
    memoryDock_->setWidget(memoryViewer_);
    addDockWidget(Qt::RightDockWidgetArea, memoryDock_);
    splitDockWidget(registersDock_, memoryDock_, Qt::Vertical);

    ioDock_ = new QDockWidget(QStringLiteral("I/O Peripherals"), this);
    ioDock_->setObjectName(QStringLiteral("io_dock"));
    ioPanel_ = new Core85::Gui::IOPanel(ioDock_);
    ioDock_->setWidget(ioPanel_);
    addDockWidget(Qt::BottomDockWidgetArea, ioDock_);

    errorDock_ = new QDockWidget(QStringLiteral("Problems"), this);
    errorDock_->setObjectName(QStringLiteral("problems_dock"));
    auto* diagnosticsTabs = new QTabWidget(errorDock_);
    problemsPanel_ = new Core85::Gui::ProblemsPanel(diagnosticsTabs);
    errorConsole_ = new QPlainTextEdit(diagnosticsTabs);
    errorConsole_->setReadOnly(true);
    diagnosticsTabs->addTab(problemsPanel_, QStringLiteral("Problems"));
    diagnosticsTabs->addTab(errorConsole_, QStringLiteral("Console"));
    errorDock_->setWidget(diagnosticsTabs);
    addDockWidget(Qt::BottomDockWidgetArea, errorDock_);
    tabifyDockWidget(ioDock_, errorDock_);

    if (viewMenu_ != nullptr) {
        viewMenu_->addAction(explorerDock_->toggleViewAction());
        viewMenu_->addAction(registersDock_->toggleViewAction());
        viewMenu_->addAction(memoryDock_->toggleViewAction());
        viewMenu_->addAction(ioDock_->toggleViewAction());
        viewMenu_->addAction(errorDock_->toggleViewAction());
    }

    connect(openFolderButton, &QPushButton::clicked, this, &MainWindow::openWorkspaceFolder);
    connect(welcomePage_, &Core85::Gui::WelcomePage::newFileRequested, this, &MainWindow::newAssemblyFile);
    connect(welcomePage_, &Core85::Gui::WelcomePage::openFilesRequested, this, &MainWindow::openSourceFile);
    connect(welcomePage_, &Core85::Gui::WelcomePage::openFolderRequested, this, &MainWindow::openWorkspaceFolder);
    connect(welcomePage_, &Core85::Gui::WelcomePage::openSampleRequested, this, &MainWindow::openSampleProgram);
    connect(findReplaceBar_,
            &Core85::Gui::FindReplaceBar::findNextRequested,
            this,
            &MainWindow::findNext);
    connect(findReplaceBar_,
            &Core85::Gui::FindReplaceBar::findPreviousRequested,
            this,
            &MainWindow::findPrevious);
    connect(findReplaceBar_,
            &Core85::Gui::FindReplaceBar::replaceOneRequested,
            this,
            &MainWindow::replaceOne);
    connect(findReplaceBar_,
            &Core85::Gui::FindReplaceBar::replaceAllRequested,
            this,
            &MainWindow::replaceAll);
    connect(findReplaceBar_, &Core85::Gui::FindReplaceBar::goToLineRequested, this, &MainWindow::goToLine);
    connect(findReplaceBar_, &Core85::Gui::FindReplaceBar::closeRequested, this, &MainWindow::hideFindBar);
    connect(problemsPanel_, &Core85::Gui::ProblemsPanel::problemActivated, this, &MainWindow::handleProblemActivated);
}

void MainWindow::wireController() {
    emulatorController_ = new Core85::Gui::EmulatorController();
    emulatorController_->moveToThread(&emulatorThread_);
    connect(&emulatorThread_, &QThread::finished, emulatorController_, &QObject::deleteLater);
    connect(emulatorController_,
            &Core85::Gui::EmulatorController::snapshotReady,
            this,
            &MainWindow::handleSnapshot);
    connect(emulatorController_,
            &Core85::Gui::EmulatorController::executionStateChanged,
            this,
            &MainWindow::handleExecutionStateChanged);
    connect(emulatorController_,
            &Core85::Gui::EmulatorController::emulatorError,
            this,
            &MainWindow::handleEmulatorError);

    connect(registerViewer_,
            &Core85::Gui::RegisterViewer::registerWriteRequested,
            this,
            &MainWindow::handleRegisterEdit);
    connect(registerViewer_,
            &Core85::Gui::RegisterViewer::invalidInputRequested,
            this,
            &MainWindow::handleInvalidRegisterInput);
    connect(memoryViewer_, &Core85::Gui::MemoryViewer::memoryWriteRequested, this, &MainWindow::handleMemoryEdit);
    connect(memoryViewer_,
            &Core85::Gui::MemoryViewer::invalidAddressRequested,
            this,
            &MainWindow::handleInvalidMemoryAddress);
    connect(ioPanel_, &Core85::Gui::IOPanel::inputPortChanged, this, [this](quint8 port, quint8 value) {
        QMetaObject::invokeMethod(
            emulatorController_,
            [controller = emulatorController_, port, value]() { controller->setInputPort(port, value); },
            Qt::QueuedConnection);
    });
    connect(ioPanel_,
            &Core85::Gui::IOPanel::projectMetadataChanged,
            this,
            &MainWindow::handleProjectMetadataChanged);
    connect(speedCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        QMetaObject::invokeMethod(
            emulatorController_,
            [controller = emulatorController_, index]() { controller->setSpeedMode(index); },
            Qt::QueuedConnection);
    });
    connect(explorerTree_, &QTreeView::doubleClicked, this, &MainWindow::handleExplorerActivated);
    connect(editorTabs_, &QTabWidget::currentChanged, this, &MainWindow::handleCurrentTabChanged);
    connect(editorTabs_, &QTabWidget::tabCloseRequested, this, &MainWindow::handleTabCloseRequested);
    connect(editorTabs_->tabBar(),
            &QTabBar::customContextMenuRequested,
            this,
            &MainWindow::showTabContextMenu);

    emulatorThread_.setObjectName(QStringLiteral("Core85EmulatorThread"));
    emulatorThread_.start();
}

void MainWindow::refreshStatusBar() {
    if (hasSnapshot_) {
        pcStatusLabel_->setText(
            QStringLiteral("PC: %1").arg(Core85::Gui::formatHex16(lastSnapshot_.cpuState.pc)));
        cyclesStatusLabel_->setText(
            QStringLiteral("T-States: %1").arg(lastSnapshot_.cpuState.elapsedCycles));
    } else {
        pcStatusLabel_->setText(QStringLiteral("PC: 0x0000"));
        cyclesStatusLabel_->setText(QStringLiteral("T-States: 0"));
    }

    QString stateText = Core85::Gui::executionStateToString(executionState_);
    if (!hasLoadedProgram_) {
        stateText = QStringLiteral("No Program Loaded");
    }
    stateStatusLabel_->setText(QStringLiteral("State: %1").arg(stateText));
    updateExecutionActions();
    updateWindowTitle();
}

void MainWindow::updateExecutionActions() {
    const bool hasEditor = currentEditor() != nullptr;
    const bool running = executionState_ == Core85::Gui::ExecutionState::Running;
    const bool loaded = hasLoadedProgram_;

    if (assembleAction_ != nullptr) {
        assembleAction_->setEnabled(hasEditor && !running);
    }
    if (runAction_ != nullptr) {
        runAction_->setEnabled(loaded && !running);
    }
    if (pauseAction_ != nullptr) {
        pauseAction_->setEnabled(running);
    }
    if (stepIntoAction_ != nullptr) {
        stepIntoAction_->setEnabled(loaded && !running);
    }
    if (stepOverAction_ != nullptr) {
        stepOverAction_->setEnabled(loaded && !running);
    }
    if (resetAction_ != nullptr) {
        resetAction_->setEnabled(loaded && !running);
    }
}

void MainWindow::clearAssemblerErrors() {
    problemsPanel_->clearProblems();
    for (int index = 0; index < editorTabs_->count(); ++index) {
        if (auto* editor = editorAt(index)) {
            editor->clearProblemMarkers();
        }
    }
    messageStatusLabel_->setText(QStringLiteral("Ready"));
}

void MainWindow::clearExecutionLineHighlights() {
    for (int index = 0; index < editorTabs_->count(); ++index) {
        if (auto* editor = editorAt(index)) {
            editor->setCurrentExecutionLine(-1);
        }
    }
}

void MainWindow::showAssemblerErrors(const std::vector<Core85::AssemblerError>& errors) {
    if (errors.empty()) {
        return;
    }

    problemsPanel_->setProblems(errors);
    errorDock_->show();
    errorDock_->raise();

    if (auto* editor = currentEditor()) {
        const auto& first = errors.front();
        editor->setErrorLine(static_cast<int>(first.line), QString::fromStdString(first.message));
        editor->goToLine(static_cast<int>(first.line));
        messageStatusLabel_->setText(
            QStringLiteral("Assembly failed at line %1").arg(first.line));
    }
}

void MainWindow::rebuildSourceMappings(const std::vector<Core85::SourceMappingEntry>& sourceMap) {
    addressToLine_.clear();
    lineToAddress_.clear();

    for (const auto& entry : sourceMap) {
        addressToLine_.insert(entry.address, static_cast<int>(entry.line));
        if (!lineToAddress_.contains(static_cast<int>(entry.line)) ||
            entry.address < lineToAddress_.value(static_cast<int>(entry.line))) {
            lineToAddress_.insert(static_cast<int>(entry.line), entry.address);
        }
    }
}

void MainWindow::syncBreakpointAddresses() {
    const QList<quint16> addresses = currentBreakpointAddresses();
    QMetaObject::invokeMethod(
        emulatorController_,
        [controller = emulatorController_, addresses]() { controller->setBreakpoints(addresses); },
        Qt::QueuedConnection);
}

void MainWindow::requestControllerSnapshot() {
    QMetaObject::invokeMethod(
        emulatorController_, &Core85::Gui::EmulatorController::requestSnapshot, Qt::QueuedConnection);
}

void MainWindow::setCurrentFilePath(const QString& path) {
    currentFilePath_ = path;
    updateWindowTitle();
}

void MainWindow::updateWindowTitle() {
    QString title = QStringLiteral("Core85");
    if (!currentFilePath_.isEmpty()) {
        title += QStringLiteral(" - %1").arg(QFileInfo(currentFilePath_).fileName());
    } else if (editorTabs_->count() > 0 && currentEditor() != nullptr) {
        title += QStringLiteral(" - %1").arg(editorDisplayName(currentEditor()));
    }
    if (!workspaceRootPath_.isEmpty()) {
        title += QStringLiteral(" [%1]").arg(QFileInfo(workspaceRootPath_).fileName());
    }
    setWindowTitle(title);
}

void MainWindow::refreshActionIcons() {
    const QColor iconColor =
        currentTheme_ == QStringLiteral("dark") ? QColor(QStringLiteral("#F8FBFF"))
                                                : QColor(QStringLiteral("#0F172A"));

    if (runAction_ != nullptr) {
        runAction_->setIcon(makeTransportIcon(TransportIconKind::Run, iconColor));
    }
    if (pauseAction_ != nullptr) {
        pauseAction_->setIcon(makeTransportIcon(TransportIconKind::Pause, iconColor));
    }
    if (stepIntoAction_ != nullptr) {
        stepIntoAction_->setIcon(makeTransportIcon(TransportIconKind::StepInto, iconColor));
    }
    if (stepOverAction_ != nullptr) {
        stepOverAction_->setIcon(makeTransportIcon(TransportIconKind::StepOver, iconColor));
    }
}

void MainWindow::setWelcomeVisible(bool visible) {
    if (centerStack_ == nullptr) {
        return;
    }

    centerStack_->setCurrentIndex(visible ? 0 : 1);
    if (!visible && currentEditor() != nullptr) {
        currentEditor()->setFocus();
    }
}

bool MainWindow::saveSourceToPath(const QString& path) {
    return saveEditorToPath(currentEditor(), path);
}

bool MainWindow::saveEditorToPath(Core85::Gui::CodeEditor* editor, const QString& path) {
    if (editor == nullptr || path.isEmpty()) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("Save Failed"), file.errorString());
        return false;
    }

    QTextStream stream(&file);
    stream << editor->toPlainText();

    setEditorFilePath(editor, path);
    editor->document()->setModified(false);
    updateTabTitle(editor);
    recordRecentFile(path);

    if (editor == currentEditor()) {
        setCurrentFilePath(path);
        loadProjectMetadataForFile(path);
    }

    saveProjectMetadataForFile(path);
    messageStatusLabel_->setText(QStringLiteral("Saved %1").arg(QFileInfo(path).fileName()));
    return true;
}

bool MainWindow::saveEditorAs(Core85::Gui::CodeEditor* editor) {
    if (editor == nullptr) {
        return false;
    }

    const QString suggestedPath = editorFilePath(editor).isEmpty()
                                      ? QStringLiteral("program.asm")
                                      : editorFilePath(editor);
    const QString path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Save Assembly Source"),
        suggestedPath,
        QStringLiteral("Assembly Files (*.asm);;All Files (*)"));

    if (path.isEmpty()) {
        return false;
    }

    return saveEditorToPath(editor, path);
}

bool MainWindow::maybeSaveEditor(Core85::Gui::CodeEditor* editor) {
    if (editor == nullptr || !editor->document()->isModified()) {
        return true;
    }

    const QMessageBox::StandardButton button = QMessageBox::question(
        this,
        QStringLiteral("Unsaved Changes"),
        QStringLiteral("Save changes to %1?").arg(editorDisplayName(editor)),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);

    if (button == QMessageBox::Cancel) {
        return false;
    }

    if (button == QMessageBox::Save) {
        const QString path = editorFilePath(editor);
        return path.isEmpty() ? saveEditorAs(editor) : saveEditorToPath(editor, path);
    }

    return true;
}

bool MainWindow::maybeSaveAllEditors() {
    for (int index = 0; index < editorTabs_->count(); ++index) {
        if (!maybeSaveEditor(editorAt(index))) {
            return false;
        }
    }
    return true;
}

void MainWindow::openTextFileInTab(const QString& path, bool activate) {
    const QString absolutePath = QFileInfo(path).absoluteFilePath();
    const int existingIndex = findEditorTabByPath(absolutePath);
    if (existingIndex >= 0) {
        if (activate) {
            editorTabs_->setCurrentIndex(existingIndex);
        }
        setWelcomeVisible(false);
        return;
    }

    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("Open Failed"), file.errorString());
        return;
    }

    auto* editor = new Core85::Gui::CodeEditor(editorTabs_);
    QTextStream stream(&file);
    editor->setPlainText(stream.readAll());
    editor->document()->setModified(false);
    setEditorFilePath(editor, absolutePath);
    createEditorTab(editor, activate);
    recordRecentFile(absolutePath);
}

void MainWindow::createEditorTab(Core85::Gui::CodeEditor* editor, bool activate) {
    if (editor == nullptr) {
        return;
    }

    connect(editor, &Core85::Gui::CodeEditor::breakpointToggled, this, &MainWindow::handleBreakpointToggled);
    connect(editor->document(),
            &QTextDocument::modificationChanged,
            this,
            &MainWindow::handleEditorModificationChanged);

    const int tabIndex = editorTabs_->addTab(editor, editorDisplayName(editor));
    editorTabs_->setTabToolTip(tabIndex,
                               editorFilePath(editor).isEmpty() ? editorDisplayName(editor)
                                                                : editorFilePath(editor));
    if (activate) {
        editorTabs_->setCurrentIndex(tabIndex);
    }
    updateTabTitle(editor);
    setWelcomeVisible(false);
}

void MainWindow::loadHexFromPath(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("HEX Load Failed"), file.errorString());
        return;
    }

    const QString hexText = QString::fromUtf8(file.readAll());
    const Core85::HexLoadResult result = assembler_.loadIntelHex(hexText.toStdString());
    if (!result.success) {
        showAssemblerErrors(result.errors);
        return;
    }

    QByteArray bytes;
    bytes.resize(static_cast<int>(result.bytes.size()));
    for (std::size_t index = 0; index < result.bytes.size(); ++index) {
        bytes[static_cast<int>(index)] = static_cast<char>(result.bytes[index]);
    }

    loadedEditor_ = nullptr;
    hasLoadedProgram_ = true;
    addressToLine_.clear();
    lineToAddress_.clear();
    clearAssemblerErrors();
    clearExecutionLineHighlights();
    setCurrentFilePath(path);
    loadProjectMetadataForFile(path);
    recordRecentFile(path);

    QMetaObject::invokeMethod(
        emulatorController_,
        [controller = emulatorController_, origin = result.origin, bytes]() {
            controller->loadProgram(origin, bytes);
        },
        Qt::QueuedConnection);

    messageStatusLabel_->setText(QStringLiteral("Loaded HEX at %1").arg(Core85::Gui::formatHex16(result.origin)));
    updateExecutionActions();
}

void MainWindow::setWorkspaceRoot(const QString& path) {
    workspaceRootPath_ = path;
    if (workspaceRootPath_.isEmpty()) {
        explorerStack_->setCurrentIndex(0);
        explorerDock_->setWindowTitle(QStringLiteral("Workspace"));
        updateWindowTitle();
        return;
    }

    const QModelIndex rootIndex = fileSystemModel_->setRootPath(path);
    explorerTree_->setRootIndex(rootIndex);
    explorerStack_->setCurrentWidget(explorerTree_);
    explorerDock_->setWindowTitle(QStringLiteral("Workspace - %1").arg(QFileInfo(path).fileName()));
    recordRecentFolder(path);
    updateWindowTitle();
}

void MainWindow::createUntitledTab(const QString& content) {
    auto* editor = new Core85::Gui::CodeEditor(editorTabs_);
    editor->setProperty("untitledName", QStringLiteral("untitled-%1.asm").arg(untitledCounter_++));
    editor->setPlainText(content);
    editor->document()->setModified(!content.isEmpty());
    createEditorTab(editor, true);
}

int MainWindow::findEditorTabByPath(const QString& path) const {
    for (int index = 0; index < editorTabs_->count(); ++index) {
        if (editorFilePath(editorAt(index)) == path) {
            return index;
        }
    }
    return -1;
}

Core85::Gui::CodeEditor* MainWindow::currentEditor() const {
    return editorAt(editorTabs_->currentIndex());
}

Core85::Gui::CodeEditor* MainWindow::editorAt(int index) const {
    return qobject_cast<Core85::Gui::CodeEditor*>(editorTabs_->widget(index));
}

QString MainWindow::editorFilePath(const Core85::Gui::CodeEditor* editor) const {
    return editor == nullptr ? QString() : editor->property("filePath").toString();
}

void MainWindow::setEditorFilePath(Core85::Gui::CodeEditor* editor, const QString& path) {
    if (editor == nullptr) {
        return;
    }

    editor->setProperty("filePath", path);
    if (!path.isEmpty()) {
        editor->setProperty("untitledName", QVariant());
    }
}

QString MainWindow::currentEditorFilePath() const {
    return editorFilePath(currentEditor());
}

QString MainWindow::editorDisplayName(const Core85::Gui::CodeEditor* editor) const {
    const QString path = editorFilePath(editor);
    if (!path.isEmpty()) {
        return QFileInfo(path).fileName();
    }

    const QString untitledName = editor == nullptr ? QString() : editor->property("untitledName").toString();
    return untitledName.isEmpty() ? QStringLiteral("untitled.asm") : untitledName;
}

void MainWindow::updateTabTitle(Core85::Gui::CodeEditor* editor) {
    if (editor == nullptr) {
        return;
    }

    const int index = editorTabs_->indexOf(editor);
    if (index < 0) {
        return;
    }

    QString title = editorDisplayName(editor);
    if (editor->document()->isModified()) {
        title.prepend(QStringLiteral("*"));
    }

    const QString tooltip = editorFilePath(editor).isEmpty()
                                ? QStringLiteral("Unsaved file: %1").arg(editorDisplayName(editor))
                                : editorFilePath(editor);
    editorTabs_->setTabText(index, title);
    editorTabs_->setTabToolTip(index, tooltip);
}

QString MainWindow::projectMetadataPathForFile(const QString& filePath) const {
    if (filePath.isEmpty()) {
        return {};
    }
    return filePath + QStringLiteral(".core85project.json");
}

void MainWindow::loadProjectMetadataForFile(const QString& filePath) {
    projectMetadata_ = {};

    const QString metadataPath = projectMetadataPathForFile(filePath);
    if (metadataPath.isEmpty() || !QFileInfo::exists(metadataPath)) {
        ioPanel_->setProjectMetadata(projectMetadata_);
        return;
    }

    QFile file(metadataPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ioPanel_->setProjectMetadata(projectMetadata_);
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    const QJsonObject object = document.object();
    projectMetadata_.inputPort = static_cast<quint8>(object.value(QStringLiteral("inputPort")).toInt(0));
    projectMetadata_.ledPort = static_cast<quint8>(object.value(QStringLiteral("ledPort")).toInt(1));
    projectMetadata_.segmentLeftPort = static_cast<quint8>(
        object.value(QStringLiteral("segmentLeftPort"))
            .toInt(object.value(QStringLiteral("segmentPort")).toInt(2)));
    projectMetadata_.segmentRightPort = static_cast<quint8>(
        object.value(QStringLiteral("segmentRightPort")).toInt(3));
    ioPanel_->setProjectMetadata(projectMetadata_);
}

void MainWindow::saveProjectMetadataForFile(const QString& filePath) const {
    const QString metadataPath = projectMetadataPathForFile(filePath);
    if (metadataPath.isEmpty()) {
        return;
    }

    QFile file(metadataPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return;
    }

    const QJsonObject object{
        {QStringLiteral("inputPort"), projectMetadata_.inputPort},
        {QStringLiteral("ledPort"), projectMetadata_.ledPort},
        {QStringLiteral("segmentLeftPort"), projectMetadata_.segmentLeftPort},
        {QStringLiteral("segmentRightPort"), projectMetadata_.segmentRightPort},
    };
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
}

void MainWindow::applyTheme(const QString& themeName) {
    currentTheme_ = themeName;
    auto* application = qobject_cast<QApplication*>(QCoreApplication::instance());
    if (application == nullptr) {
        return;
    }

    if (lightThemeAction_ != nullptr) {
        lightThemeAction_->setChecked(themeName == QStringLiteral("light"));
    }
    if (darkThemeAction_ != nullptr) {
        darkThemeAction_->setChecked(themeName == QStringLiteral("dark"));
    }

    if (themeName == QStringLiteral("light")) {
        application->setStyleSheet(QStringLiteral(
            "QMainWindow, QDockWidget, QWidget { background: #F4F7FB; color: #102033; }"
            "QPlainTextEdit, QTableView, QTableWidget, QTreeView { background: #FFFFFF; color: #111827; gridline-color: #D5DDEA; border: 1px solid #D8E0EB; }"
            "QHeaderView::section { background: #E9EEF6; color: #0F172A; border: 1px solid #D8E0EB; padding: 5px; }"
            "QToolBar { background: #E9EEF6; border: 0; spacing: 6px; padding: 6px; }"
            "QDockWidget::title { background: #E9EEF6; color: #0F172A; padding: 8px; font-weight: 600; }"
            "QMenuBar, QMenu { background: #FFFFFF; color: #0F172A; }"
            "QPushButton { background: #E2E8F0; color: #0F172A; border: 1px solid #CBD5E1; border-radius: 8px; padding: 6px 12px; }"
            "QPushButton:hover { background: #D6E2F2; }"
            "QLineEdit, QComboBox, QSpinBox { background: #FFFFFF; color: #0F172A; border: 1px solid #CBD5E1; border-radius: 6px; padding: 4px 6px; }"
            "QTabWidget::pane { border: 1px solid #D8E0EB; background: #FFFFFF; }"
            "QTabBar::tab { background: #E9EEF6; color: #334155; padding: 8px 14px; border-top-left-radius: 8px; border-top-right-radius: 8px; margin-right: 2px; }"
            "QTabBar::tab:selected { background: #FFFFFF; color: #0F172A; font-weight: 600; }"
            "QGroupBox { border: 1px solid #D8E0EB; border-radius: 10px; margin-top: 10px; padding-top: 10px; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"));
    } else {
        application->setStyleSheet(QStringLiteral(
            "QMainWindow, QDockWidget, QWidget { background: #0B1220; color: #E5EDF8; }"
            "QPlainTextEdit, QTableView, QTableWidget, QTreeView { background: #111A2B; color: #E5EDF8; gridline-color: #2B3A55; border: 1px solid #22304A; }"
            "QHeaderView::section { background: #172238; color: #E5EDF8; border: 1px solid #2B3A55; padding: 5px; }"
            "QToolBar { background: #131D31; border: 0; spacing: 6px; padding: 6px; }"
            "QDockWidget::title { background: #131D31; color: #E5EDF8; padding: 8px; font-weight: 600; }"
            "QMenuBar, QMenu { background: #0E1728; color: #E5EDF8; }"
            "QPushButton { background: #172238; color: #E5EDF8; border: 1px solid #334155; border-radius: 8px; padding: 6px 12px; }"
            "QPushButton:hover { background: #22304A; }"
            "QLineEdit, QComboBox, QSpinBox { background: #0F172A; color: #E5EDF8; border: 1px solid #334155; border-radius: 6px; padding: 4px 6px; }"
            "QTabWidget::pane { border: 1px solid #22304A; background: #0F172A; }"
            "QTabBar::tab { background: #172238; color: #AFC0DA; padding: 8px 14px; border-top-left-radius: 8px; border-top-right-radius: 8px; margin-right: 2px; }"
            "QTabBar::tab:selected { background: #0F172A; color: #F8FBFF; font-weight: 600; }"
            "QTreeView::item:selected, QTableView::item:selected, QTableWidget::item:selected { background: #1E3A5F; color: #F8FBFF; }"
            "QGroupBox { border: 1px solid #22304A; border-radius: 10px; margin-top: 10px; padding-top: 10px; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"));
    }

    refreshActionIcons();
}

void MainWindow::loadSettings() {
    QSettings settings(QStringLiteral("Core85"), QStringLiteral("Core85"));

    currentTheme_ = settings.value(QStringLiteral("appearance/theme"), QStringLiteral("dark")).toString();
    applyTheme(currentTheme_);

    recentFiles_ = settings.value(QStringLiteral("recent/files")).toStringList();
    recentFolders_ = settings.value(QStringLiteral("recent/folders")).toStringList();

    const QString workspace = settings.value(QStringLiteral("workspace/root")).toString();
    if (!workspace.isEmpty() && QFileInfo(workspace).isDir()) {
        setWorkspaceRoot(workspace);
    } else {
        setWorkspaceRoot(QString());
    }

    const QStringList tabs = settings.value(QStringLiteral("session/openTabs")).toStringList();
    for (const QString& path : tabs) {
        if (QFileInfo::exists(path)) {
            openTextFileInTab(path, false);
        }
    }

    const QString currentTabPath = settings.value(QStringLiteral("session/currentTab")).toString();
    if (!currentTabPath.isEmpty()) {
        const int index = findEditorTabByPath(currentTabPath);
        if (index >= 0) {
            editorTabs_->setCurrentIndex(index);
        }
    } else if (editorTabs_->count() > 0) {
        editorTabs_->setCurrentIndex(0);
    }

    restoreGeometry(settings.value(QStringLiteral("window/geometry")).toByteArray());
    restoreState(settings.value(QStringLiteral("window/state")).toByteArray());
}

void MainWindow::saveSettings() const {
    QSettings settings(QStringLiteral("Core85"), QStringLiteral("Core85"));
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("window/state"), saveState());
    settings.setValue(QStringLiteral("appearance/theme"), currentTheme_);
    settings.setValue(QStringLiteral("workspace/root"), workspaceRootPath_);
    settings.setValue(QStringLiteral("recent/files"), recentFiles_);
    settings.setValue(QStringLiteral("recent/folders"), recentFolders_);
    settings.setValue(QStringLiteral("session/openTabs"), openTabFilePaths());
    settings.setValue(QStringLiteral("session/currentTab"), currentEditorFilePath());
}

void MainWindow::recordRecentFile(const QString& path) {
    if (path.isEmpty()) {
        return;
    }

    recentFiles_.removeAll(path);
    recentFiles_.prepend(path);
    while (recentFiles_.size() > 10) {
        recentFiles_.removeLast();
    }
    refreshRecentMenus();
}

void MainWindow::recordRecentFolder(const QString& path) {
    if (path.isEmpty()) {
        return;
    }

    recentFolders_.removeAll(path);
    recentFolders_.prepend(path);
    while (recentFolders_.size() > 10) {
        recentFolders_.removeLast();
    }
    refreshRecentMenus();
}

void MainWindow::refreshRecentMenus() {
    if (recentFilesMenu_ == nullptr || recentFoldersMenu_ == nullptr) {
        return;
    }

    recentFilesMenu_->clear();
    for (const QString& path : recentFiles_) {
        if (!QFileInfo::exists(path)) {
            continue;
        }
        auto* action = recentFilesMenu_->addAction(QFileInfo(path).fileName(), this, &MainWindow::openRecentFile);
        action->setData(path);
        action->setToolTip(path);
    }
    if (recentFilesMenu_->isEmpty()) {
        recentFilesMenu_->addAction(QStringLiteral("No recent files"))->setEnabled(false);
    }

    recentFoldersMenu_->clear();
    for (const QString& path : recentFolders_) {
        if (!QFileInfo(path).isDir()) {
            continue;
        }
        auto* action =
            recentFoldersMenu_->addAction(QFileInfo(path).fileName(), this, &MainWindow::openRecentFolder);
        action->setData(path);
        action->setToolTip(path);
    }
    if (recentFoldersMenu_->isEmpty()) {
        recentFoldersMenu_->addAction(QStringLiteral("No recent folders"))->setEnabled(false);
    }
}

QStringList MainWindow::openTabFilePaths() const {
    QStringList paths;
    for (int index = 0; index < editorTabs_->count(); ++index) {
        const QString path = editorFilePath(editorAt(index));
        if (!path.isEmpty()) {
            paths.append(path);
        }
    }
    return paths;
}

QSet<QString> MainWindow::diffRegisterFields(const Core85::CPUState& previous,
                                             const Core85::CPUState& current) const {
    QSet<QString> changed;
    const auto addIfChanged = [&changed](const QString& name, auto left, auto right) {
        if (left != right) {
            changed.insert(name);
        }
    };

    addIfChanged(QStringLiteral("A"), previous.a, current.a);
    addIfChanged(QStringLiteral("B"), previous.b, current.b);
    addIfChanged(QStringLiteral("C"), previous.c, current.c);
    addIfChanged(QStringLiteral("D"), previous.d, current.d);
    addIfChanged(QStringLiteral("E"), previous.e, current.e);
    addIfChanged(QStringLiteral("H"), previous.h, current.h);
    addIfChanged(QStringLiteral("L"), previous.l, current.l);
    addIfChanged(QStringLiteral("F"), previous.f, current.f);
    addIfChanged(QStringLiteral("BC"), previous.bc, current.bc);
    addIfChanged(QStringLiteral("DE"), previous.de, current.de);
    addIfChanged(QStringLiteral("HL"), previous.hl, current.hl);
    addIfChanged(QStringLiteral("SP"), previous.sp, current.sp);
    addIfChanged(QStringLiteral("PC"), previous.pc, current.pc);
    return changed;
}

QSet<quint16> MainWindow::diffMemory(const QByteArray& previous, const QByteArray& current) const {
    QSet<quint16> changed;
    const int limit = qMin(previous.size(), current.size());
    for (int index = 0; index < limit; ++index) {
        if (previous.at(index) != current.at(index)) {
            changed.insert(static_cast<quint16>(index));
        }
    }
    return changed;
}

QList<quint16> MainWindow::currentBreakpointAddresses() const {
    QList<quint16> addresses;
    if (loadedEditor_ == nullptr) {
        return addresses;
    }

    const QSet<int> lines = loadedEditor_->breakpointLines();
    for (const int line : lines) {
        if (lineToAddress_.contains(line)) {
            addresses.push_back(lineToAddress_.value(line));
        }
    }
    return addresses;
}

QTextDocument::FindFlags MainWindow::buildFindFlags(bool backward,
                                                    bool caseSensitive,
                                                    bool wholeWord) const {
    QTextDocument::FindFlags flags;
    if (backward) {
        flags |= QTextDocument::FindBackward;
    }
    if (caseSensitive) {
        flags |= QTextDocument::FindCaseSensitively;
    }
    if (wholeWord) {
        flags |= QTextDocument::FindWholeWords;
    }
    return flags;
}
