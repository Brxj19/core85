#include "gui/iopanel.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QPainter>
#include <QSpinBox>
#include <QVBoxLayout>

#include "gui/value_utils.h"

namespace Core85::Gui {

class IOPanel::SevenSegmentDisplay final : public QWidget {
public:
    explicit SevenSegmentDisplay(QWidget* parent = nullptr)
        : QWidget(parent) {
        setMinimumSize(120, 180);
    }

    void setValue(quint8 value) {
        value_ = value;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), QColor(QStringLiteral("#111827")));

        const QColor onColor(QStringLiteral("#F97316"));
        const QColor offColor(QStringLiteral("#374151"));
        const QRect bounds = rect().adjusted(18, 18, -18, -18);

        auto segment = [&](const QRect& segmentRect, bool enabled) {
            painter.setBrush(enabled ? onColor : offColor);
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(segmentRect, 6, 6);
        };

        segment(QRect(bounds.left() + 20, bounds.top(), bounds.width() - 40, 14), (value_ & 0x01U) != 0U);
        segment(QRect(bounds.right() - 14, bounds.top() + 20, 14, bounds.height() / 2 - 24), (value_ & 0x02U) != 0U);
        segment(QRect(bounds.right() - 14, bounds.center().y() + 8, 14, bounds.height() / 2 - 24), (value_ & 0x04U) != 0U);
        segment(QRect(bounds.left() + 20, bounds.bottom() - 14, bounds.width() - 40, 14), (value_ & 0x08U) != 0U);
        segment(QRect(bounds.left(), bounds.center().y() + 8, 14, bounds.height() / 2 - 24), (value_ & 0x10U) != 0U);
        segment(QRect(bounds.left(), bounds.top() + 20, 14, bounds.height() / 2 - 24), (value_ & 0x20U) != 0U);
        segment(QRect(bounds.left() + 20, bounds.center().y() - 7, bounds.width() - 40, 14), (value_ & 0x40U) != 0U);

        painter.setBrush((value_ & 0x80U) != 0U ? onColor : offColor);
        painter.drawEllipse(bounds.right() - 6, bounds.bottom() - 10, 12, 12);
    }

private:
    quint8 value_ = 0U;
};

IOPanel::IOPanel(QWidget* parent)
    : QWidget(parent),
      inputPortSpin_(new QSpinBox(this)),
      ledPortSpin_(new QSpinBox(this)),
      segmentPortSpin_(new QSpinBox(this)),
      inputPorts_(256, 0),
      outputPorts_(256, 0) {
    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(12);

    auto configurePortSpin = [](QSpinBox* spin) {
        spin->setRange(0, 255);
        spin->setDisplayIntegerBase(16);
        spin->setPrefix(QStringLiteral("0x"));
        spin->setValue(0);
    };
    configurePortSpin(inputPortSpin_);
    configurePortSpin(ledPortSpin_);
    configurePortSpin(segmentPortSpin_);
    ledPortSpin_->setValue(1);
    segmentPortSpin_->setValue(2);

    auto* inputGroup = new QGroupBox(QStringLiteral("Input Switches"), this);
    auto* inputLayout = new QVBoxLayout(inputGroup);
    auto* inputForm = new QFormLayout();
    inputForm->addRow(QStringLiteral("Port"), inputPortSpin_);
    inputLayout->addLayout(inputForm);
    auto* inputBitsLayout = new QGridLayout();
    for (int bit = 7; bit >= 0; --bit) {
        auto* checkBox = new QCheckBox(QStringLiteral("B%1").arg(bit), inputGroup);
        inputBits_.push_back(checkBox);
        inputBitsLayout->addWidget(checkBox, (7 - bit) / 4, (7 - bit) % 4);
        connect(checkBox, &QCheckBox::toggled, this, &IOPanel::handleInputToggle);
    }
    inputLayout->addLayout(inputBitsLayout);
    rootLayout->addWidget(inputGroup, 1);

    auto* ledGroup = new QGroupBox(QStringLiteral("LED Output"), this);
    auto* ledLayout = new QVBoxLayout(ledGroup);
    auto* ledForm = new QFormLayout();
    ledForm->addRow(QStringLiteral("Port"), ledPortSpin_);
    ledLayout->addLayout(ledForm);
    auto* leds = new QGridLayout();
    for (int bit = 7; bit >= 0; --bit) {
        auto* label = new QLabel(QStringLiteral("B%1").arg(bit), ledGroup);
        label->setAlignment(Qt::AlignCenter);
        label->setMinimumHeight(34);
        label->setFrameShape(QFrame::StyledPanel);
        ledIndicators_.push_back(label);
        leds->addWidget(label, (7 - bit) / 4, (7 - bit) % 4);
    }
    ledLayout->addLayout(leds);
    rootLayout->addWidget(ledGroup, 1);

    auto* segmentGroup = new QGroupBox(QStringLiteral("7-Segment Display"), this);
    auto* segmentLayout = new QVBoxLayout(segmentGroup);
    auto* segmentForm = new QFormLayout();
    segmentForm->addRow(QStringLiteral("Port"), segmentPortSpin_);
    segmentLayout->addLayout(segmentForm);
    segmentDisplay_ = new SevenSegmentDisplay(segmentGroup);
    segmentLayout->addWidget(segmentDisplay_, 1);
    rootLayout->addWidget(segmentGroup, 1);

    connect(inputPortSpin_, qOverload<int>(&QSpinBox::valueChanged), this, &IOPanel::emitProjectMetadata);
    connect(ledPortSpin_, qOverload<int>(&QSpinBox::valueChanged), this, &IOPanel::emitProjectMetadata);
    connect(segmentPortSpin_, qOverload<int>(&QSpinBox::valueChanged), this, &IOPanel::emitProjectMetadata);
}

