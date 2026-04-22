#include "mainwindow.h"

#include <QActionGroup>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSettings>
#include <QStatusBar>
#include <QTextStream>
#include <QThread>
#include <QToolBar>
#include <QToolTip>

#include "gui/codeeditor.h"
#include "gui/emulatorcontroller.h"
#include "gui/iopanel.h"
#include "gui/memoryviewer.h"
#include "gui/registerviewer.h"
#include "gui/value_utils.h"

namespace {

QString errorToString(const Core85::AssemblerError& error) {
    return QStringLiteral("Line %1: %2 (%3)")
        .arg(error.line)
        .arg(QString::fromStdString(error.message))
        .arg(QString::fromStdString(error.token));
}

}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Core85"));
    resize(1440, 900);

    buildUi();
    wireController();
    loadSettings();
    refreshStatusBar();
    requestControllerSnapshot();
}

MainWindow::~MainWindow() {
    saveSettings();
    emulatorThread_.quit();
    emulatorThread_.wait();
}

void MainWindow::assembleAndLoad() {
    clearAssemblerErrors();
    const Core85::AssemblyResult result =
        assembler_.assemble(codeEditor_->toPlainText().toStdString());

    if (!result.success) {
        showAssemblerErrors(result.errors);
        return;
    }

    QByteArray programBytes;
    programBytes.resize(static_cast<int>(result.bytes.size()));
    for (std::size_t index = 0; index < result.bytes.size(); ++index) {
        programBytes[static_cast<int>(index)] = static_cast<char>(result.bytes[index]);
    }

    rebuildSourceMappings(result.sourceMap);
    syncBreakpointAddresses();

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
}

void MainWindow::openSourceFile() {
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Open Assembly Source"),
        currentFilePath_,
        QStringLiteral("Assembly Files (*.asm);;All Files (*)"));
    if (!path.isEmpty()) {
        loadSourceFromPath(path);
    }
}

void MainWindow::saveSourceFile() {
    if (currentFilePath_.isEmpty() || currentFilePath_.endsWith(QStringLiteral(".hex"), Qt::CaseInsensitive)) {
        saveSourceFileAs();
        return;
    }

    saveSourceToPath(currentFilePath_);
}

void MainWindow::saveSourceFileAs() {
    const QString path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Save Assembly Source"),
        currentFilePath_.isEmpty() ? QStringLiteral("program.asm") : currentFilePath_,
        QStringLiteral("Assembly Files (*.asm);;All Files (*)"));
    if (!path.isEmpty()) {
        saveSourceToPath(path);
    }
}

void MainWindow::loadHexFile() {
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Load Intel HEX"),
        currentFilePath_,
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
    memoryViewer_->setInteractive(executionState_ != Core85::Gui::ExecutionState::Running);
    ioPanel_->setPortSnapshots(snapshot.inputPorts, snapshot.outputPorts);

    if (addressToLine_.contains(snapshot.cpuState.pc)) {
        codeEditor_->setCurrentExecutionLine(addressToLine_.value(snapshot.cpuState.pc));
    } else {
        codeEditor_->setCurrentExecutionLine(-1);
    }

    if (autoFollowAction_->isChecked()) {
        memoryViewer_->scrollToAddress(snapshot.cpuState.pc);
    }

    refreshStatusBar();
}

void MainWindow::handleExecutionStateChanged(Core85::Gui::ExecutionState state) {
    executionState_ = state;
    codeEditor_->setReadOnly(state == Core85::Gui::ExecutionState::Running);
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
    syncBreakpointAddresses();
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
    saveProjectMetadata();
}

void MainWindow::applyLightTheme() {
    applyTheme(QStringLiteral("light"));
}

void MainWindow::applyDarkTheme() {
    applyTheme(QStringLiteral("dark"));
}

