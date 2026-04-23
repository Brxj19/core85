#ifndef CORE85_GUI_MEMORYVIEWER_H
#define CORE85_GUI_MEMORYVIEWER_H

#include <optional>

#include <QSet>
#include <QWidget>

class QAbstractTableModel;
class QComboBox;
class QLineEdit;
class QPushButton;
class QTableView;

namespace Core85::Gui {

class MemoryViewer final : public QWidget {
    Q_OBJECT

public:
    explicit MemoryViewer(QWidget* parent = nullptr);

    void setMemorySnapshot(const QByteArray& memory, const QSet<quint16>& changedAddresses);
    void scrollToAddress(quint16 address);
    void setInteractive(bool enabled);
    void setRegisterAnchors(quint16 pc, quint16 sp, quint16 hl);

signals:
    void memoryWriteRequested(quint16 address, quint8 value);
    void invalidAddressRequested(const QString& message);

private slots:
    void handleGoToAddress();
    void handleFindValue();
    void handleBookmarkCurrent();
    void handleBookmarkSelected(int index);
    void jumpToPc();
    void jumpToSp();
    void jumpToHl();

private:
    class MemoryModel;

    std::optional<quint16> parseAddressInput(const QString& text) const;
    std::optional<quint8> parseValueInput(const QString& text) const;
    quint16 currentSelectedAddress() const;
    void refreshBookmarkCombo();

    MemoryModel* model_ = nullptr;
    QTableView* table_ = nullptr;
    QLineEdit* addressInput_ = nullptr;
    QComboBox* addressFormatCombo_ = nullptr;
    QLineEdit* valueSearchInput_ = nullptr;
    QComboBox* bookmarksCombo_ = nullptr;
    QPushButton* bookmarkButton_ = nullptr;
    quint16 pcAddress_ = 0U;
    quint16 spAddress_ = 0U;
    quint16 hlAddress_ = 0U;
    QList<quint16> bookmarks_{};
};

}  // namespace Core85::Gui

#endif
