#include "core/cpu.h"

#include <iostream>
#include <sstream>

namespace Core85 {

namespace {

constexpr uint8_t kSignMask = 0x80U;
constexpr uint8_t kZeroMask = 0x40U;
constexpr uint8_t kAuxiliaryCarryMask = 0x10U;
constexpr uint8_t kParityMask = 0x04U;
constexpr uint8_t kFixedMask = 0x02U;
constexpr uint8_t kCarryMask = 0x01U;

bool hasEvenParity(uint8_t value) noexcept {
    bool parity = true;

    while (value != 0U) {
        parity = !parity;
        value &= static_cast<uint8_t>(value - 1U);
    }

    return parity;
}

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
        if (!servicePendingInterrupts()) {
            return;
        }

        return;
    }

    const uint8_t opcode = memory_.readByte(registers_.pc);

    switch (opcode) {
        case 0x00:
            registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
            elapsedCycles_ += 4U;
            break;
        case 0x02:
        case 0x12: {
            const uint16_t address = getRegisterPair(static_cast<uint8_t>((opcode >> 4U) & 0x01U));
            memory_.writeByte(address, registers_.a);
            registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
            elapsedCycles_ += 7U;
            break;
        }
        case 0x07: {
            const bool newCarry = (registers_.a & 0x80U) != 0U;
            registers_.a = static_cast<uint8_t>((registers_.a << 1U) | (newCarry ? 0x01U : 0x00U));
            registers_.flags.carry = newCarry;
            registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
            elapsedCycles_ += 4U;
            break;
        }
        case 0x09:
        case 0x19:
        case 0x29:
        case 0x39:
            addRegisterPairToHL(getRegisterPair(static_cast<uint8_t>((opcode >> 4U) & 0x03U)));
            registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
            elapsedCycles_ += 10U;
            break;
        case 0x0A:
        case 0x1A: {
            const uint16_t address = getRegisterPair(static_cast<uint8_t>((opcode >> 4U) & 0x01U));
            registers_.a = memory_.readByte(address);
            registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
            elapsedCycles_ += 7U;
            break;
        }
        case 0x0F: {
            const bool newCarry = (registers_.a & 0x01U) != 0U;
            registers_.a = static_cast<uint8_t>((registers_.a >> 1U) | (newCarry ? 0x80U : 0x00U));
            registers_.flags.carry = newCarry;
            registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
            elapsedCycles_ += 4U;
            break;
        }
        case 0x17: {
            const bool oldCarry = registers_.flags.carry;
            registers_.flags.carry = (registers_.a & 0x80U) != 0U;
            registers_.a = static_cast<uint8_t>((registers_.a << 1U) | (oldCarry ? 0x01U : 0x00U));
            registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
            elapsedCycles_ += 4U;
            break;
        }
        case 0x1F: {
            const bool oldCarry = registers_.flags.carry;
            registers_.flags.carry = (registers_.a & 0x01U) != 0U;
            registers_.a = static_cast<uint8_t>((registers_.a >> 1U) | (oldCarry ? 0x80U : 0x00U));
            registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
            elapsedCycles_ += 4U;
            break;
        }
        case 0x20: {
            uint8_t value = 0U;
            if (pendingInterrupt_.has_value()) {
                switch (pendingInterrupt_->type) {
                    case InterruptType::Rst75:
                        value |= 0x40U;
                        break;
                    case InterruptType::Rst65:
                        value |= 0x20U;
                        break;
                    case InterruptType::Rst55:
                        value |= 0x10U;
                        break;
                    default:
                        break;
                }
            }

            if (interruptEnabled_) {
                value |= 0x08U;
            }

            value |= maskRst75_ ? 0x04U : 0x00U;
            value |= maskRst65_ ? 0x02U : 0x00U;
            value |= maskRst55_ ? 0x01U : 0x00U;
            registers_.a = value;
            registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
            elapsedCycles_ += 4U;
            break;
        }
        case 0x22: {
            const uint16_t address =
                readWord(static_cast<uint16_t>(registers_.pc + 1U));
            writeWord(address, registers_.getHL());
            registers_.pc = static_cast<uint16_t>(registers_.pc + 3U);
            elapsedCycles_ += 16U;
            break;
        }
        case 0x27:
            decimalAdjustAccumulator();
            registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
            elapsedCycles_ += 4U;
            break;
        case 0x2A: {
            const uint16_t address =
                readWord(static_cast<uint16_t>(registers_.pc + 1U));
            registers_.setHL(readWord(address));
            registers_.pc = static_cast<uint16_t>(registers_.pc + 3U);
            elapsedCycles_ += 16U;
            break;
        }
        case 0x2F:
            registers_.a = static_cast<uint8_t>(~registers_.a);
            registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
            elapsedCycles_ += 4U;
            break;
        case 0x30:
            if ((registers_.a & 0x10U) != 0U &&
                pendingInterrupt_.has_value() &&
                pendingInterrupt_->type == InterruptType::Rst75) {
                pendingInterrupt_.reset();
            }

            if ((registers_.a & 0x08U) != 0U) {
                maskRst75_ = (registers_.a & 0x04U) != 0U;
                maskRst65_ = (registers_.a & 0x02U) != 0U;
                maskRst55_ = (registers_.a & 0x01U) != 0U;
            }

            registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
            elapsedCycles_ += 4U;
            break;
        case 0x32: {
            const uint16_t address =
                readWord(static_cast<uint16_t>(registers_.pc + 1U));
            memory_.writeByte(address, registers_.a);
            registers_.pc = static_cast<uint16_t>(registers_.pc + 3U);
            elapsedCycles_ += 13U;
            break;
        }
        case 0x37:
            registers_.flags.carry = true;
            registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
            elapsedCycles_ += 4U;
            break;
        case 0x3A: {
            const uint16_t address =
                readWord(static_cast<uint16_t>(registers_.pc + 1U));
            registers_.a = memory_.readByte(address);
            registers_.pc = static_cast<uint16_t>(registers_.pc + 3U);
            elapsedCycles_ += 13U;
            break;
        }
        case 0x3F:
            registers_.flags.carry = !registers_.flags.carry;
            registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
            elapsedCycles_ += 4U;
            break;
        case 0x76:
            registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
            halted_ = true;
            elapsedCycles_ += 7U;
            break;
        case 0xC3:
            registers_.pc = readWord(static_cast<uint16_t>(registers_.pc + 1U));
            elapsedCycles_ += 10U;
            break;
        case 0xC6:
            addToAccumulator(memory_.readByte(static_cast<uint16_t>(registers_.pc + 1U)), false);
            registers_.pc = static_cast<uint16_t>(registers_.pc + 2U);
            elapsedCycles_ += 7U;
            break;
        case 0xC9:
            registers_.pc = popWord();
            elapsedCycles_ += 10U;
            break;
        case 0xCE:
            addToAccumulator(memory_.readByte(static_cast<uint16_t>(registers_.pc + 1U)), true);
            registers_.pc = static_cast<uint16_t>(registers_.pc + 2U);
            elapsedCycles_ += 7U;
            break;
        case 0xCD:
            callAddress(readWord(static_cast<uint16_t>(registers_.pc + 1U)));
            elapsedCycles_ += 18U;
            break;
        case 0xD3:
            ioBus_.writeOutputPort(
                memory_.readByte(static_cast<uint16_t>(registers_.pc + 1U)),
                registers_.a);
            registers_.pc = static_cast<uint16_t>(registers_.pc + 2U);
            elapsedCycles_ += 10U;
            break;
        case 0xD6:
            subtractFromAccumulator(memory_.readByte(static_cast<uint16_t>(registers_.pc + 1U)), false);
            registers_.pc = static_cast<uint16_t>(registers_.pc + 2U);
            elapsedCycles_ += 7U;
            break;
        case 0xDB:
            registers_.a = ioBus_.readInputPort(
                memory_.readByte(static_cast<uint16_t>(registers_.pc + 1U)));
            registers_.pc = static_cast<uint16_t>(registers_.pc + 2U);
            elapsedCycles_ += 10U;
            break;
        case 0xDE:
            subtractFromAccumulator(memory_.readByte(static_cast<uint16_t>(registers_.pc + 1U)), true);
            registers_.pc = static_cast<uint16_t>(registers_.pc + 2U);
            elapsedCycles_ += 7U;
            break;
        case 0xE3: {
            const uint8_t low = memory_.readByte(registers_.sp);
            const uint8_t high = memory_.readByte(static_cast<uint16_t>(registers_.sp + 1U));
            memory_.writeByte(registers_.sp, registers_.l);
            memory_.writeByte(static_cast<uint16_t>(registers_.sp + 1U), registers_.h);
            registers_.l = low;
            registers_.h = high;
            registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
            elapsedCycles_ += 18U;
            break;
        }
        case 0xEB: {
            const uint16_t previousDE = registers_.getDE();
            registers_.setDE(registers_.getHL());
            registers_.setHL(previousDE);
            registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
            elapsedCycles_ += 4U;
            break;
        }
        case 0xE6:
            andWithAccumulator(memory_.readByte(static_cast<uint16_t>(registers_.pc + 1U)));
            registers_.pc = static_cast<uint16_t>(registers_.pc + 2U);
            elapsedCycles_ += 7U;
            break;
        case 0xE9:
            registers_.pc = registers_.getHL();
            elapsedCycles_ += 5U;
            break;
        case 0xEE:
            xorWithAccumulator(memory_.readByte(static_cast<uint16_t>(registers_.pc + 1U)));
            registers_.pc = static_cast<uint16_t>(registers_.pc + 2U);
            elapsedCycles_ += 7U;
            break;
        case 0xF3:
            interruptEnabled_ = false;
            interruptEnableDelay_ = 0U;
            registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
            elapsedCycles_ += 4U;
            break;
        case 0xF6:
            orWithAccumulator(memory_.readByte(static_cast<uint16_t>(registers_.pc + 1U)));
            registers_.pc = static_cast<uint16_t>(registers_.pc + 2U);
            elapsedCycles_ += 7U;
            break;
        case 0xF9:
            registers_.sp = registers_.getHL();
            registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
            elapsedCycles_ += 5U;
            break;
        case 0xFB:
            interruptEnableDelay_ = 2U;
            registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
            elapsedCycles_ += 4U;
            break;
        case 0xFE:
            compareWithAccumulator(memory_.readByte(static_cast<uint16_t>(registers_.pc + 1U)));
            registers_.pc = static_cast<uint16_t>(registers_.pc + 2U);
            elapsedCycles_ += 7U;
            break;
        default:
            if ((opcode & 0xC0U) == 0x40U) {
                const uint8_t destination = static_cast<uint8_t>((opcode >> 3U) & 0x07U);
                const uint8_t source = static_cast<uint8_t>(opcode & 0x07U);
                writeRegisterOrMemory(destination, readRegisterOrMemory(source));
                registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
                elapsedCycles_ += (destination == 0x06U || source == 0x06U) ? 7U : 5U;
                break;
            }

            if ((opcode & 0xC7U) == 0x06U) {
                const uint8_t destination = static_cast<uint8_t>((opcode >> 3U) & 0x07U);
                writeRegisterOrMemory(
                    destination,
                    memory_.readByte(static_cast<uint16_t>(registers_.pc + 1U)));
                registers_.pc = static_cast<uint16_t>(registers_.pc + 2U);
                elapsedCycles_ += destination == 0x06U ? 10U : 7U;
                break;
            }

            if ((opcode & 0xCFU) == 0x01U) {
                const uint8_t pairCode = static_cast<uint8_t>((opcode >> 4U) & 0x03U);
                setRegisterPair(
                    pairCode,
                    readWord(static_cast<uint16_t>(registers_.pc + 1U)));
                registers_.pc = static_cast<uint16_t>(registers_.pc + 3U);
                elapsedCycles_ += 10U;
                break;
            }

            if ((opcode & 0xCFU) == 0x03U) {
                const uint8_t pairCode = static_cast<uint8_t>((opcode >> 4U) & 0x03U);
                setRegisterPair(
                    pairCode,
                    static_cast<uint16_t>(getRegisterPair(pairCode) + 1U));
                registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
                elapsedCycles_ += 5U;
                break;
            }

            if ((opcode & 0xCFU) == 0x0BU) {
                const uint8_t pairCode = static_cast<uint8_t>((opcode >> 4U) & 0x03U);
                setRegisterPair(
                    pairCode,
                    static_cast<uint16_t>(getRegisterPair(pairCode) - 1U));
                registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
                elapsedCycles_ += 5U;
                break;
            }

            if ((opcode & 0xC7U) == 0x04U) {
                const uint8_t target = static_cast<uint8_t>((opcode >> 3U) & 0x07U);
                writeRegisterOrMemory(target, incrementByte(readRegisterOrMemory(target)));
                registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
                elapsedCycles_ += target == 0x06U ? 10U : 5U;
                break;
            }

            if ((opcode & 0xC7U) == 0x05U) {
                const uint8_t target = static_cast<uint8_t>((opcode >> 3U) & 0x07U);
                writeRegisterOrMemory(target, decrementByte(readRegisterOrMemory(target)));
                registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
                elapsedCycles_ += target == 0x06U ? 10U : 5U;
                break;
            }

            if ((opcode & 0xF8U) == 0x80U) {
                addToAccumulator(readRegisterOrMemory(static_cast<uint8_t>(opcode & 0x07U)), false);
                registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
                elapsedCycles_ += (opcode & 0x07U) == 0x06U ? 7U : 4U;
                break;
            }

            if ((opcode & 0xF8U) == 0x88U) {
                addToAccumulator(readRegisterOrMemory(static_cast<uint8_t>(opcode & 0x07U)), true);
                registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
                elapsedCycles_ += (opcode & 0x07U) == 0x06U ? 7U : 4U;
                break;
            }

            if ((opcode & 0xF8U) == 0x90U) {
                subtractFromAccumulator(readRegisterOrMemory(static_cast<uint8_t>(opcode & 0x07U)), false);
                registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
                elapsedCycles_ += (opcode & 0x07U) == 0x06U ? 7U : 4U;
                break;
            }

            if ((opcode & 0xF8U) == 0x98U) {
                subtractFromAccumulator(readRegisterOrMemory(static_cast<uint8_t>(opcode & 0x07U)), true);
                registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
                elapsedCycles_ += (opcode & 0x07U) == 0x06U ? 7U : 4U;
                break;
            }

            if ((opcode & 0xF8U) == 0xA0U) {
                andWithAccumulator(readRegisterOrMemory(static_cast<uint8_t>(opcode & 0x07U)));
                registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
                elapsedCycles_ += (opcode & 0x07U) == 0x06U ? 7U : 4U;
                break;
            }

            if ((opcode & 0xF8U) == 0xA8U) {
                xorWithAccumulator(readRegisterOrMemory(static_cast<uint8_t>(opcode & 0x07U)));
                registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
                elapsedCycles_ += (opcode & 0x07U) == 0x06U ? 7U : 4U;
                break;
            }

            if ((opcode & 0xF8U) == 0xB0U) {
                orWithAccumulator(readRegisterOrMemory(static_cast<uint8_t>(opcode & 0x07U)));
                registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
                elapsedCycles_ += (opcode & 0x07U) == 0x06U ? 7U : 4U;
                break;
            }

            if ((opcode & 0xF8U) == 0xB8U) {
                compareWithAccumulator(readRegisterOrMemory(static_cast<uint8_t>(opcode & 0x07U)));
                registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
                elapsedCycles_ += (opcode & 0x07U) == 0x06U ? 7U : 4U;
                break;
            }

            if ((opcode & 0xC7U) == 0xC0U) {
                const uint8_t conditionCode = static_cast<uint8_t>((opcode >> 3U) & 0x07U);

                if (evaluateCondition(conditionCode)) {
                    registers_.pc = popWord();
                    elapsedCycles_ += 12U;
                } else {
                    registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
                    elapsedCycles_ += 6U;
                }

                break;
            }

            if ((opcode & 0xC7U) == 0xC2U) {
                const uint16_t address =
                    readWord(static_cast<uint16_t>(registers_.pc + 1U));

                if (evaluateCondition(static_cast<uint8_t>((opcode >> 3U) & 0x07U))) {
                    registers_.pc = address;
                } else {
                    registers_.pc = static_cast<uint16_t>(registers_.pc + 3U);
                }

                elapsedCycles_ += 10U;
                break;
            }

            if ((opcode & 0xC7U) == 0xC4U) {
                const uint16_t address =
                    readWord(static_cast<uint16_t>(registers_.pc + 1U));

                if (evaluateCondition(static_cast<uint8_t>((opcode >> 3U) & 0x07U))) {
                    callAddress(address);
                    elapsedCycles_ += 18U;
                } else {
                    registers_.pc = static_cast<uint16_t>(registers_.pc + 3U);
                    elapsedCycles_ += 9U;
                }

                break;
            }

            if ((opcode & 0xCFU) == 0xC1U) {
                const uint16_t value = popWord();
                const uint8_t pairCode = static_cast<uint8_t>((opcode >> 4U) & 0x03U);

                if (pairCode == 0x03U) {
                    registers_.setFlagRegister(static_cast<uint8_t>(value & 0x00FFU));
                    registers_.a = static_cast<uint8_t>(value >> 8U);
                } else {
                    setRegisterPair(pairCode, value);
                }

                registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
                elapsedCycles_ += 10U;
                break;
            }

            if ((opcode & 0xCFU) == 0xC5U) {
                const uint8_t pairCode = static_cast<uint8_t>((opcode >> 4U) & 0x03U);
                const uint16_t value = pairCode == 0x03U
                                           ? static_cast<uint16_t>((static_cast<uint16_t>(registers_.a) << 8U) |
                                                                   registers_.getFlagRegister())
                                           : getRegisterPair(pairCode);
                pushWord(value);
                registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
                elapsedCycles_ += 12U;
                break;
            }

            if ((opcode & 0xC7U) == 0xC7U) {
                pushWord(static_cast<uint16_t>(registers_.pc + 1U));
                registers_.pc = static_cast<uint16_t>(opcode & 0x38U);
                elapsedCycles_ += 12U;
                break;
            }

            logUndocumentedOpcode(opcode);
            registers_.pc = static_cast<uint16_t>(registers_.pc + 1U);
            elapsedCycles_ += 4U;
            break;
    }