void MainWindow::buildUi() {
    createMenus();
    createToolbar();
    createDocks();

    pcStatusLabel_ = new QLabel(QStringLiteral("PC: 0x0000"), this);
    cyclesStatusLabel_ = new QLabel(QStringLiteral("T-States: 0"), this);
    stateStatusLabel_ = new QLabel(QStringLiteral("State: Paused"), this);
    messageStatusLabel_ = new QLabel(QStringLiteral("Ready"), this);
    statusBar()->addWidget(messageStatusLabel_, 1);
    statusBar()->addPermanentWidget(pcStatusLabel_);
    statusBar()->addPermanentWidget(cyclesStatusLabel_);
    statusBar()->addPermanentWidget(stateStatusLabel_);
}

void MainWindow::createMenus() {
    auto* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    auto* openAction = fileMenu->addAction(QStringLiteral("&Open..."), this, &MainWindow::openSourceFile);
    openAction->setShortcut(QKeySequence::Open);
    auto* saveAction = fileMenu->addAction(QStringLiteral("&Save"), this, &MainWindow::saveSourceFile);
    saveAction->setShortcut(QKeySequence::Save);
    auto* saveAsAction =
        fileMenu->addAction(QStringLiteral("Save &As..."), this, &MainWindow::saveSourceFileAs);
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("Load &HEX..."), this, &MainWindow::loadHexFile);
    fileMenu->addSeparator();
    auto* exitAction = fileMenu->addAction(QStringLiteral("E&xit"), this, &QWidget::close);
    exitAction->setShortcut(QKeySequence::Quit);

    viewMenu_ = menuBar()->addMenu(QStringLiteral("&View"));

    auto* preferencesMenu = menuBar()->addMenu(QStringLiteral("&Preferences"));
    auto* themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);
    auto* lightAction = preferencesMenu->addAction(QStringLiteral("&Light Theme"), this, &MainWindow::applyLightTheme);
    auto* darkAction = preferencesMenu->addAction(QStringLiteral("&Dark Theme"), this, &MainWindow::applyDarkTheme);
    lightAction->setCheckable(true);
    darkAction->setCheckable(true);
    themeGroup->addAction(lightAction);
    themeGroup->addAction(darkAction);
    darkAction->setChecked(true);
}

void MainWindow::createToolbar() {
    executionToolBar_ = addToolBar(QStringLiteral("Execution Controls"));
    executionToolBar_->setObjectName(QStringLiteral("execution_toolbar"));
    executionToolBar_->setMovable(false);
    if (viewMenu_ != nullptr) {
        viewMenu_->addAction(executionToolBar_->toggleViewAction());
        viewMenu_->addSeparator();
    }

    assembleAction_ =
        executionToolBar_->addAction(QStringLiteral("Assemble & Load"), this, &MainWindow::assembleAndLoad);
    runAction_ = executionToolBar_->addAction(QStringLiteral("Run"), [this]() {
        QMetaObject::invokeMethod(emulatorController_, &Core85::Gui::EmulatorController::run, Qt::QueuedConnection);
    });
    pauseAction_ = executionToolBar_->addAction(QStringLiteral("Pause"), [this]() {
        QMetaObject::invokeMethod(emulatorController_, &Core85::Gui::EmulatorController::pause, Qt::QueuedConnection);
    });
    stepIntoAction_ = executionToolBar_->addAction(QStringLiteral("Step Into"), [this]() {
        QMetaObject::invokeMethod(
            emulatorController_, &Core85::Gui::EmulatorController::stepInto, Qt::QueuedConnection);
    });
    stepOverAction_ = executionToolBar_->addAction(QStringLiteral("Step Over"), [this]() {
        QMetaObject::invokeMethod(
            emulatorController_, &Core85::Gui::EmulatorController::stepOver, Qt::QueuedConnection);
    });
    resetAction_ = executionToolBar_->addAction(QStringLiteral("Reset"), [this]() {
        QMetaObject::invokeMethod(
            emulatorController_, &Core85::Gui::EmulatorController::resetToLoadedProgram, Qt::QueuedConnection);
    });

    executionToolBar_->addSeparator();
    autoFollowAction_ = executionToolBar_->addAction(QStringLiteral("Follow PC"));
    autoFollowAction_->setCheckable(true);
    autoFollowAction_->setChecked(true);

    executionToolBar_->addSeparator();
    executionToolBar_->addWidget(new QLabel(QStringLiteral("Speed"), executionToolBar_));
    speedCombo_ = new QComboBox(executionToolBar_);
    speedCombo_->addItems({QStringLiteral("1 instr"),
                           QStringLiteral("10 instr"),
                           QStringLiteral("100 instr"),
                           QStringLiteral("1000 instr"),
                           QStringLiteral("Max")});
    speedCombo_->setCurrentIndex(4);
    executionToolBar_->addWidget(speedCombo_);
}

