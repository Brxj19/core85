#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "core/cpu.h"

namespace {

using Core85::CPU;
using Core85::Span;

TEST(LogicalTest, AnaXraOraAndCmpUpdateFlagsAndAccumulator) {
    CPU cpu;
    const std::vector<uint8_t> program{
        0x3EU, 0xF0U,  // MVI A,F0h
        0x06U, 0x0FU,  // MVI B,0Fh
        0xA0U,         // ANA B
        0xEEU, 0xAAU,  // XRI AAh
        0xF6U, 0x05U,  // ORI 05h
        0xFEU, 0xAFU   // CPI AFh
    };

    cpu.loadProgram(Span<const uint8_t>(program.data(), program.size()), 0x0000U);

    for (int i = 0; i < 6; ++i) {
        cpu.step();
    }

    EXPECT_EQ(cpu.registers().a, 0xAFU);
    EXPECT_FALSE(cpu.registers().flags.sign);
    EXPECT_TRUE(cpu.registers().flags.zero);
    EXPECT_FALSE(cpu.registers().flags.carry);
}

TEST(LogicalTest, RotateAndCarryInstructionsMutateAccumulatorAndCarry) {
    CPU cpu;
    const std::vector<uint8_t> program{
        0x3EU, 0x81U,  // MVI A,81h
        0x07U,         // RLC => 03h, CY=1
        0x17U,         // RAL => 07h, CY=0
        0x0FU,         // RRC => 83h, CY=1
        0x1FU,         // RAR => C1h, CY=1
        0x37U,         // STC
        0x3FU          // CMC
    };

    cpu.loadProgram(Span<const uint8_t>(program.data(), program.size()), 0x0000U);

    for (int i = 0; i < 7; ++i) {
        cpu.step();
    }

    EXPECT_EQ(cpu.registers().a, 0xC1U);
    EXPECT_FALSE(cpu.registers().flags.carry);
    EXPECT_EQ(cpu.getElapsedCycles(), 31U);
}

TEST(LogicalTest, CmaAndDaaOperateOnAccumulator) {
    CPU cpu;
    const std::vector<uint8_t> program{
        0x3EU, 0x09U,  // MVI A,09h
        0xC6U, 0x09U,  // ADI 09h => 12h
        0x27U,         // DAA => 18h
        0x2FU          // CMA => E7h
    };

    cpu.loadProgram(Span<const uint8_t>(program.data(), program.size()), 0x0000U);

    for (int i = 0; i < 4; ++i) {
        cpu.step();
    }

    EXPECT_EQ(cpu.registers().a, 0xE7U);
    EXPECT_TRUE(cpu.registers().flags.parity);
}

}  // namespace
