#include "gui/registerviewer.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QToolTip>
#include <QVBoxLayout>

#include "gui/value_utils.h"

namespace Core85::Gui {

namespace {

QLabel* makeFlagLabel(const QString& text, const QString& tooltip, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setAlignment(Qt::AlignCenter);
    label->setMinimumWidth(54);
    label->setFrameShape(QFrame::StyledPanel);
    label->setToolTip(tooltip);
    return label;
}

}  // namespace

RegisterViewer::RegisterViewer(QWidget* parent)
    : QWidget(parent),
      table_(new QTableWidget(this)),
      displayModeCombo_(new QComboBox(this)),
      copySnapshotButton_(new QPushButton(QStringLiteral("Copy Snapshot"), this)),
      controlStateLabel_(new QLabel(this)),
      signFlag_(makeFlagLabel(QStringLiteral("S"),
                              QStringLiteral("Sign flag: set when the most significant bit of the result is 1."),
                              this)),
      zeroFlag_(makeFlagLabel(QStringLiteral("Z"),
                              QStringLiteral("Zero flag: set when the result of an operation becomes zero."),
                              this)),
      auxCarryFlag_(makeFlagLabel(
          QStringLiteral("AC"),
          QStringLiteral("Auxiliary carry: set when there is a carry from bit 3 to bit 4."),
          this)),
      parityFlag_(makeFlagLabel(QStringLiteral("P"),
                                QStringLiteral("Parity flag: set when the result contains an even number of 1 bits."),
                                this)),
      carryFlag_(makeFlagLabel(QStringLiteral("CY"),
                               QStringLiteral("Carry flag: set when an arithmetic operation overflows 8 bits."),
                               this)) {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(10);

    auto* headerLayout = new QGridLayout();
    auto* title = new QLabel(QStringLiteral("Registers"), this);
    title->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 15px;"));
    headerLayout->addWidget(title, 0, 0);

    displayModeCombo_->addItems(
        {QStringLiteral("Hex view"), QStringLiteral("Decimal view"), QStringLiteral("Binary view")});
    displayModeCombo_->setToolTip(QStringLiteral("Choose how register values are displayed."));
    headerLayout->addWidget(displayModeCombo_, 0, 1);

    copySnapshotButton_->setToolTip(QStringLiteral("Copy the current CPU register snapshot to the clipboard."));
    headerLayout->addWidget(copySnapshotButton_, 0, 2);
    headerLayout->setColumnStretch(0, 1);
    rootLayout->addLayout(headerLayout);

    controlStateLabel_->setWordWrap(true);
    controlStateLabel_->setStyleSheet(QStringLiteral("color: palette(mid);"));
    rootLayout->addWidget(controlStateLabel_);

    table_->setColumnCount(3);
    table_->setHorizontalHeaderLabels(
        {QStringLiteral("Register"), QStringLiteral("Primary"), QStringLiteral("Alternate")});
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->setAlternatingRowColors(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setToolTip(QStringLiteral("Double-click a value to edit it while execution is paused."));
    rootLayout->addWidget(table_, 1);

    auto* flagsTitle = new QLabel(QStringLiteral("Flags"), this);
    flagsTitle->setStyleSheet(QStringLiteral("font-weight: 700;"));
    rootLayout->addWidget(flagsTitle);

    auto* flagsLayout = new QGridLayout();
    flagsLayout->setHorizontalSpacing(8);
    flagsLayout->addWidget(signFlag_, 0, 0);
    flagsLayout->addWidget(zeroFlag_, 0, 1);
    flagsLayout->addWidget(auxCarryFlag_, 0, 2);
    flagsLayout->addWidget(parityFlag_, 0, 3);
    flagsLayout->addWidget(carryFlag_, 0, 4);
    rootLayout->addLayout(flagsLayout);

    connect(table_, &QTableWidget::cellChanged, this, &RegisterViewer::handleCellChanged);
    connect(copySnapshotButton_, &QPushButton::clicked, this, &RegisterViewer::copySnapshot);
    connect(displayModeCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        displayMode_ = index == 1 ? DisplayMode::Decimal
                                  : (index == 2 ? DisplayMode::Binary : DisplayMode::Hex);
        setState(lastState_, QSet<QString>{});
    });

    setInteractive(true);
}

void RegisterViewer::setState(const Core85::CPUState& state, const QSet<QString>& changedFields) {
    updating_ = true;
    lastState_ = state;
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

    controlStateLabel_->setText(
        QStringLiteral("Control state: PC %1, SP %2, HL %3, cycles %4")
            .arg(formatHex16(state.pc))
            .arg(formatHex16(state.sp))
            .arg(formatHex16(state.hl))
            .arg(state.elapsedCycles));
    updating_ = false;
}

void RegisterViewer::setInteractive(bool enabled) {
    table_->setEditTriggers(enabled ? QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed
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

void RegisterViewer::copySnapshot() {
    QStringList lines;
    lines << QStringLiteral("A=%1 B=%2 C=%3 D=%4 E=%5 H=%6 L=%7 F=%8")
                 .arg(formatHex8(lastState_.a))
                 .arg(formatHex8(lastState_.b))
                 .arg(formatHex8(lastState_.c))
                 .arg(formatHex8(lastState_.d))
                 .arg(formatHex8(lastState_.e))
                 .arg(formatHex8(lastState_.h))
                 .arg(formatHex8(lastState_.l))
                 .arg(formatHex8(lastState_.f));
    lines << QStringLiteral("BC=%1 DE=%2 HL=%3 SP=%4 PC=%5")
                 .arg(formatHex16(lastState_.bc))
                 .arg(formatHex16(lastState_.de))
                 .arg(formatHex16(lastState_.hl))
                 .arg(formatHex16(lastState_.sp))
                 .arg(formatHex16(lastState_.pc));
    lines << QStringLiteral("Flags: S=%1 Z=%2 AC=%3 P=%4 CY=%5")
                 .arg((lastState_.f & 0x80U) != 0U ? 1 : 0)
                 .arg((lastState_.f & 0x40U) != 0U ? 1 : 0)
                 .arg((lastState_.f & 0x10U) != 0U ? 1 : 0)
                 .arg((lastState_.f & 0x04U) != 0U ? 1 : 0)
                 .arg((lastState_.f & 0x01U) != 0U ? 1 : 0);
    QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
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
    auto* primaryItem = table_->item(row, 1);
    auto* secondaryItem = table_->item(row, 2);

    if (nameItem == nullptr) {
        nameItem = new QTableWidgetItem();
        table_->setItem(row, 0, nameItem);
    }
    if (primaryItem == nullptr) {
        primaryItem = new QTableWidgetItem();
        table_->setItem(row, 1, primaryItem);
    }
    if (secondaryItem == nullptr) {
        secondaryItem = new QTableWidgetItem();
        table_->setItem(row, 2, secondaryItem);
    }

    nameItem->setText(data.name);
    nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    primaryItem->setText(primaryValueForRow(data));
    secondaryItem->setText(secondaryValueForRow(data));

    const Qt::ItemFlags editableFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable |
                                        (data.editable ? Qt::ItemIsEditable : Qt::NoItemFlags);
    primaryItem->setFlags(editableFlags);
    secondaryItem->setFlags(editableFlags);
    primaryItem->setToolTip(QStringLiteral("Primary display mode value for %1").arg(data.name));
    secondaryItem->setToolTip(QStringLiteral("Alternate display mode value for %1").arg(data.name));

    const bool darkTheme = table_->palette().base().color().lightness() < 128;
    const QColor background = changed
                                  ? (darkTheme ? QColor(QStringLiteral("#6B5321"))
                                               : QColor(QStringLiteral("#FEF3C7")))
                                  : table_->palette().base().color();
    const QColor foreground = table_->palette().text().color();
    nameItem->setBackground(background);
    primaryItem->setBackground(background);
    secondaryItem->setBackground(background);
    nameItem->setForeground(foreground);
    primaryItem->setForeground(foreground);
    secondaryItem->setForeground(foreground);
}

QString RegisterViewer::primaryValueForRow(const RegisterRow& data) const {
    switch (displayMode_) {
        case DisplayMode::Decimal:
            return formatUnsigned(data.value);
        case DisplayMode::Binary:
            return formatBinary(data.value, data.bits);
        case DisplayMode::Hex:
        default:
            return data.bits == 8 ? formatHex8(static_cast<quint8>(data.value)) : formatHex16(data.value);
    }
}

QString RegisterViewer::secondaryValueForRow(const RegisterRow& data) const {
    switch (displayMode_) {
        case DisplayMode::Decimal:
            return data.bits == 8 ? formatHex8(static_cast<quint8>(data.value)) : formatHex16(data.value);
        case DisplayMode::Binary:
            return data.bits == 8 ? formatHex8(static_cast<quint8>(data.value)) : formatHex16(data.value);
        case DisplayMode::Hex:
        default:
            return formatUnsigned(data.value);
    }
}

QString RegisterViewer::formatBinary(quint16 value, int bits) const {
    QString output;
    output.reserve(bits + (bits / 4));
    for (int bit = bits - 1; bit >= 0; --bit) {
        if (bit != bits - 1 && ((bit + 1) % 4 == 0)) {
            output.append(QLatin1Char(' '));
        }
        output.append((value & (1U << bit)) != 0U ? QLatin1Char('1') : QLatin1Char('0'));
    }
    return output;
}

}  // namespace Core85::Gui
