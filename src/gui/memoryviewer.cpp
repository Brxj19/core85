#include "gui/memoryviewer.h"

#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
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
                return QColor(QStringLiteral("#FECACA"));
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
        endResetModel();
    }

    void setInteractive(bool enabled) {
        interactive_ = enabled;
    }

signals:
    void memoryWriteRequested(quint16 address, quint8 value);

private:
    QByteArray memory_;
    QSet<quint16> changedAddresses_{};
    bool interactive_ = true;
};

MemoryViewer::MemoryViewer(QWidget* parent)
    : QWidget(parent), model_(new MemoryModel(this)), table_(new QTableView(this)), addressInput_(new QLineEdit(this)) {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(8);

    auto* title = new QLabel(QStringLiteral("Memory"), this);
    title->setStyleSheet(QStringLiteral("font-weight: 700;"));
    rootLayout->addWidget(title);

    auto* controls = new QHBoxLayout();
    auto* goToLabel = new QLabel(QStringLiteral("Go to:"), this);
    controls->addWidget(goToLabel);
    addressInput_->setPlaceholderText(QStringLiteral("0x0000"));
    controls->addWidget(addressInput_, 1);
    rootLayout->addLayout(controls);

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
    rootLayout->addWidget(table_, 1);

    connect(addressInput_, &QLineEdit::returnPressed, this, &MemoryViewer::handleGoToAddress);
    connect(model_, &MemoryModel::memoryWriteRequested, this, &MemoryViewer::memoryWriteRequested);
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
}

void MemoryViewer::setInteractive(bool enabled) {
    model_->setInteractive(enabled);
    table_->setEditTriggers(enabled ? QAbstractItemView::DoubleClicked |
                                          QAbstractItemView::EditKeyPressed |
                                          QAbstractItemView::SelectedClicked
                                    : QAbstractItemView::NoEditTriggers);
}

void MemoryViewer::handleGoToAddress() {
    const auto parsed = parseUnsignedValue(addressInput_->text(), 16);
    if (!parsed.has_value()) {
        emit invalidAddressRequested(QStringLiteral("Invalid 16-bit address"));
        return;
    }
    scrollToAddress(*parsed);
}

}  // namespace Core85::Gui

#include "memoryviewer.moc"
