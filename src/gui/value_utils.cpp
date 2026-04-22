#include "gui/value_utils.h"

#include <limits>

namespace Core85::Gui {

namespace {

QString normalizedNumber(QString text) {
    text = text.trimmed().toUpper();

    if (text.startsWith(QStringLiteral("0X"))) {
        return text.mid(2);
    }

    if (text.endsWith(QLatin1Char('H'))) {
        return text.left(text.size() - 1);
    }

    if (text.endsWith(QLatin1Char('D'))) {
        return text.left(text.size() - 1);
    }

    return text;
}

int detectBase(const QString& text) {
    const QString trimmed = text.trimmed().toUpper();
    if (trimmed.startsWith(QStringLiteral("0X")) || trimmed.endsWith(QLatin1Char('H'))) {
        return 16;
    }
    return 10;
}

}  // namespace

QString formatHex8(quint8 value) {
    return QStringLiteral("0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();
}

QString formatHex16(quint16 value) {
    return QStringLiteral("0x%1").arg(value, 4, 16, QLatin1Char('0')).toUpper();
}

QString formatUnsigned(quint16 value) {
    return QString::number(value);
}

std::optional<quint16> parseUnsignedValue(const QString& text, int maxBits) {
    bool ok = false;
    const int base = detectBase(text);
    const QString normalized = normalizedNumber(text);
    const unsigned long parsed = normalized.toULong(&ok, base);
    if (!ok) {
        return std::nullopt;
    }

    const quint32 maxValue =
        maxBits >= 16 ? std::numeric_limits<quint16>::max()
                      : static_cast<quint32>((1U << maxBits) - 1U);
    if (parsed > maxValue) {
        return std::nullopt;
    }

    return static_cast<quint16>(parsed);
}

}  // namespace Core85::Gui