void MainWindow::createDocks() {
    codeEditor_ = new Core85::Gui::CodeEditor(this);
    setCentralWidget(codeEditor_);

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

    errorDock_ = new QDockWidget(QStringLiteral("Error Console"), this);
    errorDock_->setObjectName(QStringLiteral("error_dock"));
    errorConsole_ = new QPlainTextEdit(errorDock_);
    errorConsole_->setReadOnly(true);
    errorDock_->setWidget(errorConsole_);
    addDockWidget(Qt::BottomDockWidgetArea, errorDock_);
    tabifyDockWidget(ioDock_, errorDock_);

    if (viewMenu_ != nullptr) {
        viewMenu_->addAction(registersDock_->toggleViewAction());
        viewMenu_->addAction(memoryDock_->toggleViewAction());
        viewMenu_->addAction(ioDock_->toggleViewAction());
        viewMenu_->addAction(errorDock_->toggleViewAction());
    }
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
    connect(codeEditor_, &Core85::Gui::CodeEditor::breakpointToggled, this, &MainWindow::handleBreakpointToggled);
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
    stateStatusLabel_->setText(
        QStringLiteral("State: %1").arg(Core85::Gui::executionStateToString(executionState_)));
}

void MainWindow::clearAssemblerErrors() {
    errorConsole_->clear();
    codeEditor_->clearProblemMarkers();
    messageStatusLabel_->setText(QStringLiteral("Ready"));
}

void MainWindow::showAssemblerErrors(const std::vector<Core85::AssemblerError>& errors) {
    if (errors.empty()) {
        return;
    }

    for (const auto& error : errors) {
        errorConsole_->appendPlainText(errorToString(error));
    }

    const auto& first = errors.front();
    codeEditor_->setErrorLine(static_cast<int>(first.line), QString::fromStdString(first.message));
    messageStatusLabel_->setText(errorToString(first));
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
    if (currentFilePath_.isEmpty()) {
        setWindowTitle(QStringLiteral("Core85"));
        return;
    }
    setWindowTitle(QStringLiteral("Core85 - %1").arg(QFileInfo(currentFilePath_).fileName()));
}

bool MainWindow::saveSourceToPath(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("Save Failed"), file.errorString());
        return false;
    }

    QTextStream stream(&file);
    stream << codeEditor_->toPlainText();
    setCurrentFilePath(path);
    saveProjectMetadata();
    messageStatusLabel_->setText(QStringLiteral("Saved %1").arg(QFileInfo(path).fileName()));
    return true;
}

void MainWindow::loadSourceFromPath(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("Open Failed"), file.errorString());
        return;
    }

    QTextStream stream(&file);
    codeEditor_->setPlainText(stream.readAll());
    addressToLine_.clear();
    lineToAddress_.clear();
    codeEditor_->setCurrentExecutionLine(-1);
    setCurrentFilePath(path);
    loadProjectMetadata();
    clearAssemblerErrors();
    syncBreakpointAddresses();
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

    addressToLine_.clear();
    lineToAddress_.clear();
    codeEditor_->setCurrentExecutionLine(-1);
    clearAssemblerErrors();
    setCurrentFilePath(path);
    loadProjectMetadata();

    QMetaObject::invokeMethod(
        emulatorController_,
        [controller = emulatorController_, origin = result.origin, bytes]() {
            controller->loadProgram(origin, bytes);
        },
        Qt::QueuedConnection);

    messageStatusLabel_->setText(QStringLiteral("Loaded HEX at %1").arg(Core85::Gui::formatHex16(result.origin)));
}

