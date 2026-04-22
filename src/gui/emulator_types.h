#ifndef CORE85_GUI_EMULATOR_TYPES_H
#define CORE85_GUI_EMULATOR_TYPES_H

#include <QByteArray>
#include <QMetaType>
#include <QString>

#include "core/cpu.h"

namespace Core85::Gui {

enum class ExecutionState {
    Paused,
    Running,
    Halted,
};

struct EmulatorSnapshot {
    Core85::CPUState cpuState{};
    QByteArray memory;
    QByteArray inputPorts;
    QByteArray outputPorts;
};

struct ProjectMetadata {
    quint8 inputPort = 0x00U;
    quint8 ledPort = 0x01U;
    quint8 segmentPort = 0x02U;
};

QString executionStateToString(ExecutionState state);

}  // namespace Core85::Gui

Q_DECLARE_METATYPE(Core85::Gui::ExecutionState)
Q_DECLARE_METATYPE(Core85::Gui::EmulatorSnapshot)

#endif
