#include "core/cpu.h"

#include <sstream>

namespace Core85 {

namespace {

constexpr uint8_t kSignMask = 0x80U;
constexpr uint8_t kZeroMask = 0x40U;
constexpr uint8_t kAuxiliaryCarryMask = 0x10U;
constexpr uint8_t kParityMask = 0x04U;
constexpr uint8_t kFixedMask = 0x02U;
constexpr uint8_t kCarryMask = 0x01U;

}  // namespace

uint8_t Flags::toByte() const noexcept {
    uint8_t value = kFixedMask;
    value |= sign ? kSignMask : 0U;
    value |= zero ? kZeroMask : 0U;
    value |= auxiliaryCarry ? kAuxiliaryCarryMask : 0U;
    value |= parity ? kParityMask : 0U;
    value |= carry ? kCarryMask : 0U;
    return value;
}

Flags Flags::fromByte(uint8_t value) noexcept {
    Flags flags;
    flags.sign = (value & kSignMask) != 0U;
    flags.zero = (value & kZeroMask) != 0U;
    flags.auxiliaryCarry = (value & kAuxiliaryCarryMask) != 0U;
    flags.parity = (value & kParityMask) != 0U;
    flags.carry = (value & kCarryMask) != 0U;
    return flags;
}

uint8_t RegisterBank::getFlagRegister() const noexcept {
    return flags.toByte();
}

void RegisterBank::setFlagRegister(uint8_t value) noexcept {
    flags = Flags::fromByte(value);
}

uint16_t RegisterBank::getBC() const noexcept {
    return static_cast<uint16_t>((static_cast<uint16_t>(b) << 8U) | c);
}

uint16_t RegisterBank::getDE() const noexcept {
    return static_cast<uint16_t>((static_cast<uint16_t>(d) << 8U) | e);
}

uint16_t RegisterBank::getHL() const noexcept {
    return static_cast<uint16_t>((static_cast<uint16_t>(h) << 8U) | l);
}

void RegisterBank::setBC(uint16_t value) noexcept {
    b = static_cast<uint8_t>(value >> 8U);
    c = static_cast<uint8_t>(value & 0x00FFU);
}

void RegisterBank::setDE(uint16_t value) noexcept {
    d = static_cast<uint8_t>(value >> 8U);
    e = static_cast<uint8_t>(value & 0x00FFU);
}

void RegisterBank::setHL(uint16_t value) noexcept {
    h = static_cast<uint8_t>(value >> 8U);
    l = static_cast<uint8_t>(value & 0x00FFU);
}

void RegisterBank::clear() noexcept {
    a = 0U;
    b = 0U;
    c = 0U;
    d = 0U;
    e = 0U;
    h = 0U;
    l = 0U;
    flags = {};
    pc = 0U;
    sp = 0xFFFFU;
}

CPU::CPU() {
    reset();
}

void CPU::step() {
    if (halted_) {
        if (pendingInterrupt_.has_value()) {
            halted_ = false;
        } else {
            return;
        }
    }

    const uint8_t opcode = memory_.readByte(registers_.pc);

    switch (opcode) {
        case 0x00:
            registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
            elapsedCycles_ += 4U;
            break;
        case 0x76:
            registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
            halted_ = true;
            elapsedCycles_ += 7U;
            break;
        default:
            throwUnimplementedOpcode(opcode);
    }

    pendingInterrupt_.reset();
}

void CPU::reset() noexcept {
    registers_.clear();
    ioBus_.reset();
    elapsedCycles_ = 0U;
    halted_ = false;
    pendingInterrupt_.reset();
}

CPUState CPU::getState() const noexcept {
    CPUState state;
    state.a = registers_.a;
    state.b = registers_.b;
    state.c = registers_.c;
    state.d = registers_.d;
    state.e = registers_.e;
    state.h = registers_.h;
    state.l = registers_.l;
    state.f = registers_.getFlagRegister();
    state.bc = registers_.getBC();
    state.de = registers_.getDE();
    state.hl = registers_.getHL();
    state.pc = registers_.pc;
    state.sp = registers_.sp;
    state.elapsedCycles = elapsedCycles_;
    state.halted = halted_;
    return state;
}

void CPU::loadProgram(Span<const uint8_t> program, uint16_t address) noexcept {
    memory_.loadBytes(program, address);
    registers_.pc = address;
    halted_ = false;
}

void CPU::setInputPort(uint8_t port, uint8_t value) noexcept {
    ioBus_.setInputPort(port, value);
}

uint8_t CPU::getOutputPort(uint8_t port) const noexcept {
    return ioBus_.getOutputPort(port);
}

void CPU::raiseInterrupt(InterruptType type, uint8_t rst) noexcept {
    pendingInterrupt_ = PendingInterrupt{type, rst};
    halted_ = false;
}

bool CPU::isHalted() const noexcept {
    return halted_;
}

void CPU::setOnOutputWrite(OutputCallback callback) {
    ioBus_.setOnOutputWrite(std::move(callback));
}

uint64_t CPU::getElapsedCycles() const noexcept {
    return elapsedCycles_;
}

void CPU::resetCycles() noexcept {
    elapsedCycles_ = 0U;
}

RegisterBank& CPU::registers() noexcept {
    return registers_;
}

const RegisterBank& CPU::registers() const noexcept {
    return registers_;
}

Memory& CPU::memory() noexcept {
    return memory_;
}

const Memory& CPU::memory() const noexcept {
    return memory_;
}

IOBus& CPU::ioBus() noexcept {
    return ioBus_;
}

const IOBus& CPU::ioBus() const noexcept {
    return ioBus_;
}

[[noreturn]] void CPU::throwUnimplementedOpcode(uint8_t opcode) const {
    std::ostringstream message;
    message << "Opcode 0x" << std::hex << static_cast<int>(opcode)
            << " is not implemented yet.";
    throw std::runtime_error(message.str());
}

}  // namespace Core85
