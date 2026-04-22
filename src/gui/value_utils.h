#ifndef CORE85_GUI_VALUE_UTILS_H
#define CORE85_GUI_VALUE_UTILS_H

#include <optional>

#include <QString>

namespace Core85::Gui {

QString formatHex8(quint8 value);
QString formatHex16(quint16 value);
QString formatUnsigned(quint16 value);

std::optional<quint16> parseUnsignedValue(const QString& text, int maxBits);

}  // namespace Core85::Gui

#endif
