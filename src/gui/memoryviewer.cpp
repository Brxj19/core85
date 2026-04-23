#include "gui/memoryviewer.h"

#include <algorithm>

#include <QAbstractTableModel>
#include <QApplication>
#include <QComboBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableView>
#include <QVBoxLayout>
#include <QtGlobal>

#include "gui/value_utils.h"

namespace Core85::Gui {

class MemoryViewer::MemoryModel final : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit MemoryModel(QObject* parent = nullptr)
        : QAbstractTableModel(parent),
          memory_(65536, static_cast<char>(0xFF)) {
    }

    int rowCount(const QModelIndex& parent) const override {
        return parent.isValid() ? 0 : 4096;
    }

    int columnCount(const QModelIndex& parent) const override {
        return parent.isValid() ? 0 : 18;
    }

    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid()) {
            return {};
        }

        const int row = index.row();
        const int column = index.column();
        const quint16 rowBase = static_cast<quint16>(row * 16);

        if (role == Qt::DisplayRole || role == Qt::EditRole) {
            if (column == 0) {
                return formatHex16(rowBase);
            }

            if (column == 17) {
                QString ascii;
                ascii.reserve(16);
                for (int offset = 0; offset < 16; ++offset) {
                    const char value = memory_.at(rowBase + offset);
                    ascii.append((value >= 32 && value <= 126) ? QChar(value) : QChar('.'));
                }
                return ascii;
            }

            const quint16 address = static_cast<quint16>(rowBase + column - 1);
            return formatHex8(static_cast<quint8>(memory_.at(address)));
        }

        if (role == Qt::BackgroundRole && column > 0 && column < 17) {
            const quint16 address = static_cast<quint16>(rowBase + column - 1);
            if (changedAddresses_.contains(address)) {
                return darkTheme_ ? QColor(QStringLiteral("#5B4A16"))
                                  : QColor(QStringLiteral("#FEF3C7"));
            }
        }

        if (role == Qt::TextAlignmentRole) {
            return Qt::AlignCenter;
        }

        return {};
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
            return {};
        }

        if (section == 0) {
            return QStringLiteral("Addr");
        }
        if (section == 17) {
            return QStringLiteral("ASCII");
        }
        return QStringLiteral("%1").arg(section - 1, 2, 16, QLatin1Char('0')).toUpper();
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override {
        if (!index.isValid()) {
            return Qt::NoItemFlags;
        }

        Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        if (interactive_ && index.column() > 0 && index.column() < 17) {
            flags |= Qt::ItemIsEditable;
        }
        return flags;
    }

    bool setData(const QModelIndex& index, const QVariant& value, int role) override {
        if (role != Qt::EditRole || !index.isValid() || index.column() <= 0 || index.column() >= 17) {
            return false;
        }

        const auto parsed = parseUnsignedValue(value.toString(), 8);
        if (!parsed.has_value()) {
            return false;
        }

        const quint16 address = static_cast<quint16>(index.row() * 16 + index.column() - 1);
        memory_[address] = static_cast<char>(*parsed & 0xFFU);
        changedAddresses_.insert(address);
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::BackgroundRole});
        emit dataChanged(this->index(index.row(), 17), this->index(index.row(), 17), {Qt::DisplayRole});
        emit memoryWriteRequested(address, static_cast<quint8>(*parsed));
        return true;
    }

    void setMemorySnapshot(const QByteArray& memory, const QSet<quint16>& changedAddresses) {
        beginResetModel();
        memory_ = memory;
        changedAddresses_ = changedAddresses;
        darkTheme_ = QApplication::palette().base().color().lightness() < 128;
        endResetModel();
    }

    void setInteractive(bool enabled) {
        interactive_ = enabled;
    }

    QByteArray memory() const {
        return memory_;
    }

signals:
    void memoryWriteRequested(quint16 address, quint8 value);

private:
    QByteArray memory_;
    QSet<quint16> changedAddresses_{};
    bool interactive_ = true;
    bool darkTheme_ = true;
};

