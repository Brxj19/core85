#include "gui/emulator_types.h"

namespace Core85::Gui {

QString executionStateToString(ExecutionState state) {
    switch (state) {
        case ExecutionState::Paused:
            return QStringLiteral("Paused");
        case ExecutionState::Running:
            return QStringLiteral("Running");
        case ExecutionState::Halted:
            return QStringLiteral("Halted");
    }

    return QStringLiteral("Paused");
}

}  // namespace Core85::Gui