    updateInterruptEnableState();
    servicePendingInterrupts();
}

void CPU::reset() noexcept {
    registers_.clear();
    ioBus_.reset();
    elapsedCycles_ = 0U;
    halted_ = false;
    interruptEnabled_ = false;
    interruptEnableDelay_ = 0U;
    maskRst75_ = false;
    maskRst65_ = false;
    maskRst55_ = false;
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
    pendingInterrupt_ = PendingInterrupt{
        type,
        static_cast<uint8_t>(rst == 0U ? 0xC7U : rst)};
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

uint8_t CPU::readRegisterOrMemory(uint8_t code) const noexcept {
    switch (code & 0x07U) {
        case 0x00:
            return registers_.b;
        case 0x01:
            return registers_.c;
        case 0x02:
            return registers_.d;
        case 0x03:
            return registers_.e;
        case 0x04:
            return registers_.h;
        case 0x05:
            return registers_.l;
        case 0x06:
            return memory_.readByte(registers_.getHL());
        default:
            return registers_.a;
    }
}

void CPU::writeRegisterOrMemory(uint8_t code, uint8_t value) noexcept {
    switch (code & 0x07U) {
        case 0x00:
            registers_.b = value;
            break;
        case 0x01:
            registers_.c = value;
            break;
        case 0x02:
            registers_.d = value;
            break;
        case 0x03:
            registers_.e = value;
            break;
        case 0x04:
            registers_.h = value;
            break;
        case 0x05:
            registers_.l = value;
            break;
        case 0x06:
            memory_.writeByte(registers_.getHL(), value);
            break;
        default:
            registers_.a = value;
            break;
    }
}

uint16_t CPU::readWord(uint16_t address) const noexcept {
    const uint8_t low = memory_.readByte(address);
    const uint8_t high =
        memory_.readByte(static_cast<uint16_t>(address + 1U));
    return static_cast<uint16_t>((static_cast<uint16_t>(high) << 8U) | low);
}

void CPU::writeWord(uint16_t address, uint16_t value) noexcept {
    memory_.writeByte(address, static_cast<uint8_t>(value & 0x00FFU));
    memory_.writeByte(
        static_cast<uint16_t>(address + 1U),
        static_cast<uint8_t>(value >> 8U));
}

uint16_t CPU::getRegisterPair(uint8_t pairCode) const noexcept {
    switch (pairCode & 0x03U) {
        case 0x00:
            return registers_.getBC();
        case 0x01:
            return registers_.getDE();
        case 0x02:
            return registers_.getHL();
        default:
            return registers_.sp;
    }
}

void CPU::setRegisterPair(uint8_t pairCode, uint16_t value) noexcept {
    switch (pairCode & 0x03U) {
        case 0x00:
            registers_.setBC(value);
            break;
        case 0x01:
            registers_.setDE(value);
            break;
        case 0x02:
            registers_.setHL(value);
            break;
        default:
            registers_.sp = value;
            break;
    }
}

void CPU::updateSignZeroParity(uint8_t value) noexcept {
    registers_.flags.sign = (value & 0x80U) != 0U;
    registers_.flags.zero = value == 0U;
    registers_.flags.parity = hasEvenParity(value);
}

void CPU::addToAccumulator(uint8_t value, bool withCarry) noexcept {
    const uint8_t carryIn =
        withCarry && registers_.flags.carry ? 1U : 0U;
    const uint16_t result = static_cast<uint16_t>(registers_.a) +
                            static_cast<uint16_t>(value) +
                            static_cast<uint16_t>(carryIn);
    const uint8_t resultByte = static_cast<uint8_t>(result & 0x00FFU);
    const uint8_t lowerNibble =
        static_cast<uint8_t>((registers_.a & 0x0FU) + (value & 0x0FU) + carryIn);

    registers_.a = resultByte;
    updateSignZeroParity(resultByte);
    registers_.flags.auxiliaryCarry = lowerNibble > 0x0FU;
    registers_.flags.carry = result > 0x00FFU;
}

void CPU::subtractFromAccumulator(uint8_t value, bool withBorrow) noexcept {
    const uint8_t borrowIn =
        withBorrow && registers_.flags.carry ? 1U : 0U;
    const int result = static_cast<int>(registers_.a) -
                       static_cast<int>(value) -
                       static_cast<int>(borrowIn);
    const uint8_t resultByte = static_cast<uint8_t>(result & 0x00FF);
    const int lowerNibble = static_cast<int>(registers_.a & 0x0FU) -
                            static_cast<int>(value & 0x0FU) -
                            static_cast<int>(borrowIn);

    registers_.a = resultByte;
    updateSignZeroParity(resultByte);
    registers_.flags.auxiliaryCarry = lowerNibble < 0;
    registers_.flags.carry = result < 0;
}

void CPU::compareWithAccumulator(uint8_t value) noexcept {
    const uint8_t originalAccumulator = registers_.a;
    subtractFromAccumulator(value, false);
    registers_.a = originalAccumulator;
}

void CPU::andWithAccumulator(uint8_t value) noexcept {
    registers_.a = static_cast<uint8_t>(registers_.a & value);
    updateSignZeroParity(registers_.a);
    registers_.flags.auxiliaryCarry = true;
    registers_.flags.carry = false;
}

void CPU::xorWithAccumulator(uint8_t value) noexcept {
    registers_.a = static_cast<uint8_t>(registers_.a ^ value);
    updateSignZeroParity(registers_.a);
    registers_.flags.auxiliaryCarry = false;
    registers_.flags.carry = false;
}

void CPU::orWithAccumulator(uint8_t value) noexcept {
    registers_.a = static_cast<uint8_t>(registers_.a | value);
    updateSignZeroParity(registers_.a);
    registers_.flags.auxiliaryCarry = false;
    registers_.flags.carry = false;
}

uint8_t CPU::incrementByte(uint8_t value) noexcept {
    const uint8_t result = static_cast<uint8_t>(value + 1U);
    updateSignZeroParity(result);
    registers_.flags.auxiliaryCarry =
        static_cast<uint8_t>((value & 0x0FU) + 1U) > 0x0FU;
    return result;
}

uint8_t CPU::decrementByte(uint8_t value) noexcept {
    const uint8_t result = static_cast<uint8_t>(value - 1U);
    updateSignZeroParity(result);
    registers_.flags.auxiliaryCarry = (value & 0x0FU) == 0U;
    return result;
}

void CPU::addRegisterPairToHL(uint16_t value) noexcept {
    const uint32_t result =
        static_cast<uint32_t>(registers_.getHL()) + static_cast<uint32_t>(value);
    registers_.setHL(static_cast<uint16_t>(result & 0xFFFFU));
    registers_.flags.carry = result > 0xFFFFU;
}

void CPU::decimalAdjustAccumulator() noexcept {
    uint8_t correction = 0U;
    bool carryOut = registers_.flags.carry;

    if (registers_.flags.auxiliaryCarry || (registers_.a & 0x0FU) > 9U) {
        correction = 0x06U;
    }

    if (registers_.flags.carry || registers_.a > 0x99U) {
        correction = static_cast<uint8_t>(correction | 0x60U);
        carryOut = true;
    }

    const uint16_t result =
        static_cast<uint16_t>(registers_.a) + static_cast<uint16_t>(correction);
    registers_.flags.auxiliaryCarry =
        ((registers_.a & 0x0FU) + (correction & 0x0FU)) > 0x0FU;
    registers_.a = static_cast<uint8_t>(result & 0x00FFU);
    updateSignZeroParity(registers_.a);
    registers_.flags.carry = carryOut || result > 0x00FFU;
}

bool CPU::evaluateCondition(uint8_t conditionCode) const noexcept {
    switch (conditionCode & 0x07U) {
        case 0x00:
            return !registers_.flags.zero;
        case 0x01:
            return registers_.flags.zero;
        case 0x02:
            return !registers_.flags.carry;
        case 0x03:
            return registers_.flags.carry;
        case 0x04:
            return !registers_.flags.parity;
        case 0x05:
            return registers_.flags.parity;
        case 0x06:
            return !registers_.flags.sign;
        default:
            return registers_.flags.sign;
    }
}

void CPU::pushWord(uint16_t value) noexcept {
    registers_.sp = static_cast<uint16_t>(registers_.sp - 2U);
    memory_.writeByte(registers_.sp, static_cast<uint8_t>(value & 0x00FFU));
    memory_.writeByte(
        static_cast<uint16_t>(registers_.sp + 1U),
        static_cast<uint8_t>(value >> 8U));
}

uint16_t CPU::popWord() noexcept {
    const uint16_t value = readWord(registers_.sp);
    registers_.sp = static_cast<uint16_t>(registers_.sp + 2U);
    return value;
}

void CPU::callAddress(uint16_t address) noexcept {
    pushWord(static_cast<uint16_t>(registers_.pc + 3U));
    registers_.pc = address;
}

bool CPU::servicePendingInterrupts() noexcept {
    if (!pendingInterrupt_.has_value()) {
        return false;
    }

    bool shouldService = false;
    uint16_t vector = 0U;

    switch (pendingInterrupt_->type) {
        case InterruptType::Trap:
            shouldService = true;
            vector = 0x0024U;
            break;
        case InterruptType::Rst75:
            shouldService = interruptEnabled_ && !maskRst75_;
            vector = 0x003CU;
            break;
        case InterruptType::Rst65:
            shouldService = interruptEnabled_ && !maskRst65_;
            vector = 0x0034U;
            break;
        case InterruptType::Rst55:
            shouldService = interruptEnabled_ && !maskRst55_;
            vector = 0x002CU;
            break;
        case InterruptType::Intr:
            shouldService = interruptEnabled_;
            vector = static_cast<uint16_t>(pendingInterrupt_->rstOpcode & 0x38U);
            break;
    }

    if (!shouldService) {
        return false;
    }

    halted_ = false;
    interruptEnabled_ = false;
    interruptEnableDelay_ = 0U;
    pushWord(registers_.pc);
    registers_.pc = vector;
    elapsedCycles_ += 12U;
    pendingInterrupt_.reset();
    return true;
}

void CPU::updateInterruptEnableState() noexcept {
    if (interruptEnableDelay_ == 0U) {
        return;
    }

    --interruptEnableDelay_;

    if (interruptEnableDelay_ == 0U) {
        interruptEnabled_ = true;
    }
}

void CPU::logUndocumentedOpcode(uint8_t opcode) const {
    std::cerr << "Warning: undocumented opcode 0x" << std::hex
              << static_cast<int>(opcode) << " treated as NOP.\n";
}

[[noreturn]] void CPU::throwUnimplementedOpcode(uint8_t opcode) const {
    std::ostringstream message;
    message << "Opcode 0x" << std::hex << static_cast<int>(opcode)
            << " is not implemented yet.";
    throw std::runtime_error(message.str());
}

}  // namespace Core85