MemoryViewer::MemoryViewer(QWidget* parent)
    : QWidget(parent),
      model_(new MemoryModel(this)),
      table_(new QTableView(this)),
      addressInput_(new QLineEdit(this)),
      addressFormatCombo_(new QComboBox(this)),
      valueSearchInput_(new QLineEdit(this)),
      bookmarksCombo_(new QComboBox(this)),
      bookmarkButton_(new QPushButton(QStringLiteral("Bookmark"), this)) {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(8);

    auto* title = new QLabel(QStringLiteral("Memory"), this);
    title->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 15px;"));
    rootLayout->addWidget(title);

    auto* topControls = new QHBoxLayout();
    topControls->addWidget(new QLabel(QStringLiteral("Go to"), this));
    addressInput_->setPlaceholderText(QStringLiteral("0x0000"));
    addressInput_->setToolTip(QStringLiteral("Jump to an address in hex or decimal."));
    topControls->addWidget(addressInput_, 1);
    addressFormatCombo_->addItems({QStringLiteral("Hex"), QStringLiteral("Decimal")});
    addressFormatCombo_->setToolTip(QStringLiteral("Choose the default address format for the Go to field."));
    topControls->addWidget(addressFormatCombo_);

    auto addJumpButton = [&](const QString& text, auto slot) {
        auto* button = new QPushButton(text, this);
        button->setToolTip(QStringLiteral("Jump to %1").arg(text));
        connect(button, &QPushButton::clicked, this, slot);
        topControls->addWidget(button);
    };
    addJumpButton(QStringLiteral("PC"), &MemoryViewer::jumpToPc);
    addJumpButton(QStringLiteral("SP"), &MemoryViewer::jumpToSp);
    addJumpButton(QStringLiteral("HL"), &MemoryViewer::jumpToHl);
    rootLayout->addLayout(topControls);

    auto* searchControls = new QHBoxLayout();
    searchControls->addWidget(new QLabel(QStringLiteral("Find byte"), this));
    valueSearchInput_->setPlaceholderText(QStringLiteral("0x3F"));
    valueSearchInput_->setToolTip(QStringLiteral("Find the next byte value in memory."));
    searchControls->addWidget(valueSearchInput_, 1);
    auto* findNextButton = new QPushButton(QStringLiteral("Find Next"), this);
    searchControls->addWidget(findNextButton);
    searchControls->addSpacing(8);
    searchControls->addWidget(bookmarksCombo_, 1);
    bookmarksCombo_->setToolTip(QStringLiteral("Jump to a bookmarked address."));
    searchControls->addWidget(bookmarkButton_);
    rootLayout->addLayout(searchControls);

    table_->setModel(model_);
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table_->setAlternatingRowColors(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectItems);
    table_->setFont(QFont(QStringLiteral("Menlo"), 11));
    table_->setColumnWidth(0, 88);
    for (int column = 1; column < 17; ++column) {
        table_->setColumnWidth(column, 44);
    }
    table_->setColumnWidth(17, 180);
    table_->setToolTip(QStringLiteral("Hex bytes with ASCII preview. Double-click a byte to edit it while paused."));
    rootLayout->addWidget(table_, 1);

    connect(addressInput_, &QLineEdit::returnPressed, this, &MemoryViewer::handleGoToAddress);
    connect(findNextButton, &QPushButton::clicked, this, &MemoryViewer::handleFindValue);
    connect(bookmarkButton_, &QPushButton::clicked, this, &MemoryViewer::handleBookmarkCurrent);
    connect(bookmarksCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MemoryViewer::handleBookmarkSelected);
    connect(model_, &MemoryModel::memoryWriteRequested, this, &MemoryViewer::memoryWriteRequested);

    refreshBookmarkCombo();
}

void MemoryViewer::setMemorySnapshot(const QByteArray& memory, const QSet<quint16>& changedAddresses) {
    model_->setMemorySnapshot(memory, changedAddresses);
}

void MemoryViewer::scrollToAddress(quint16 address) {
    const int row = address / 16;
    const int column = (address % 16) + 1;
    const QModelIndex target = model_->index(row, column);
    table_->scrollTo(target, QAbstractItemView::PositionAtCenter);
    table_->setCurrentIndex(target);
    addressInput_->setText(addressFormatCombo_->currentIndex() == 0 ? formatHex16(address)
                                                                    : QString::number(address));
}