void IOPanel::setProjectMetadata(const ProjectMetadata& metadata) {
    inputPortSpin_->setValue(metadata.inputPort);
    ledPortSpin_->setValue(metadata.ledPort);
    segmentPortSpin_->setValue(metadata.segmentPort);
    refreshOutputWidgets();
}

ProjectMetadata IOPanel::projectMetadata() const {
    return ProjectMetadata{
        static_cast<quint8>(inputPortSpin_->value()),
        static_cast<quint8>(ledPortSpin_->value()),
        static_cast<quint8>(segmentPortSpin_->value()),
    };
}

void IOPanel::setPortSnapshots(const QByteArray& inputPorts, const QByteArray& outputPorts) {
    inputPorts_ = inputPorts;
    outputPorts_ = outputPorts;

    const quint8 inputValue = static_cast<quint8>(inputPorts_.at(inputPortSpin_->value()));
    for (int visualIndex = 0; visualIndex < inputBits_.size(); ++visualIndex) {
        const int bit = 7 - visualIndex;
        QSignalBlocker blocker(inputBits_.at(visualIndex));
        inputBits_.at(visualIndex)->setChecked((inputValue & (1U << bit)) != 0U);
    }

    refreshOutputWidgets();
}

void IOPanel::handleInputToggle() {
    const quint8 port = static_cast<quint8>(inputPortSpin_->value());
    emit inputPortChanged(port, currentInputValue());
}

void IOPanel::emitProjectMetadata() {
    emit projectMetadataChanged(projectMetadata());
    handleInputToggle();
    refreshOutputWidgets();
}

quint8 IOPanel::currentInputValue() const {
    quint8 value = 0U;
    for (int visualIndex = 0; visualIndex < inputBits_.size(); ++visualIndex) {
        if (inputBits_.at(visualIndex)->isChecked()) {
            value |= static_cast<quint8>(1U << (7 - visualIndex));
        }
    }
    return value;
}

void IOPanel::refreshOutputWidgets() {
    const quint8 ledValue = static_cast<quint8>(outputPorts_.at(ledPortSpin_->value()));
    for (int visualIndex = 0; visualIndex < ledIndicators_.size(); ++visualIndex) {
        const int bit = 7 - visualIndex;
        const bool enabled = (ledValue & (1U << bit)) != 0U;
        ledIndicators_.at(visualIndex)->setText(QStringLiteral("B%1\n%2").arg(bit).arg(enabled ? QStringLiteral("ON")
                                                                                               : QStringLiteral("OFF")));
        ledIndicators_.at(visualIndex)->setStyleSheet(enabled ? QStringLiteral("background: #FDE68A; color: #78350F;")
                                                              : QStringLiteral("background: #E5E7EB; color: #4B5563;"));
    }

    const quint8 segmentValue = static_cast<quint8>(outputPorts_.at(segmentPortSpin_->value()));
    segmentDisplay_->setValue(segmentValue);
}

}  // namespace Core85::Gui
