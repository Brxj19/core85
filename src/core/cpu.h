#ifndef CORE85_CPU_H
#define CORE85_CPU_H

#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>

#include "core/io_bus.h"
#include "core/memory.h"
#include "core/span.h"

namespace Core85 {

enum class InterruptType {
    Trap,
    Rst75,
    Rst65,
    Rst55,
    Intr,
};

struct Flags {
    bool sign = false;
    bool zero = false;
    bool auxiliaryCarry = false;
    bool parity = false;
    bool carry = false;

    uint8_t toByte() const noexcept;
    static Flags fromByte(uint8_t value) noexcept;
};

struct RegisterBank {
    uint8_t a = 0U;
    uint8_t b = 0U;
    uint8_t c = 0U;
    uint8_t d = 0U;
    uint8_t e = 0U;
    uint8_t h = 0U;
    uint8_t l = 0U;
    Flags flags{};
    uint16_t pc = 0U;
    uint16_t sp = 0xFFFFU;

    uint8_t getFlagRegister() const noexcept;
    void setFlagRegister(uint8_t value) noexcept;

    uint16_t getBC() const noexcept;
    uint16_t getDE() const noexcept;
    uint16_t getHL() const noexcept;

    void setBC(uint16_t value) noexcept;
    void setDE(uint16_t value) noexcept;
    void setHL(uint16_t value) noexcept;

    void clear() noexcept;
};

struct CPUState {
    uint8_t a = 0U;
    uint8_t b = 0U;
    uint8_t c = 0U;
    uint8_t d = 0U;
    uint8_t e = 0U;
    uint8_t h = 0U;
    uint8_t l = 0U;
    uint8_t f = 0U;
    uint16_t bc = 0U;
    uint16_t de = 0U;
    uint16_t hl = 0U;
    uint16_t pc = 0U;
    uint16_t sp = 0U;
    uint64_t elapsedCycles = 0U;
    bool halted = false;
};

class CPU {
public:
    using OutputCallback = IOBus::OutputCallback;

    CPU();

    void step();
    void reset() noexcept;
    CPUState getState() const noexcept;

    void loadProgram(Span<const uint8_t> program, uint16_t address) noexcept;

    void setInputPort(uint8_t port, uint8_t value) noexcept;
    uint8_t getOutputPort(uint8_t port) const noexcept;

    void raiseInterrupt(InterruptType type, uint8_t rst = 0U) noexcept;
    bool isHalted() const noexcept;
    void setOnOutputWrite(OutputCallback callback);

    uint64_t getElapsedCycles() const noexcept;
    void resetCycles() noexcept;

    RegisterBank& registers() noexcept;
    const RegisterBank& registers() const noexcept;

    Memory& memory() noexcept;
    const Memory& memory() const noexcept;

    IOBus& ioBus() noexcept;
    const IOBus& ioBus() const noexcept;

private:
    struct PendingInterrupt {
        InterruptType type = InterruptType::Trap;
        uint8_t rstOpcode = 0U;
    };

    [[noreturn]] void throwUnimplementedOpcode(uint8_t opcode) const;

    RegisterBank registers_{};
    Memory memory_{};
    IOBus ioBus_{};
    uint64_t elapsedCycles_ = 0U;
    bool halted_ = false;
    std::optional<PendingInterrupt> pendingInterrupt_;
};

}  // namespace Core85

#endif