void MemoryViewer::setInteractive(bool enabled) {
    model_->setInteractive(enabled);
    table_->setEditTriggers(enabled ? QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed
                                    : QAbstractItemView::NoEditTriggers);
}

void MemoryViewer::setRegisterAnchors(quint16 pc, quint16 sp, quint16 hl) {
    pcAddress_ = pc;
    spAddress_ = sp;
    hlAddress_ = hl;
}

void MemoryViewer::handleGoToAddress() {
    const auto parsed = parseAddressInput(addressInput_->text());
    if (!parsed.has_value()) {
        emit invalidAddressRequested(QStringLiteral("Enter a valid 16-bit address in hex or decimal."));
        return;
    }
    scrollToAddress(*parsed);
}

void MemoryViewer::handleFindValue() {
    const auto parsed = parseValueInput(valueSearchInput_->text());
    if (!parsed.has_value()) {
        emit invalidAddressRequested(QStringLiteral("Enter a valid 8-bit value to search for."));
        return;
    }

    const QByteArray memory = model_->memory();
    const quint16 startAddress = static_cast<quint16>(currentSelectedAddress() + 1U);
    for (int offset = 0; offset < memory.size(); ++offset) {
        const quint16 address = static_cast<quint16>((startAddress + offset) & 0xFFFFU);
        if (static_cast<quint8>(memory.at(address)) == *parsed) {
            scrollToAddress(address);
            return;
        }
    }

    emit invalidAddressRequested(QStringLiteral("Value %1 was not found in memory.")
                                     .arg(formatHex8(*parsed)));
}

void MemoryViewer::handleBookmarkCurrent() {
    const quint16 address = currentSelectedAddress();
    if (!bookmarks_.contains(address)) {
        bookmarks_.append(address);
        std::sort(bookmarks_.begin(), bookmarks_.end());
        refreshBookmarkCombo();
    }
    const int index = bookmarks_.indexOf(address);
    if (index >= 0) {
        bookmarksCombo_->setCurrentIndex(index + 1);
    }
}

void MemoryViewer::handleBookmarkSelected(int index) {
    if (index <= 0 || index - 1 >= bookmarks_.size()) {
        return;
    }
    scrollToAddress(bookmarks_.at(index - 1));
}

void MemoryViewer::jumpToPc() {
    scrollToAddress(pcAddress_);
}

void MemoryViewer::jumpToSp() {
    scrollToAddress(spAddress_);
}

void MemoryViewer::jumpToHl() {
    scrollToAddress(hlAddress_);
}

std::optional<quint16> MemoryViewer::parseAddressInput(const QString& text) const {
    if (addressFormatCombo_->currentIndex() == 1) {
        bool ok = false;
        const uint value = text.trimmed().toUInt(&ok, 10);
        if (ok && value <= 0xFFFFU) {
            return static_cast<quint16>(value);
        }
    }
    return parseUnsignedValue(text, 16);
}

std::optional<quint8> MemoryViewer::parseValueInput(const QString& text) const {
    const auto parsed = parseUnsignedValue(text, 8);
    if (!parsed.has_value()) {
        return std::nullopt;
    }
    return static_cast<quint8>(*parsed);
}

quint16 MemoryViewer::currentSelectedAddress() const {
    const QModelIndex currentIndex = table_->currentIndex();
    if (!currentIndex.isValid()) {
        return 0U;
    }

    const int row = currentIndex.row();
    const int column = currentIndex.column();
    if (column <= 0 || column >= 17) {
        return static_cast<quint16>(row * 16);
    }
    return static_cast<quint16>(row * 16 + column - 1);
}

void MemoryViewer::refreshBookmarkCombo() {
    const QSignalBlocker blocker(bookmarksCombo_);
    bookmarksCombo_->clear();
    bookmarksCombo_->addItem(QStringLiteral("Bookmarks"));
    for (const quint16 address : bookmarks_) {
        bookmarksCombo_->addItem(formatHex16(address));
    }
}

}  // namespace Core85::Gui

#include "memoryviewer.moc"
