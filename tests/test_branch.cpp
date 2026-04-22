#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "core/cpu.h"

namespace {

using Core85::CPU;
using Core85::Span;

TEST(BranchTest, JumpsCallsAndReturnsFollowConditions) {
    CPU cpu;
    const std::vector<uint8_t> program{
        0x3EU, 0x00U,        // 0000: MVI A,00h
        0xFEU, 0x00U,        // 0002: CPI 00h => Z=1
        0xCAU, 0x09U, 0x00U, // 0004: JZ 0009h
        0x00U,               // 0007: NOP (skipped)
        0x00U,               // 0008: NOP (skipped)
        0xCCU, 0x10U, 0x00U, // 0009: CZ 0010h
        0xC3U, 0x13U, 0x00U, // 000C: JMP 0013h
        0x00U,               // 000F: padding
        0x04U,               // 0010: INR B
        0xC9U,               // 0011: RET
        0x00U,               // 0012: padding
        0x05U                // 0013: DCR B
    };

    cpu.loadProgram(Span<const uint8_t>(program.data(), program.size()), 0x0000U);

    for (int i = 0; i < 8; ++i) {
        cpu.step();
    }

    EXPECT_EQ(cpu.registers().b, 0x00U);
    EXPECT_EQ(cpu.getState().pc, 0x0014U);
    EXPECT_EQ(cpu.registers().sp, 0xFFFFU);
}

TEST(BranchTest, RstPushesReturnAddressAndTransfersControl) {
    CPU cpu;
    const std::vector<uint8_t> program{
        0xEFU,  // RST 5
        0x00U,  // return target
    };

    cpu.memory().writeByte(0x0028U, 0x04U);  // INR B
    cpu.loadProgram(Span<const uint8_t>(program.data(), program.size()), 0x0000U);

    cpu.step();

    EXPECT_EQ(cpu.getState().pc, 0x0028U);
    EXPECT_EQ(cpu.memory().readByte(0xFFFD), 0x01U);
    EXPECT_EQ(cpu.memory().readByte(0xFFFE), 0x00U);

    cpu.step();
    EXPECT_EQ(cpu.registers().b, 0x01U);
}

TEST(BranchTest, PchlLoadsProgramCounterFromHL) {
    CPU cpu;
    const std::vector<uint8_t> program{
        0x21U, 0x34U, 0x12U,  // LXI H,1234h
        0xE9U                 // PCHL
    };

    cpu.loadProgram(Span<const uint8_t>(program.data(), program.size()), 0x0000U);
    cpu.step();
    cpu.step();

    EXPECT_EQ(cpu.getState().pc, 0x1234U);
    EXPECT_EQ(cpu.getElapsedCycles(), 15U);
}

}  // namespace