QString MainWindow::projectMetadataPath() const {
    if (currentFilePath_.isEmpty()) {
        return {};
    }
    return currentFilePath_ + QStringLiteral(".core85project.json");
}

void MainWindow::loadProjectMetadata() {
    projectMetadata_ = {};
    const QString metadataPath = projectMetadataPath();
    if (!QFileInfo::exists(metadataPath)) {
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
    projectMetadata_.segmentPort = static_cast<quint8>(object.value(QStringLiteral("segmentPort")).toInt(2));
    ioPanel_->setProjectMetadata(projectMetadata_);
}

void MainWindow::saveProjectMetadata() const {
    const QString metadataPath = projectMetadataPath();
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
        {QStringLiteral("segmentPort"), projectMetadata_.segmentPort},
    };
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
}

void MainWindow::applyTheme(const QString& themeName) {
    currentTheme_ = themeName;
    auto* application = qobject_cast<QApplication*>(QCoreApplication::instance());
    if (application == nullptr) {
        return;
    }

    if (themeName == QStringLiteral("light")) {
        application->setStyleSheet(QStringLiteral(
            "QMainWindow, QDockWidget, QWidget { background: #F8FAFC; color: #0F172A; }"
            "QPlainTextEdit, QTableView, QTableWidget { background: #FFFFFF; color: #111827; gridline-color: #CBD5E1; }"
            "QHeaderView::section { background: #E2E8F0; color: #0F172A; border: 1px solid #CBD5E1; }"
            "QToolBar { background: #E2E8F0; border: 0; }"
            "QDockWidget::title { background: #E2E8F0; color: #0F172A; padding-left: 8px; }"
            "QMenuBar, QMenu { background: #FFFFFF; color: #0F172A; }"));
    } else {
        application->setStyleSheet(QStringLiteral(
            "QMainWindow, QDockWidget, QWidget { background: #111827; color: #E5E7EB; }"
            "QPlainTextEdit, QTableView, QTableWidget { background: #0F172A; color: #E5E7EB; gridline-color: #334155; }"
            "QHeaderView::section { background: #1E293B; color: #E5E7EB; border: 1px solid #334155; }"
            "QToolBar { background: #1F2937; border: 0; }"
            "QDockWidget::title { background: #1F2937; color: #E5E7EB; padding-left: 8px; }"
            "QMenuBar, QMenu { background: #111827; color: #E5E7EB; }"
            "QLineEdit, QComboBox, QSpinBox { background: #0F172A; color: #E5E7EB; border: 1px solid #334155; }"
            "QTabBar::tab { background: #1F2937; color: #D1D5DB; padding: 6px 10px; }"
            "QTabBar::tab:selected { background: #334155; color: #F8FAFC; }"));
    }
}

void MainWindow::loadSettings() {
    QSettings settings(QStringLiteral("Core85"), QStringLiteral("Core85"));
    restoreGeometry(settings.value(QStringLiteral("window/geometry")).toByteArray());
    restoreState(settings.value(QStringLiteral("window/state")).toByteArray());
    const QString theme = settings.value(QStringLiteral("appearance/theme"), QStringLiteral("dark")).toString();
    applyTheme(theme);
}

void MainWindow::saveSettings() const {
    QSettings settings(QStringLiteral("Core85"), QStringLiteral("Core85"));
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("window/state"), saveState());
    settings.setValue(QStringLiteral("appearance/theme"), currentTheme_);
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
    const QSet<int> lines = codeEditor_->breakpointLines();
    for (const int line : lines) {
        if (lineToAddress_.contains(line)) {
            addresses.push_back(lineToAddress_.value(line));
        }
    }
    return addresses;
}
