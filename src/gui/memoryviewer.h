#ifndef CORE85_GUI_MEMORYVIEWER_H
#define CORE85_GUI_MEMORYVIEWER_H

#include <QSet>
#include <QWidget>

class QAbstractTableModel;
class QLineEdit;
class QTableView;

namespace Core85::Gui {

class MemoryViewer final : public QWidget {
    Q_OBJECT

public:
    explicit MemoryViewer(QWidget* parent = nullptr);

    void setMemorySnapshot(const QByteArray& memory, const QSet<quint16>& changedAddresses);
    void scrollToAddress(quint16 address);
    void setInteractive(bool enabled);

signals:
    void memoryWriteRequested(quint16 address, quint8 value);
    void invalidAddressRequested(const QString& message);

private slots:
    void handleGoToAddress();

private:
    class MemoryModel;

    MemoryModel* model_ = nullptr;
    QTableView* table_ = nullptr;
    QLineEdit* addressInput_ = nullptr;
};

}  // namespace Core85::Gui

#endif
