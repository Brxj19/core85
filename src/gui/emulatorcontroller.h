#ifndef CORE85_GUI_EMULATORCONTROLLER_H
#define CORE85_GUI_EMULATORCONTROLLER_H

#include <QByteArray>
#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

#include <memory>
#include <optional>

#include "core/cpu.h"
#include "gui/emulator_types.h"

namespace Core85::Gui {

class EmulatorController final : public QObject {
    Q_OBJECT

public:
    explicit EmulatorController(QObject* parent = nullptr);

public slots:
    void loadProgram(quint16 origin, const QByteArray& bytes);
    void requestSnapshot();
    void run();
    void pause();
    void stepInto();
    void stepOver();
    void resetToLoadedProgram();
    void setBreakpoints(const QList<quint16>& addresses);
    void setSpeedMode(int speedIndex);
    void setInputPort(quint8 port, quint8 value);
    void writeMemory(quint16 address, quint8 value);
    void setRegisterValue(const QString& field, quint16 value);

signals:
    void snapshotReady(const Core85::Gui::EmulatorSnapshot& snapshot);
    void executionStateChanged(Core85::Gui::ExecutionState state);
    void emulatorError(const QString& message);

private slots:
    void executeRunSlice();

private:
    static bool isCallOpcode(quint8 opcode);
    static int instructionsPerBatch(int speedIndex);

    void emitSnapshot();
    void setExecutionState(ExecutionState state);
    bool shouldPauseForBreakpoint();
    QTimer* ensureRunTimer();
    bool isRunTimerActive() const;
    void stopRunTimer();

    Core85::CPU cpu_{};
    QByteArray baselineMemory_;
    quint16 baselineOrigin_ = 0U;
    bool hasBaseline_ = false;
    std::unique_ptr<QTimer> runTimer_{};
    QSet<quint16> breakpoints_{};
    std::optional<quint16> temporaryBreakpoint_{};
    ExecutionState executionState_ = ExecutionState::Paused;
    int speedIndex_ = 4;
    QElapsedTimer stateRefreshTimer_{};
};

}  // namespace Core85::Gui

#endif
