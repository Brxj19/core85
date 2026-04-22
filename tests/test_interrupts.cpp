#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "core/cpu.h"

namespace {

using Core85::CPU;
using Core85::InterruptType;
using Core85::Span;

TEST(InterruptTest, EiEnablesInterruptsAfterFollowingInstruction) {
    CPU cpu;
    const std::vector<uint8_t> program{
        0xFBU,  // EI
        0x00U,  // NOP
        0x00U   // placeholder
    };

    cpu.loadProgram(Span<const uint8_t>(program.data(), program.size()), 0x0000U);
    cpu.raiseInterrupt(InterruptType::Rst55);

    cpu.step();
    EXPECT_EQ(cpu.getState().pc, 0x0001U);

    cpu.step();
    EXPECT_EQ(cpu.getState().pc, 0x002CU);
}

TEST(InterruptTest, TrapBypassesInterruptMasking) {
    CPU cpu;
    const std::vector<uint8_t> program{
        0xF3U,  // DI
        0x00U
    };

    cpu.loadProgram(Span<const uint8_t>(program.data(), program.size()), 0x0000U);
    cpu.raiseInterrupt(InterruptType::Trap);

    cpu.step();

    EXPECT_EQ(cpu.getState().pc, 0x0024U);
}

TEST(InterruptTest, SimMasksInterruptsAndRimReportsMaskState) {
    CPU cpu;
    const std::vector<uint8_t> program{
        0x3EU, 0x0BU,  // MVI A,0Bh => MSE + M5.5 mask
        0x30U,         // SIM
        0x20U          // RIM
    };

    cpu.loadProgram(Span<const uint8_t>(program.data(), program.size()), 0x0000U);

    cpu.step();
    cpu.step();
    cpu.step();

    EXPECT_EQ(cpu.registers().a & 0x01U, 0x01U);
}

TEST(InterruptTest, IntrUsesProvidedRstOpcodeVector) {
    CPU cpu;
    const std::vector<uint8_t> program{
        0xFBU,
        0x00U
    };

    cpu.loadProgram(Span<const uint8_t>(program.data(), program.size()), 0x0000U);
    cpu.raiseInterrupt(InterruptType::Intr, 0xE7U);  // RST 4

    cpu.step();
    cpu.step();

    EXPECT_EQ(cpu.getState().pc, 0x0020U);
}

}  // namespace
