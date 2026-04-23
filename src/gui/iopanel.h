#ifndef CORE85_GUI_IOPANEL_H
#define CORE85_GUI_IOPANEL_H

#include <QByteArray>
#include <QVector>
#include <QWidget>

#include "gui/emulator_types.h"

class QCheckBox;
class QLabel;
class QSpinBox;
class QWidget;

namespace Core85::Gui {

class IOPanel final : public QWidget {
    Q_OBJECT

public:
    explicit IOPanel(QWidget* parent = nullptr);

    void setProjectMetadata(const ProjectMetadata& metadata);
    ProjectMetadata projectMetadata() const;
    void setPortSnapshots(const QByteArray& inputPorts, const QByteArray& outputPorts);

signals:
    void inputPortChanged(quint8 port, quint8 value);
    void projectMetadataChanged(const Core85::Gui::ProjectMetadata& metadata);

private slots:
    void handleInputToggle();
    void emitProjectMetadata();

private:
    class SevenSegmentDisplay;

    quint8 currentInputValue() const;
    void refreshOutputWidgets();
    QString formatBits(quint8 value) const;

    QSpinBox* inputPortSpin_ = nullptr;
    QSpinBox* ledPortSpin_ = nullptr;
    QSpinBox* segmentLeftPortSpin_ = nullptr;
    QSpinBox* segmentRightPortSpin_ = nullptr;
    QLabel* inputSummaryLabel_ = nullptr;
    QLabel* ledSummaryLabel_ = nullptr;
    QLabel* segmentSummaryLabel_ = nullptr;
    QVector<QCheckBox*> inputBits_{};
    QVector<QWidget*> ledIndicators_{};
    SevenSegmentDisplay* segmentLeftDisplay_ = nullptr;
    SevenSegmentDisplay* segmentRightDisplay_ = nullptr;
    QByteArray inputPorts_;
    QByteArray outputPorts_;
};

}  // namespace Core85::Gui

#endif
