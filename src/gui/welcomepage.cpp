#include "gui/welcomepage.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace Core85::Gui {

WelcomePage::WelcomePage(QWidget* parent)
    : QWidget(parent) {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(36, 32, 36, 32);
    rootLayout->setSpacing(18);

    auto* eyebrow = new QLabel(QStringLiteral("Core85"), this);
    eyebrow->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 700; color: #3B82F6;"));
    rootLayout->addWidget(eyebrow, 0, Qt::AlignLeft);

    auto* title = new QLabel(QStringLiteral("8085 assembly, made easier to explore"), this);
    title->setWordWrap(true);
    title->setStyleSheet(QStringLiteral("font-size: 28px; font-weight: 750;"));
    rootLayout->addWidget(title);

    auto* subtitle = new QLabel(
        QStringLiteral("Write, assemble, step through, and inspect programs with the editor, registers, memory, and I/O panels working together."),
        this);
    subtitle->setWordWrap(true);
    subtitle->setStyleSheet(QStringLiteral("font-size: 14px; color: palette(mid);"));
    rootLayout->addWidget(subtitle);

    auto* actionsLayout = new QGridLayout();
    actionsLayout->setHorizontalSpacing(12);
    actionsLayout->setVerticalSpacing(12);

    auto makeActionButton = [this](const QIcon& icon, const QString& titleText, const QString& desc) {
        auto* button = new QPushButton(icon, titleText + QStringLiteral("\n") + desc, this);
        button->setMinimumHeight(72);
        button->setStyleSheet(QStringLiteral("text-align: left; padding: 10px 14px; font-size: 13px;"));
        return button;
    };

    auto* newButton = makeActionButton(style()->standardIcon(QStyle::SP_FileIcon),
                                       QStringLiteral("New Assembly File"),
                                       QStringLiteral("Start a blank .asm document"));
    auto* filesButton = makeActionButton(style()->standardIcon(QStyle::SP_DialogOpenButton),
                                         QStringLiteral("Open Files"),
                                         QStringLiteral("Load one or more source files"));
    auto* folderButton = makeActionButton(style()->standardIcon(QStyle::SP_DirOpenIcon),
                                          QStringLiteral("Open Folder"),
                                          QStringLiteral("Browse a workspace in the explorer"));
    auto* sampleButton = makeActionButton(style()->standardIcon(QStyle::SP_ComputerIcon),
                                          QStringLiteral("Open Sample Program"),
                                          QStringLiteral("Try a small demo immediately"));

    actionsLayout->addWidget(newButton, 0, 0);
    actionsLayout->addWidget(filesButton, 0, 1);
    actionsLayout->addWidget(folderButton, 1, 0);
    actionsLayout->addWidget(sampleButton, 1, 1);
    rootLayout->addLayout(actionsLayout);

    auto* tipsBox = new QGroupBox(QStringLiteral("Quick Tips"), this);
    auto* tipsLayout = new QVBoxLayout(tipsBox);
    tipsLayout->setSpacing(8);
    const QStringList tips{
        QStringLiteral("Use Ctrl+Shift+B to assemble and load the active file."),
        QStringLiteral("Use F5 to run, Shift+F5 to pause, F11 to step into, and F10 to step over."),
        QStringLiteral("Double-click register values or memory bytes to edit them when execution is paused."),
        QStringLiteral("Load a sample, then watch the register and memory panels while stepping."),
    };
    for (const QString& tip : tips) {
        auto* label = new QLabel(QStringLiteral("• %1").arg(tip), tipsBox);
        label->setWordWrap(true);
        tipsLayout->addWidget(label);
    }
    rootLayout->addWidget(tipsBox);
    rootLayout->addStretch();

    connect(newButton, &QPushButton::clicked, this, &WelcomePage::newFileRequested);
    connect(filesButton, &QPushButton::clicked, this, &WelcomePage::openFilesRequested);
    connect(folderButton, &QPushButton::clicked, this, &WelcomePage::openFolderRequested);
    connect(sampleButton, &QPushButton::clicked, this, &WelcomePage::openSampleRequested);
}

}  // namespace Core85::Gui
