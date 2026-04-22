#ifndef CORE85_GUI_IOPANEL_H
#define CORE85_GUI_IOPANEL_H

#include <QByteArray>
#include <QVector>
#include <QWidget>

#include "gui/emulator_types.h"

class QCheckBox;
class QLabel;
class QSpinBox;

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

    QSpinBox* inputPortSpin_ = nullptr;
    QSpinBox* ledPortSpin_ = nullptr;
    QSpinBox* segmentPortSpin_ = nullptr;
    QVector<QCheckBox*> inputBits_{};
    QVector<QLabel*> ledIndicators_{};
    SevenSegmentDisplay* segmentDisplay_ = nullptr;
    QByteArray inputPorts_;
    QByteArray outputPorts_;
};

}  // namespace Core85::Gui

#endif
