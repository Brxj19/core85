#include "gui/problemspanel.h"

#include <QHeaderView>
#include <QLabel>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace Core85::Gui {

ProblemsPanel::ProblemsPanel(QWidget* parent)
    : QWidget(parent), tree_(new QTreeWidget(this)), summaryLabel_(new QLabel(this)) {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(6);

    summaryLabel_->setText(QStringLiteral("No problems"));
    rootLayout->addWidget(summaryLabel_);

    tree_->setColumnCount(3);
    tree_->setHeaderLabels({QStringLiteral("Line"), QStringLiteral("Token"), QStringLiteral("Message")});
    tree_->setRootIsDecorated(false);
    tree_->setUniformRowHeights(true);
    tree_->header()->setStretchLastSection(true);
    tree_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    rootLayout->addWidget(tree_, 1);

    connect(tree_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item, int) {
        if (item == nullptr) {
            return;
        }
        emit problemActivated(item->data(0, Qt::UserRole).toInt());
    });
    connect(tree_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int) {
        if (item == nullptr) {
            return;
        }
        emit problemActivated(item->data(0, Qt::UserRole).toInt());
    });
}

void ProblemsPanel::setProblems(const std::vector<Core85::AssemblerError>& problems) {
    tree_->clear();
    summaryLabel_->setText(problems.empty() ? QStringLiteral("No problems")
                                            : QStringLiteral("%1 problem(s)").arg(problems.size()));

    for (const auto& problem : problems) {
        auto* item = new QTreeWidgetItem(tree_);
        item->setText(0, QString::number(problem.line));
        item->setText(1, QString::fromStdString(problem.token));
        item->setText(2, friendlyMessage(problem));
        item->setData(0, Qt::UserRole, static_cast<int>(problem.line));
    }
}

void ProblemsPanel::clearProblems() {
    tree_->clear();
    summaryLabel_->setText(QStringLiteral("No problems"));
}

bool ProblemsPanel::hasProblems() const {
    return tree_->topLevelItemCount() > 0;
}

QString ProblemsPanel::friendlyMessage(const Core85::AssemblerError& problem) const {
    const QString raw = QString::fromStdString(problem.message);

    if (raw.contains(QStringLiteral("Undefined"), Qt::CaseInsensitive)) {
        return QStringLiteral("Unknown symbol or label. Check spelling or declaration.");
    }
    if (raw.contains(QStringLiteral("duplicate"), Qt::CaseInsensitive)) {
        return QStringLiteral("This label or symbol was already defined earlier.");
    }
    if (raw.contains(QStringLiteral("mnemonic"), Qt::CaseInsensitive)) {
        return QStringLiteral("Unknown instruction or directive. Check the opcode spelling.");
    }
    if (raw.contains(QStringLiteral("operand"), Qt::CaseInsensitive)) {
        return QStringLiteral("The instruction arguments look invalid. Check register names and values.");
    }

    return raw;
}

}  // namespace Core85::Gui
