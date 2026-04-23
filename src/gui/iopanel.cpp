#include "gui/iopanel.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include "gui/value_utils.h"

namespace Core85::Gui {

class ToggleSwitch final : public QCheckBox {
public:
    explicit ToggleSwitch(const QString& text, QWidget* parent = nullptr)
        : QCheckBox(text, parent) {
        setCursor(Qt::PointingHandCursor);
    }

    QSize sizeHint() const override {
        return QSize(84, 42);
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRect switchRect(0, 8, 46, 24);
        const bool darkTheme = palette().window().color().lightness() < 128;
        const QColor trackColor = isChecked() ? QColor(QStringLiteral("#22C55E"))
                                              : (darkTheme ? QColor(QStringLiteral("#334155"))
                                                           : QColor(QStringLiteral("#CBD5E1")));
        const QColor knobColor = QColor(QStringLiteral("#F8FAFC"));
        const int knobX = isChecked() ? switchRect.right() - 20 : switchRect.left() + 2;

        painter.setPen(Qt::NoPen);
        painter.setBrush(trackColor);
        painter.drawRoundedRect(switchRect, 12, 12);

        painter.setBrush(knobColor);
        painter.drawEllipse(QRect(knobX, switchRect.top() + 2, 20, 20));

        painter.setPen(palette().text().color());
        painter.setFont(QFont(font().family(), font().pointSize() - 1, QFont::Bold));
        painter.drawText(QRect(54, 0, width() - 54, height()),
                         Qt::AlignVCenter | Qt::AlignLeft,
                         text());
    }
};

class LedIndicator final : public QWidget {
public:
    explicit LedIndicator(const QString& bitLabel, QWidget* parent = nullptr)
        : QWidget(parent), bitLabel_(bitLabel) {
        setMinimumSize(62, 70);
    }

    void setEnabledState(bool enabled) {
        enabled_ = enabled;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF ledRect((width() - 28) / 2.0, 8.0, 28.0, 28.0);
        const QColor glowColor = enabled_ ? QColor(255, 196, 60, 110) : QColor(0, 0, 0, 0);
        const QColor ledColor = enabled_ ? QColor(QStringLiteral("#FCD34D"))
                                         : QColor(QStringLiteral("#334155"));
        const QColor rimColor = enabled_ ? QColor(QStringLiteral("#F59E0B"))
                                         : QColor(QStringLiteral("#475569"));

        if (enabled_) {
            painter.setBrush(glowColor);
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(ledRect.adjusted(-8, -8, 8, 8));
        }

        QRadialGradient gradient(ledRect.center(), ledRect.width() / 2.0);
        gradient.setColorAt(0.0, enabled_ ? QColor(QStringLiteral("#FFF7CC"))
                                          : QColor(QStringLiteral("#64748B")));
        gradient.setColorAt(1.0, ledColor);

        painter.setBrush(gradient);
        painter.setPen(QPen(rimColor, 1.5));
        painter.drawEllipse(ledRect);

        painter.setPen(palette().text().color());
        painter.setFont(QFont(font().family(), font().pointSize() - 1, QFont::Bold));
        painter.drawText(QRectF(0, 42, width(), 18), Qt::AlignCenter, bitLabel_);
        painter.setPen(enabled_ ? QColor(QStringLiteral("#FCD34D"))
                                : palette().mid().color());
        painter.drawText(QRectF(0, 56, width(), 12), Qt::AlignCenter, enabled_ ? QStringLiteral("ON")
                                                                                : QStringLiteral("OFF"));
    }

private:
    QString bitLabel_;
    bool enabled_ = false;
};

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
      segmentLeftPortSpin_(new QSpinBox(this)),
      segmentRightPortSpin_(new QSpinBox(this)),
      inputSummaryLabel_(new QLabel(this)),
      ledSummaryLabel_(new QLabel(this)),
      segmentSummaryLabel_(new QLabel(this)),
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
    configurePortSpin(segmentLeftPortSpin_);
    configurePortSpin(segmentRightPortSpin_);
    ledPortSpin_->setValue(1);
    segmentLeftPortSpin_->setValue(2);
    segmentRightPortSpin_->setValue(3);

    auto* inputGroup = new QGroupBox(QStringLiteral("Input Switches"), this);
    auto* inputLayout = new QVBoxLayout(inputGroup);
    auto* inputForm = new QFormLayout();
    inputForm->addRow(QStringLiteral("Port"), inputPortSpin_);
    inputPortSpin_->setToolTip(QStringLiteral("Port number read by the IN instruction for the switch bank."));
    inputLayout->addLayout(inputForm);
    auto* inputBitsLayout = new QGridLayout();
    for (int bit = 7; bit >= 0; --bit) {
        auto* checkBox = new ToggleSwitch(QStringLiteral("B%1").arg(bit), inputGroup);
        inputBits_.push_back(checkBox);
        inputBitsLayout->addWidget(checkBox, (7 - bit) / 4, (7 - bit) % 4);
        connect(checkBox, &QCheckBox::toggled, this, &IOPanel::handleInputToggle);
    }
    inputLayout->addLayout(inputBitsLayout);
    inputSummaryLabel_->setStyleSheet(QStringLiteral("color: palette(mid);"));
    inputSummaryLabel_->setWordWrap(true);
    inputLayout->addWidget(inputSummaryLabel_);
    rootLayout->addWidget(inputGroup, 1);

