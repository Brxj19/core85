#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "core/cpu.h"

namespace {

using Core85::CPU;
using Core85::Span;

TEST(DataTransferTest, MovCopiesRegisterValues) {
    CPU cpu;
    const std::vector<uint8_t> program{
        0x06U, 0x12U,  // MVI B,12h
        0x78U          // MOV A,B
    };

    cpu.loadProgram(Span<const uint8_t>(program.data(), program.size()), 0x0000U);
    cpu.step();
    cpu.step();

    EXPECT_EQ(cpu.registers().a, 0x12U);
    EXPECT_EQ(cpu.registers().b, 0x12U);
    EXPECT_EQ(cpu.getState().pc, 0x0003U);
    EXPECT_EQ(cpu.getElapsedCycles(), 12U);
}

TEST(DataTransferTest, MviAndMovSupportMemoryThroughHL) {
    CPU cpu;
    const std::vector<uint8_t> program{
        0x21U, 0x00U, 0x40U,  // LXI H,4000h
        0x36U, 0x5AU,         // MVI M,5Ah
        0x7EU                 // MOV A,M
    };

    cpu.loadProgram(Span<const uint8_t>(program.data(), program.size()), 0x0000U);

    cpu.step();
    cpu.step();
    cpu.step();

    EXPECT_EQ(cpu.registers().getHL(), 0x4000U);
    EXPECT_EQ(cpu.memory().readByte(0x4000U), 0x5AU);
    EXPECT_EQ(cpu.registers().a, 0x5AU);
    EXPECT_EQ(cpu.getElapsedCycles(), 27U);
}

TEST(DataTransferTest, StaAndLdaRoundTripAccumulatorThroughMemory) {
    CPU cpu;
    const std::vector<uint8_t> program{
        0x3EU, 0xA5U,         // MVI A,A5h
        0x32U, 0x34U, 0x12U,  // STA 1234h
        0x3EU, 0x00U,         // MVI A,00h
        0x3AU, 0x34U, 0x12U   // LDA 1234h
    };

    cpu.loadProgram(Span<const uint8_t>(program.data(), program.size()), 0x0000U);

    for (int i = 0; i < 4; ++i) {
        cpu.step();
    }

    EXPECT_EQ(cpu.memory().readByte(0x1234U), 0xA5U);
    EXPECT_EQ(cpu.registers().a, 0xA5U);
    EXPECT_EQ(cpu.getState().pc, 0x000AU);
    EXPECT_EQ(cpu.getElapsedCycles(), 40U);
}

TEST(DataTransferTest, ShldAndLhldRoundTripRegisterPairThroughMemory) {
    CPU cpu;
    const std::vector<uint8_t> program{
        0x21U, 0x34U, 0x12U,  // LXI H,1234h
        0x22U, 0x00U, 0x20U,  // SHLD 2000h
        0x21U, 0x00U, 0x00U,  // LXI H,0000h
        0x2AU, 0x00U, 0x20U   // LHLD 2000h
    };

    cpu.loadProgram(Span<const uint8_t>(program.data(), program.size()), 0x0000U);

    for (int i = 0; i < 4; ++i) {
        cpu.step();
    }

    EXPECT_EQ(cpu.memory().readByte(0x2000U), 0x34U);
    EXPECT_EQ(cpu.memory().readByte(0x2001U), 0x12U);
    EXPECT_EQ(cpu.registers().getHL(), 0x1234U);
    EXPECT_EQ(cpu.getElapsedCycles(), 52U);
}

TEST(DataTransferTest, StaxLdaxAndXchgOperateOnRegisterPairs) {
    CPU cpu;
    const std::vector<uint8_t> program{
        0x01U, 0x00U, 0x30U,  // LXI B,3000h
        0x11U, 0x00U, 0x40U,  // LXI D,4000h
        0x21U, 0x00U, 0x50U,  // LXI H,5000h
        0x3EU, 0x77U,         // MVI A,77h
        0x02U,                // STAX B
        0x3EU, 0x00U,         // MVI A,00h
        0x0AU,                // LDAX B
        0xEBU                 // XCHG
    };

    cpu.loadProgram(Span<const uint8_t>(program.data(), program.size()), 0x0000U);

    for (int i = 0; i < 8; ++i) {
        cpu.step();
    }

    EXPECT_EQ(cpu.memory().readByte(0x3000U), 0x77U);
    EXPECT_EQ(cpu.registers().a, 0x77U);
    EXPECT_EQ(cpu.registers().getDE(), 0x5000U);
    EXPECT_EQ(cpu.registers().getHL(), 0x4000U);
    EXPECT_EQ(cpu.getElapsedCycles(), 62U);
}

TEST(DataTransferTest, InAndOutUsePortArraysAndCallback) {
    CPU cpu;
    uint8_t observedPort = 0U;
    uint8_t observedValue = 0U;
    cpu.setOnOutputWrite([&observedPort, &observedValue](uint8_t port, uint8_t value) {
        observedPort = port;
        observedValue = value;
    });
    cpu.setInputPort(0x20U, 0x4CU);

    const std::vector<uint8_t> program{
        0xDBU, 0x20U,  // IN 20h
        0xD3U, 0x21U   // OUT 21h
    };

    cpu.loadProgram(Span<const uint8_t>(program.data(), program.size()), 0x0000U);

    cpu.step();
    cpu.step();

    EXPECT_EQ(cpu.registers().a, 0x4CU);
    EXPECT_EQ(cpu.getOutputPort(0x21U), 0x4CU);
    EXPECT_EQ(observedPort, 0x21U);
    EXPECT_EQ(observedValue, 0x4CU);
    EXPECT_EQ(cpu.getElapsedCycles(), 20U);
}

}  // namespace
