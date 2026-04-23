#include "gui/findreplacebar.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace Core85::Gui {

FindReplaceBar::FindReplaceBar(QWidget* parent)
    : QWidget(parent),
      findInput_(new QLineEdit(this)),
      replaceInput_(new QLineEdit(this)),
      goToLineInput_(new QLineEdit(this)),
      caseSensitiveCheck_(new QCheckBox(QStringLiteral("Match Case"), this)),
      wholeWordCheck_(new QCheckBox(QStringLiteral("Whole Word"), this)) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(8);

    findInput_->setPlaceholderText(QStringLiteral("Find"));
    replaceInput_->setPlaceholderText(QStringLiteral("Replace"));
    goToLineInput_->setPlaceholderText(QStringLiteral("Go to line"));
    goToLineInput_->setMaximumWidth(110);

    auto* nextButton = new QPushButton(QStringLiteral("Next"), this);
    auto* prevButton = new QPushButton(QStringLiteral("Prev"), this);
    auto* replaceButton = new QPushButton(QStringLiteral("Replace"), this);
    auto* replaceAllButton = new QPushButton(QStringLiteral("Replace All"), this);
    auto* closeButton = new QPushButton(QStringLiteral("Close"), this);

    layout->addWidget(new QLabel(QStringLiteral("Find"), this));
    layout->addWidget(findInput_, 1);
    layout->addWidget(new QLabel(QStringLiteral("Replace"), this));
    layout->addWidget(replaceInput_, 1);
    layout->addWidget(caseSensitiveCheck_);
    layout->addWidget(wholeWordCheck_);
    layout->addWidget(prevButton);
    layout->addWidget(nextButton);
    layout->addWidget(replaceButton);
    layout->addWidget(replaceAllButton);
    layout->addSpacing(8);
    layout->addWidget(goToLineInput_);
    layout->addWidget(closeButton);

    connect(nextButton, &QPushButton::clicked, this, [this]() {
        emit findNextRequested(findText(), caseSensitiveCheck_->isChecked(), wholeWordCheck_->isChecked());
    });
    connect(prevButton, &QPushButton::clicked, this, [this]() {
        emit findPreviousRequested(findText(), caseSensitiveCheck_->isChecked(), wholeWordCheck_->isChecked());
    });
    connect(replaceButton, &QPushButton::clicked, this, [this]() {
        emit replaceOneRequested(
            findText(), replaceText(), caseSensitiveCheck_->isChecked(), wholeWordCheck_->isChecked());
    });
    connect(replaceAllButton, &QPushButton::clicked, this, [this]() {
        emit replaceAllRequested(
            findText(), replaceText(), caseSensitiveCheck_->isChecked(), wholeWordCheck_->isChecked());
    });
    connect(goToLineInput_, &QLineEdit::returnPressed, this, [this]() {
        bool ok = false;
        const int line = goToLineInput_->text().trimmed().toInt(&ok);
        if (ok && line > 0) {
            emit goToLineRequested(line);
        }
    });
    connect(closeButton, &QPushButton::clicked, this, &FindReplaceBar::closeRequested);
}

QString FindReplaceBar::findText() const {
    return findInput_->text();
}

QString FindReplaceBar::replaceText() const {
    return replaceInput_->text();
}

void FindReplaceBar::focusFind() {
    findInput_->setFocus();
    findInput_->selectAll();
}

void FindReplaceBar::focusGoToLine() {
    goToLineInput_->setFocus();
    goToLineInput_->selectAll();
}

void FindReplaceBar::setFindText(const QString& text) {
    findInput_->setText(text);
}

}  // namespace Core85::Gui