    auto* ledGroup = new QGroupBox(QStringLiteral("LED Output"), this);
    auto* ledLayout = new QVBoxLayout(ledGroup);
    auto* ledForm = new QFormLayout();
    ledForm->addRow(QStringLiteral("Port"), ledPortSpin_);
    ledPortSpin_->setToolTip(QStringLiteral("Port number written by OUT to drive the LED bank."));
    ledLayout->addLayout(ledForm);
    auto* leds = new QGridLayout();
    for (int bit = 7; bit >= 0; --bit) {
        auto* indicator = new LedIndicator(QStringLiteral("B%1").arg(bit), ledGroup);
        ledIndicators_.push_back(indicator);
        leds->addWidget(indicator, (7 - bit) / 4, (7 - bit) % 4);
    }
    ledLayout->addLayout(leds);
    ledSummaryLabel_->setStyleSheet(QStringLiteral("color: palette(mid);"));
    ledSummaryLabel_->setWordWrap(true);
    ledLayout->addWidget(ledSummaryLabel_);
    rootLayout->addWidget(ledGroup, 1);

    auto* segmentGroup = new QGroupBox(QStringLiteral("2-Digit 7-Segment"), this);
    auto* segmentLayout = new QVBoxLayout(segmentGroup);
    auto* segmentForm = new QFormLayout();
    segmentForm->addRow(QStringLiteral("Left Port"), segmentLeftPortSpin_);
    segmentForm->addRow(QStringLiteral("Right Port"), segmentRightPortSpin_);
    segmentLeftPortSpin_->setToolTip(QStringLiteral("Port that feeds the left seven-segment digit."));
    segmentRightPortSpin_->setToolTip(QStringLiteral("Port that feeds the right seven-segment digit."));
    segmentLayout->addLayout(segmentForm);
    auto* digitsLayout = new QHBoxLayout();
    segmentLeftDisplay_ = new SevenSegmentDisplay(segmentGroup);
    segmentRightDisplay_ = new SevenSegmentDisplay(segmentGroup);
    digitsLayout->addWidget(segmentLeftDisplay_, 1);
    digitsLayout->addWidget(segmentRightDisplay_, 1);
    segmentLayout->addLayout(digitsLayout, 1);
    segmentSummaryLabel_->setStyleSheet(QStringLiteral("color: palette(mid);"));
    segmentSummaryLabel_->setWordWrap(true);
    segmentLayout->addWidget(segmentSummaryLabel_);
    rootLayout->addWidget(segmentGroup, 1);

    connect(inputPortSpin_, qOverload<int>(&QSpinBox::valueChanged), this, &IOPanel::emitProjectMetadata);
    connect(ledPortSpin_, qOverload<int>(&QSpinBox::valueChanged), this, &IOPanel::emitProjectMetadata);
    connect(segmentLeftPortSpin_, qOverload<int>(&QSpinBox::valueChanged), this, &IOPanel::emitProjectMetadata);
    connect(segmentRightPortSpin_, qOverload<int>(&QSpinBox::valueChanged), this, &IOPanel::emitProjectMetadata);
}

void IOPanel::setProjectMetadata(const ProjectMetadata& metadata) {
    inputPortSpin_->setValue(metadata.inputPort);
    ledPortSpin_->setValue(metadata.ledPort);
    segmentLeftPortSpin_->setValue(metadata.segmentLeftPort);
    segmentRightPortSpin_->setValue(metadata.segmentRightPort);
    refreshOutputWidgets();
}

ProjectMetadata IOPanel::projectMetadata() const {
    return ProjectMetadata{
        static_cast<quint8>(inputPortSpin_->value()),
        static_cast<quint8>(ledPortSpin_->value()),
        static_cast<quint8>(segmentLeftPortSpin_->value()),
        static_cast<quint8>(segmentRightPortSpin_->value()),
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
    inputSummaryLabel_->setText(
        QStringLiteral("Current input: %1 (%2)")
            .arg(formatHex8(currentInputValue()))
            .arg(formatBits(currentInputValue())));
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
    inputSummaryLabel_->setText(
        QStringLiteral("Current input: %1 (%2)")
            .arg(formatHex8(currentInputValue()))
            .arg(formatBits(currentInputValue())));

    const quint8 ledValue = static_cast<quint8>(outputPorts_.at(ledPortSpin_->value()));
    for (int visualIndex = 0; visualIndex < ledIndicators_.size(); ++visualIndex) {
        const int bit = 7 - visualIndex;
        const bool enabled = (ledValue & (1U << bit)) != 0U;
        static_cast<LedIndicator*>(ledIndicators_.at(visualIndex))->setEnabledState(enabled);
    }
    ledSummaryLabel_->setText(
        QStringLiteral("LED value: %1 (%2)")
            .arg(formatHex8(ledValue))
            .arg(formatBits(ledValue)));

    const quint8 leftSegmentValue = static_cast<quint8>(outputPorts_.at(segmentLeftPortSpin_->value()));
    const quint8 rightSegmentValue = static_cast<quint8>(outputPorts_.at(segmentRightPortSpin_->value()));
    segmentLeftDisplay_->setValue(leftSegmentValue);
    segmentRightDisplay_->setValue(rightSegmentValue);
    segmentSummaryLabel_->setText(
        QStringLiteral("Left %1  Right %2")
            .arg(formatHex8(leftSegmentValue))
            .arg(formatHex8(rightSegmentValue)));
}

QString IOPanel::formatBits(quint8 value) const {
    QString bits;
    bits.reserve(11);
    for (int bit = 7; bit >= 0; --bit) {
        if (bit != 7 && ((bit + 1) % 4 == 0)) {
            bits.append(QLatin1Char(' '));
        }
        bits.append((value & (1U << bit)) != 0U ? QLatin1Char('1') : QLatin1Char('0'));
    }
    return bits;
}

}  // namespace Core85::Gui
