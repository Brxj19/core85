#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "core/cpu.h"

namespace {

using Core85::CPU;
using Core85::Span;

TEST(ArithmeticTest, AddAndAdiUpdateAccumulatorFlagsAndCycles) {
    CPU cpu;
    const std::vector<uint8_t> program{
        0x3EU, 0x14U,  // MVI A,14h
        0x06U, 0x22U,  // MVI B,22h
        0x80U,         // ADD B
        0xC6U, 0xD0U   // ADI D0h
    };

    cpu.loadProgram(Span<const uint8_t>(program.data(), program.size()), 0x0000U);

    for (int i = 0; i < 4; ++i) {
        cpu.step();
    }

    EXPECT_EQ(cpu.registers().a, 0x06U);
    EXPECT_FALSE(cpu.registers().flags.sign);
    EXPECT_FALSE(cpu.registers().flags.zero);
    EXPECT_FALSE(cpu.registers().flags.auxiliaryCarry);
    EXPECT_TRUE(cpu.registers().flags.parity);
    EXPECT_TRUE(cpu.registers().flags.carry);
    EXPECT_EQ(cpu.getElapsedCycles(), 25U);
}

TEST(ArithmeticTest, AdcIncludesExistingCarryBit) {
    CPU cpu;
    const std::vector<uint8_t> program{
        0x3EU, 0xFFU,  // MVI A,FFh
        0xC6U, 0x01U,  // ADI 01h
        0x0EU, 0x00U,  // MVI C,00h
        0x89U          // ADC C
    };

    cpu.loadProgram(Span<const uint8_t>(program.data(), program.size()), 0x0000U);

    for (int i = 0; i < 4; ++i) {
        cpu.step();
    }

    EXPECT_EQ(cpu.registers().a, 0x01U);
    EXPECT_FALSE(cpu.registers().flags.carry);
    EXPECT_FALSE(cpu.registers().flags.zero);
    EXPECT_FALSE(cpu.registers().flags.sign);
    EXPECT_FALSE(cpu.registers().flags.parity);
}

TEST(ArithmeticTest, SubAndSuiSetBorrowRelatedFlags) {
    CPU cpu;
    const std::vector<uint8_t> program{
        0x3EU, 0x10U,  // MVI A,10h
        0x16U, 0x01U,  // MVI D,01h
        0x92U,         // SUB D
        0xD6U, 0x20U   // SUI 20h
    };

    cpu.loadProgram(Span<const uint8_t>(program.data(), program.size()), 0x0000U);

    for (int i = 0; i < 4; ++i) {
        cpu.step();
    }

    EXPECT_EQ(cpu.registers().a, 0xEFU);
    EXPECT_TRUE(cpu.registers().flags.sign);
    EXPECT_FALSE(cpu.registers().flags.zero);
    EXPECT_FALSE(cpu.registers().flags.auxiliaryCarry);
    EXPECT_FALSE(cpu.registers().flags.parity);
    EXPECT_TRUE(cpu.registers().flags.carry);
}

TEST(ArithmeticTest, SbbConsumesBorrowFlag) {
    CPU cpu;
    const std::vector<uint8_t> program{
        0x3EU, 0x00U,  // MVI A,00h
        0xD6U, 0x01U,  // SUI 01h
        0x1EU, 0x00U,  // MVI E,00h
        0x9BU          // SBB E
    };

    cpu.loadProgram(Span<const uint8_t>(program.data(), program.size()), 0x0000U);

    for (int i = 0; i < 4; ++i) {
        cpu.step();
    }

    EXPECT_EQ(cpu.registers().a, 0xFEU);
    EXPECT_TRUE(cpu.registers().flags.sign);
    EXPECT_FALSE(cpu.registers().flags.zero);
    EXPECT_FALSE(cpu.registers().flags.parity);
    EXPECT_FALSE(cpu.registers().flags.carry);
}

TEST(ArithmeticTest, InrAndDcrUpdateFlagsButPreserveCarry) {
    CPU cpu;
    cpu.registers().flags.carry = true;
    const std::vector<uint8_t> program{
        0x06U, 0x0FU,  // MVI B,0Fh
        0x04U,         // INR B
        0x05U          // DCR B
    };

    cpu.loadProgram(Span<const uint8_t>(program.data(), program.size()), 0x0000U);

    cpu.step();
    cpu.step();

    EXPECT_EQ(cpu.registers().b, 0x10U);
    EXPECT_TRUE(cpu.registers().flags.auxiliaryCarry);
    EXPECT_TRUE(cpu.registers().flags.carry);

    cpu.step();

    EXPECT_EQ(cpu.registers().b, 0x0FU);
    EXPECT_FALSE(cpu.registers().flags.sign);
    EXPECT_FALSE(cpu.registers().flags.zero);
    EXPECT_TRUE(cpu.registers().flags.parity);
    EXPECT_TRUE(cpu.registers().flags.auxiliaryCarry);
    EXPECT_TRUE(cpu.registers().flags.carry);
}

TEST(ArithmeticTest, InxAndDcxUpdateRegisterPairsWithoutTouchingFlags) {
    CPU cpu;
    cpu.registers().setFlagRegister(0xD7U);
    const std::vector<uint8_t> program{
        0x21U, 0xFFU, 0x12U,  // LXI H,12FFh
        0x23U,                // INX H
        0x2BU                 // DCX H
    };

    cpu.loadProgram(Span<const uint8_t>(program.data(), program.size()), 0x0000U);

    cpu.step();
    cpu.step();
    EXPECT_EQ(cpu.registers().getHL(), 0x1300U);
    EXPECT_EQ(cpu.registers().getFlagRegister(), 0xD7U);

    cpu.step();
    EXPECT_EQ(cpu.registers().getHL(), 0x12FFU);
    EXPECT_EQ(cpu.registers().getFlagRegister(), 0xD7U);
    EXPECT_EQ(cpu.getElapsedCycles(), 20U);
}

TEST(ArithmeticTest, AddAndSubSupportMemoryOperandAtHL) {
    CPU cpu;
    const std::vector<uint8_t> program{
        0x21U, 0x00U, 0x20U,  // LXI H,2000h
        0x36U, 0x05U,         // MVI M,05h
        0x3EU, 0x03U,         // MVI A,03h
        0x86U,                // ADD M
        0x96U                 // SUB M
    };

    cpu.loadProgram(Span<const uint8_t>(program.data(), program.size()), 0x0000U);

    for (int i = 0; i < 5; ++i) {
        cpu.step();
    }

    EXPECT_EQ(cpu.registers().a, 0x03U);
    EXPECT_FALSE(cpu.registers().flags.sign);
    EXPECT_FALSE(cpu.registers().flags.zero);
    EXPECT_TRUE(cpu.registers().flags.parity);
    EXPECT_FALSE(cpu.registers().flags.carry);
    EXPECT_EQ(cpu.getElapsedCycles(), 41U);
}

}  // namespace
