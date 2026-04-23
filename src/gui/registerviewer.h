#ifndef CORE85_GUI_REGISTERVIEWER_H
#define CORE85_GUI_REGISTERVIEWER_H

#include <QSet>
#include <QWidget>

#include "core/cpu.h"

class QLabel;
class QPushButton;
class QComboBox;
class QTableWidget;

namespace Core85::Gui {

class RegisterViewer final : public QWidget {
    Q_OBJECT

public:
    explicit RegisterViewer(QWidget* parent = nullptr);

    void setState(const Core85::CPUState& state, const QSet<QString>& changedFields);
    void setInteractive(bool enabled);

signals:
    void registerWriteRequested(const QString& field, quint16 value);
    void invalidInputRequested(const QPoint& globalPos, const QString& message);

private slots:
    void handleCellChanged(int row, int column);
    void copySnapshot();

private:
    enum class DisplayMode {
        Hex,
        Decimal,
        Binary,
    };

    struct RegisterRow {
        QString name;
        quint16 value = 0U;
        int bits = 8;
        bool editable = true;
    };

    void rebuildRows(const Core85::CPUState& state);
    void updateFlagIndicator(QLabel* label, const QString& name, bool enabled);
    void updateRow(int row, const RegisterRow& data, bool changed);
    QString primaryValueForRow(const RegisterRow& data) const;
    QString secondaryValueForRow(const RegisterRow& data) const;
    QString formatBinary(quint16 value, int bits) const;

    QTableWidget* table_ = nullptr;
    QComboBox* displayModeCombo_ = nullptr;
    QPushButton* copySnapshotButton_ = nullptr;
    QLabel* controlStateLabel_ = nullptr;
    QLabel* signFlag_ = nullptr;
    QLabel* zeroFlag_ = nullptr;
    QLabel* auxCarryFlag_ = nullptr;
    QLabel* parityFlag_ = nullptr;
    QLabel* carryFlag_ = nullptr;
    QVector<RegisterRow> rows_{};
    Core85::CPUState lastState_{};
    DisplayMode displayMode_ = DisplayMode::Hex;
    bool updating_ = false;
};

}  // namespace Core85::Gui

#endif
