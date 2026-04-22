#include "gui/emulatorcontroller.h"

#include <exception>

namespace Core85::Gui {

EmulatorController::EmulatorController(QObject* parent)
    : QObject(parent), baselineMemory_(Core85::Memory::kSize, static_cast<char>(Core85::Memory::kDefaultValue)) {
    qRegisterMetaType<Core85::Gui::EmulatorSnapshot>("Core85::Gui::EmulatorSnapshot");
    qRegisterMetaType<Core85::Gui::ExecutionState>("Core85::Gui::ExecutionState");

    cpu_.setOnOutputWrite([this](uint8_t, uint8_t) {
        if (!isRunTimerActive() || stateRefreshTimer_.elapsed() >= 16) {
            emitSnapshot();
        }
    });
    stateRefreshTimer_.start();
}

void EmulatorController::loadProgram(quint16 origin, const QByteArray& bytes) {
    baselineMemory_.fill(static_cast<char>(Core85::Memory::kDefaultValue));
    for (int offset = 0; offset < bytes.size(); ++offset) {
        baselineMemory_[origin + offset] = bytes.at(offset);
    }

    baselineOrigin_ = origin;
    hasBaseline_ = true;
    temporaryBreakpoint_.reset();
    cpu_.memory().reset();
    cpu_.reset();
    cpu_.loadProgram(Span<const uint8_t>(
                         reinterpret_cast<const uint8_t*>(bytes.constData()),
                         static_cast<std::size_t>(bytes.size())),
                     origin);
    setExecutionState(ExecutionState::Paused);
    emitSnapshot();
}

void EmulatorController::requestSnapshot() {
    emitSnapshot();
}

void EmulatorController::run() {
    if (cpu_.isHalted()) {
        setExecutionState(ExecutionState::Halted);
        emitSnapshot();
        return;
    }

    setExecutionState(ExecutionState::Running);
    QTimer* timer = ensureRunTimer();
    if (!timer->isActive()) {
        timer->start();
    }
}

void EmulatorController::pause() {
    stopRunTimer();
    if (cpu_.isHalted()) {
        setExecutionState(ExecutionState::Halted);
    } else {
        setExecutionState(ExecutionState::Paused);
    }
    emitSnapshot();
}

void EmulatorController::stepInto() {
    stopRunTimer();

    if (cpu_.isHalted()) {
        setExecutionState(ExecutionState::Halted);
        emitSnapshot();
        return;
    }

    try {
        cpu_.step();
    } catch (const std::exception& error) {
        emit emulatorError(QString::fromUtf8(error.what()));
    }

    setExecutionState(cpu_.isHalted() ? ExecutionState::Halted : ExecutionState::Paused);
    emitSnapshot();
}

void EmulatorController::stepOver() {
    stopRunTimer();

    const quint8 opcode = cpu_.memory().readByte(cpu_.registers().pc);
    if (!isCallOpcode(opcode)) {
        stepInto();
        return;
    }

    temporaryBreakpoint_ = static_cast<quint16>(cpu_.registers().pc + 3U);
    run();
}

void EmulatorController::resetToLoadedProgram() {
    stopRunTimer();

    cpu_.memory().reset();
    cpu_.reset();
    if (hasBaseline_) {
        for (int address = 0; address < baselineMemory_.size(); ++address) {
            const quint8 value = static_cast<quint8>(baselineMemory_.at(address));
            if (value != Core85::Memory::kDefaultValue) {
                cpu_.memory().writeByte(static_cast<uint16_t>(address), value);
            }
        }

        cpu_.registers().pc = baselineOrigin_;
    }

    temporaryBreakpoint_.reset();
    setExecutionState(ExecutionState::Paused);
    emitSnapshot();
}

void EmulatorController::setBreakpoints(const QList<quint16>& addresses) {
    breakpoints_.clear();
    for (const quint16 address : addresses) {
        breakpoints_.insert(address);
    }
}

void EmulatorController::setSpeedMode(int speedIndex) {
    speedIndex_ = speedIndex;
}

void EmulatorController::setInputPort(quint8 port, quint8 value) {
    cpu_.setInputPort(port, value);
    emitSnapshot();
}

void EmulatorController::writeMemory(quint16 address, quint8 value) {
    cpu_.memory().writeByte(address, value);
    emitSnapshot();
}

void EmulatorController::setRegisterValue(const QString& field, quint16 value) {
    const QString key = field.trimmed().toUpper();
    auto& registers = cpu_.registers();

    if (key == QStringLiteral("A")) {
        registers.a = static_cast<uint8_t>(value);
    } else if (key == QStringLiteral("B")) {
        registers.b = static_cast<uint8_t>(value);
    } else if (key == QStringLiteral("C")) {
        registers.c = static_cast<uint8_t>(value);
    } else if (key == QStringLiteral("D")) {
        registers.d = static_cast<uint8_t>(value);
    } else if (key == QStringLiteral("E")) {
        registers.e = static_cast<uint8_t>(value);
    } else if (key == QStringLiteral("H")) {
        registers.h = static_cast<uint8_t>(value);
    } else if (key == QStringLiteral("L")) {
        registers.l = static_cast<uint8_t>(value);
    } else if (key == QStringLiteral("F")) {
        registers.setFlagRegister(static_cast<uint8_t>(value));
    } else if (key == QStringLiteral("BC")) {
        registers.setBC(value);
    } else if (key == QStringLiteral("DE")) {
        registers.setDE(value);
    } else if (key == QStringLiteral("HL")) {
        registers.setHL(value);
    } else if (key == QStringLiteral("SP")) {
        registers.sp = value;
    } else if (key == QStringLiteral("PC")) {
        registers.pc = value;
    } else {
        emit emulatorError(QStringLiteral("Unsupported register edit: %1").arg(field));
        return;
    }

    emitSnapshot();
}

void EmulatorController::executeRunSlice() {
    if (shouldPauseForBreakpoint()) {
        stopRunTimer();
        setExecutionState(ExecutionState::Paused);
        emitSnapshot();
        return;
    }

    const int batch = instructionsPerBatch(speedIndex_);
    for (int count = 0; count < batch; ++count) {
        if (shouldPauseForBreakpoint()) {
            stopRunTimer();
            setExecutionState(ExecutionState::Paused);
            emitSnapshot();
            return;
        }

        try {
            cpu_.step();
        } catch (const std::exception& error) {
            stopRunTimer();
            setExecutionState(ExecutionState::Paused);
            emit emulatorError(QString::fromUtf8(error.what()));
            emitSnapshot();
            return;
        }

        if (cpu_.isHalted()) {
            stopRunTimer();
            setExecutionState(ExecutionState::Halted);
            emitSnapshot();
            return;
        }
    }

    if (stateRefreshTimer_.elapsed() >= 16) {
        emitSnapshot();
    }
}

bool EmulatorController::isCallOpcode(quint8 opcode) {
    switch (opcode) {
        case 0xC4U:
        case 0xCCU:
        case 0xCDU:
        case 0xD4U:
        case 0xDCU:
        case 0xE4U:
        case 0xECU:
        case 0xF4U:
        case 0xFCU:
            return true;
        default:
            return false;
    }
}

int EmulatorController::instructionsPerBatch(int speedIndex) {
    switch (speedIndex) {
        case 0:
            return 1;
        case 1:
            return 10;
        case 2:
            return 100;
        case 3:
            return 1000;
        case 4:
        default:
            return 10000;
    }
}

QTimer* EmulatorController::ensureRunTimer() {
    if (!runTimer_) {
        runTimer_ = std::make_unique<QTimer>();
        runTimer_->setInterval(0);
        runTimer_->setSingleShot(false);
        connect(runTimer_.get(), &QTimer::timeout, this, &EmulatorController::executeRunSlice);
    }

    return runTimer_.get();
}

bool EmulatorController::isRunTimerActive() const {
    return runTimer_ != nullptr && runTimer_->isActive();
}

void EmulatorController::stopRunTimer() {
    if (runTimer_ != nullptr && runTimer_->isActive()) {
        runTimer_->stop();
    }
}

void EmulatorController::emitSnapshot() {
    EmulatorSnapshot snapshot;
    snapshot.cpuState = cpu_.getState();
    snapshot.memory.resize(Core85::Memory::kSize);
    const auto& memory = cpu_.memory().raw();
    for (std::size_t index = 0; index < memory.size(); ++index) {
        snapshot.memory[static_cast<int>(index)] = static_cast<char>(memory[index]);
    }

    snapshot.inputPorts.resize(256);
    snapshot.outputPorts.resize(256);
    for (int port = 0; port < 256; ++port) {
        snapshot.inputPorts[port] =
            static_cast<char>(cpu_.ioBus().readInputPort(static_cast<uint8_t>(port)));
        snapshot.outputPorts[port] =
            static_cast<char>(cpu_.ioBus().getOutputPort(static_cast<uint8_t>(port)));
    }

    stateRefreshTimer_.restart();
    emit snapshotReady(snapshot);
}

void EmulatorController::setExecutionState(ExecutionState state) {
    if (executionState_ == state) {
        return;
    }
    executionState_ = state;
    emit executionStateChanged(state);
}

bool EmulatorController::shouldPauseForBreakpoint() {
    const quint16 pc = cpu_.registers().pc;
    if (temporaryBreakpoint_.has_value() && *temporaryBreakpoint_ == pc) {
        temporaryBreakpoint_.reset();
        return true;
    }
    return breakpoints_.contains(pc);
}

}  // namespace Core85::Gui
