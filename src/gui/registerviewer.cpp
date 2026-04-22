#include "gui/registerviewer.h"

#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QToolTip>
#include <QVBoxLayout>

#include "gui/value_utils.h"

namespace Core85::Gui {

namespace {

QLabel* makeFlagLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setAlignment(Qt::AlignCenter);
    label->setMinimumWidth(52);
    label->setFrameShape(QFrame::StyledPanel);
    return label;
}

}  // namespace

RegisterViewer::RegisterViewer(QWidget* parent)
    : QWidget(parent),
      table_(new QTableWidget(this)),
      signFlag_(makeFlagLabel(QStringLiteral("S"), this)),
      zeroFlag_(makeFlagLabel(QStringLiteral("Z"), this)),
      auxCarryFlag_(makeFlagLabel(QStringLiteral("AC"), this)),
      parityFlag_(makeFlagLabel(QStringLiteral("P"), this)),
      carryFlag_(makeFlagLabel(QStringLiteral("CY"), this)) {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(8);

    auto* title = new QLabel(QStringLiteral("Registers"), this);
    title->setStyleSheet(QStringLiteral("font-weight: 700;"));
    rootLayout->addWidget(title);

    table_->setColumnCount(3);
    table_->setHorizontalHeaderLabels(
        {QStringLiteral("Register"), QStringLiteral("Hex"), QStringLiteral("Unsigned")});
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->setAlternatingRowColors(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    rootLayout->addWidget(table_, 1);

    auto* flagsTitle = new QLabel(QStringLiteral("Flags"), this);
    flagsTitle->setStyleSheet(QStringLiteral("font-weight: 700;"));
    rootLayout->addWidget(flagsTitle);

    auto* flagsLayout = new QGridLayout();
    flagsLayout->addWidget(signFlag_, 0, 0);
    flagsLayout->addWidget(zeroFlag_, 0, 1);
    flagsLayout->addWidget(auxCarryFlag_, 0, 2);
    flagsLayout->addWidget(parityFlag_, 0, 3);
    flagsLayout->addWidget(carryFlag_, 0, 4);
    rootLayout->addLayout(flagsLayout);

    connect(table_, &QTableWidget::cellChanged, this, &RegisterViewer::handleCellChanged);
}

void RegisterViewer::setState(const Core85::CPUState& state, const QSet<QString>& changedFields) {
    updating_ = true;
    rebuildRows(state);

    table_->setRowCount(rows_.size());
    for (int index = 0; index < rows_.size(); ++index) {
        updateRow(index, rows_.at(index), changedFields.contains(rows_.at(index).name));
    }

    updateFlagIndicator(signFlag_, QStringLiteral("S"), (state.f & 0x80U) != 0U);
    updateFlagIndicator(zeroFlag_, QStringLiteral("Z"), (state.f & 0x40U) != 0U);
    updateFlagIndicator(auxCarryFlag_, QStringLiteral("AC"), (state.f & 0x10U) != 0U);
    updateFlagIndicator(parityFlag_, QStringLiteral("P"), (state.f & 0x04U) != 0U);
    updateFlagIndicator(carryFlag_, QStringLiteral("CY"), (state.f & 0x01U) != 0U);
    updating_ = false;
}

void RegisterViewer::setInteractive(bool enabled) {
    table_->setEditTriggers(enabled ? QAbstractItemView::DoubleClicked |
                                          QAbstractItemView::EditKeyPressed |
                                          QAbstractItemView::SelectedClicked
                                    : QAbstractItemView::NoEditTriggers);
}

void RegisterViewer::handleCellChanged(int row, int column) {
    if (updating_ || row < 0 || row >= rows_.size() || column == 0) {
        return;
    }

    const RegisterRow& rowData = rows_.at(row);
    if (!rowData.editable) {
        return;
    }

    const auto* item = table_->item(row, column);
    if (item == nullptr) {
        return;
    }

    const auto value = parseUnsignedValue(item->text(), rowData.bits);
    if (!value.has_value()) {
        emit invalidInputRequested(
            table_->viewport()->mapToGlobal(table_->visualItemRect(item).center()),
            QStringLiteral("Invalid %1-bit value for %2").arg(rowData.bits).arg(rowData.name));
        updating_ = true;
        updateRow(row, rowData, false);
        updating_ = false;
        return;
    }

    emit registerWriteRequested(rowData.name, *value);
}

void RegisterViewer::rebuildRows(const Core85::CPUState& state) {
    rows_ = {
        {QStringLiteral("A"), state.a, 8, true},
        {QStringLiteral("B"), state.b, 8, true},
        {QStringLiteral("C"), state.c, 8, true},
        {QStringLiteral("D"), state.d, 8, true},
        {QStringLiteral("E"), state.e, 8, true},
        {QStringLiteral("H"), state.h, 8, true},
        {QStringLiteral("L"), state.l, 8, true},
        {QStringLiteral("F"), state.f, 8, true},
        {QStringLiteral("BC"), state.bc, 16, true},
        {QStringLiteral("DE"), state.de, 16, true},
        {QStringLiteral("HL"), state.hl, 16, true},
        {QStringLiteral("SP"), state.sp, 16, true},
        {QStringLiteral("PC"), state.pc, 16, true},
    };
}

void RegisterViewer::updateFlagIndicator(QLabel* label, const QString& name, bool enabled) {
    const bool darkTheme = palette().base().color().lightness() < 128;
    const QString disabledStyle = darkTheme
                                      ? QStringLiteral("background: #1F2937; color: #D1D5DB;")
                                      : QStringLiteral("background: #E5E7EB; color: #374151;");
    label->setText(QStringLiteral("%1: %2").arg(name).arg(enabled ? QStringLiteral("1")
                                                                  : QStringLiteral("0")));
    label->setStyleSheet(enabled ? QStringLiteral("background: #BBF7D0; color: #14532D;")
                                 : disabledStyle);
}

void RegisterViewer::updateRow(int row, const RegisterRow& data, bool changed) {
    auto* nameItem = table_->item(row, 0);
    auto* hexItem = table_->item(row, 1);
    auto* decItem = table_->item(row, 2);

    if (nameItem == nullptr) {
        nameItem = new QTableWidgetItem();
        table_->setItem(row, 0, nameItem);
    }
    if (hexItem == nullptr) {
        hexItem = new QTableWidgetItem();
        table_->setItem(row, 1, hexItem);
    }
    if (decItem == nullptr) {
        decItem = new QTableWidgetItem();
        table_->setItem(row, 2, decItem);
    }

    nameItem->setText(data.name);
    nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    hexItem->setText(data.bits == 8 ? formatHex8(static_cast<quint8>(data.value))
                                    : formatHex16(data.value));
    decItem->setText(formatUnsigned(data.value));

    const Qt::ItemFlags editableFlags =
        Qt::ItemIsEnabled | Qt::ItemIsSelectable | (data.editable ? Qt::ItemIsEditable : Qt::NoItemFlags);
    hexItem->setFlags(editableFlags);
    decItem->setFlags(editableFlags);

    const bool darkTheme = table_->palette().base().color().lightness() < 128;
    const QColor background = changed
                                  ? (darkTheme ? QColor(QStringLiteral("#5B4A16"))
                                               : QColor(QStringLiteral("#FEF3C7")))
                                  : table_->palette().base().color();
    const QColor foreground = table_->palette().text().color();
    nameItem->setBackground(background);
    hexItem->setBackground(background);
    decItem->setBackground(background);
    nameItem->setForeground(foreground);
    hexItem->setForeground(foreground);
    decItem->setForeground(foreground);
}

}  // namespace Core85::Gui
