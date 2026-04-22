#include "mainwindow.h"

#include <QLabel>
#include <QStatusBar>
#include <QVBoxLayout>

#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("Core85"));
    resize(1280, 800);

    auto* layout = new QVBoxLayout(ui->centralwidget);
    auto* placeholder = new QLabel(
        QStringLiteral(
            "Core85 GUI scaffold\n\n"
            "The emulator core is running with tests in place.\n"
            "The full editor, registers, memory viewer, and I/O panels\n"
            "will be added in the upcoming GUI phases."),
        ui->centralwidget);

    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setWordWrap(true);
    layout->addWidget(placeholder);

    statusBar()->showMessage(QStringLiteral("GUI scaffold loaded"));
}

MainWindow::~MainWindow() {
    delete ui;
}
