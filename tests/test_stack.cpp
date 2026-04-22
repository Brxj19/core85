#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "core/cpu.h"

namespace {

using Core85::CPU;
using Core85::Span;

TEST(StackTest, PushAndPopRestoreRegisterPairsAndPsw) {
    CPU cpu;
    const std::vector<uint8_t> program{
        0x01U, 0x34U, 0x12U,  // LXI B,1234h
        0x3EU, 0x80U,         // MVI A,80h
        0xF5U,                // PUSH PSW
        0xC5U,                // PUSH B
        0x01U, 0x00U, 0x00U,  // LXI B,0000h
        0x3EU, 0x00U,         // MVI A,00h
        0xC1U,                // POP B
        0xF1U                 // POP PSW
    };

    cpu.registers().flags.sign = true;
    cpu.loadProgram(Span<const uint8_t>(program.data(), program.size()), 0x0000U);

    for (int i = 0; i < 8; ++i) {
        cpu.step();
    }

    EXPECT_EQ(cpu.registers().a, 0x80U);
    EXPECT_EQ(cpu.registers().getBC(), 0x1234U);
    EXPECT_TRUE(cpu.registers().flags.sign);
    EXPECT_EQ(cpu.registers().sp, 0xFFFFU);
}

TEST(StackTest, XthlAndSphlUseStackAndHlRegisters) {
    CPU cpu;
    const std::vector<uint8_t> program{
        0x31U, 0x00U, 0x20U,  // LXI SP,2000h
        0x21U, 0x78U, 0x56U,  // LXI H,5678h
        0xE3U,                // XTHL
        0xF9U                 // SPHL
    };

    cpu.memory().writeByte(0x2000U, 0x34U);
    cpu.memory().writeByte(0x2001U, 0x12U);
    cpu.loadProgram(Span<const uint8_t>(program.data(), program.size()), 0x0000U);

    for (int i = 0; i < 4; ++i) {
        cpu.step();
    }

    EXPECT_EQ(cpu.registers().getHL(), 0x1234U);
    EXPECT_EQ(cpu.memory().readByte(0x2000U), 0x78U);
    EXPECT_EQ(cpu.memory().readByte(0x2001U), 0x56U);
    EXPECT_EQ(cpu.registers().sp, 0x1234U);
}

}  // namespace
