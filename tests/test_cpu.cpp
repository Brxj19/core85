#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "core/cpu.h"

namespace {

using Core85::CPU;
using Core85::Flags;
using Core85::Span;

TEST(FlagsTest, EncodesNamedBitsIntoFlagRegister) {
    Flags flags;
    flags.sign = true;
    flags.zero = true;
    flags.auxiliaryCarry = true;
    flags.parity = true;
    flags.carry = true;

    EXPECT_EQ(flags.toByte(), 0xD7U);
}

TEST(FlagsTest, DecodesNamedBitsFromFlagRegister) {
    const auto flags = Flags::fromByte(0xD5U);

    EXPECT_TRUE(flags.sign);
    EXPECT_TRUE(flags.zero);
    EXPECT_TRUE(flags.auxiliaryCarry);
    EXPECT_TRUE(flags.parity);
    EXPECT_TRUE(flags.carry);
}

TEST(RegisterBankTest, RegisterPairsCombineAndSplitCorrectly) {
    CPU cpu;

    cpu.registers().setBC(0x1234U);
    cpu.registers().setDE(0xABCDU);
    cpu.registers().setHL(0xFEDCU);

    EXPECT_EQ(cpu.registers().b, 0x12U);
    EXPECT_EQ(cpu.registers().c, 0x34U);
    EXPECT_EQ(cpu.registers().d, 0xABU);
    EXPECT_EQ(cpu.registers().e, 0xCDU);
    EXPECT_EQ(cpu.registers().h, 0xFEU);
    EXPECT_EQ(cpu.registers().l, 0xDCU);
    EXPECT_EQ(cpu.registers().getBC(), 0x1234U);
    EXPECT_EQ(cpu.registers().getDE(), 0xABCDU);
    EXPECT_EQ(cpu.registers().getHL(), 0xFEDCU);
}

TEST(RegisterBankTest, FlagRegisterSanitizesReservedBits) {
    CPU cpu;

    cpu.registers().setFlagRegister(0xFFU);

    EXPECT_EQ(cpu.registers().getFlagRegister(), 0xD7U);
}

TEST(CPUTest, ResetInitializesCoreStateWithoutClearingMemory) {
    CPU cpu;
    cpu.memory().writeByte(0x0042U, 0x11U);
    cpu.registers().a = 0x9AU;
    cpu.registers().pc = 0x1234U;
    cpu.reset();

    const auto state = cpu.getState();
    EXPECT_EQ(state.a, 0x00U);
    EXPECT_EQ(state.pc, 0x0000U);
    EXPECT_EQ(state.sp, 0xFFFFU);
    EXPECT_EQ(state.elapsedCycles, 0U);
    EXPECT_FALSE(state.halted);
    EXPECT_EQ(cpu.memory().readByte(0x0042U), 0x11U);
}

TEST(CPUTest, StateSnapshotReflectsRegistersAndPairs) {
    CPU cpu;
    cpu.registers().a = 0xA5U;
    cpu.registers().setBC(0x1234U);
    cpu.registers().setDE(0x5678U);
    cpu.registers().setHL(0x9ABCU);
    cpu.registers().pc = 0x00F0U;
    cpu.registers().sp = 0xFFF0U;
    cpu.registers().flags.parity = true;

    const auto state = cpu.getState();
    EXPECT_EQ(state.a, 0xA5U);
    EXPECT_EQ(state.bc, 0x1234U);
    EXPECT_EQ(state.de, 0x5678U);
    EXPECT_EQ(state.hl, 0x9ABCU);
    EXPECT_EQ(state.pc, 0x00F0U);
    EXPECT_EQ(state.sp, 0xFFF0U);
    EXPECT_EQ(state.f, 0x06U);
}

TEST(CPUTest, LoadProgramWritesBytesAndMovesProgramCounter) {
    CPU cpu;
    const std::vector<uint8_t> bytes{0x00U, 0x76U, 0xAAU};

    cpu.loadProgram(Span<const uint8_t>(bytes.data(), bytes.size()), 0x2000U);

    EXPECT_EQ(cpu.getState().pc, 0x2000U);
    EXPECT_EQ(cpu.memory().readByte(0x2000U), 0x00U);
    EXPECT_EQ(cpu.memory().readByte(0x2001U), 0x76U);
    EXPECT_EQ(cpu.memory().readByte(0x2002U), 0xAAU);
}

TEST(CPUTest, ResetCyclesClearsCycleCounterOnly) {
    CPU cpu;
    cpu.memory().writeByte(0x0000U, 0x00U);
    cpu.step();
    ASSERT_EQ(cpu.getElapsedCycles(), 4U);

    cpu.resetCycles();

    EXPECT_EQ(cpu.getElapsedCycles(), 0U);
    EXPECT_EQ(cpu.getState().pc, 0x0001U);
}

TEST(CPUTest, StepExecutesNopInstruction) {
    CPU cpu;
    cpu.memory().writeByte(0x0000U, 0x00U);

    cpu.step();

    EXPECT_EQ(cpu.getState().pc, 0x0001U);
    EXPECT_EQ(cpu.getElapsedCycles(), 4U);
    EXPECT_FALSE(cpu.isHalted());
}

TEST(CPUTest, StepExecutesHaltInstruction) {
    CPU cpu;
    cpu.memory().writeByte(0x0000U, 0x76U);

    cpu.step();

    EXPECT_EQ(cpu.getState().pc, 0x0001U);
    EXPECT_EQ(cpu.getElapsedCycles(), 7U);
    EXPECT_TRUE(cpu.isHalted());
}

TEST(CPUTest, HaltedCpuDoesNotAdvanceWithoutInterrupt) {
    CPU cpu;
    cpu.memory().writeByte(0x0000U, 0x76U);
    cpu.step();

    cpu.step();

    EXPECT_EQ(cpu.getState().pc, 0x0001U);
    EXPECT_EQ(cpu.getElapsedCycles(), 7U);
}

TEST(CPUTest, RaisingInterruptReleasesHaltState) {
    CPU cpu;
    cpu.memory().writeByte(0x0000U, 0x76U);
    cpu.step();
    ASSERT_TRUE(cpu.isHalted());

    cpu.raiseInterrupt(Core85::InterruptType::Trap);
    cpu.step();

    EXPECT_FALSE(cpu.isHalted());
    EXPECT_EQ(cpu.getState().pc, 0x0024U);
    EXPECT_EQ(cpu.getElapsedCycles(), 19U);
}

TEST(CPUTest, UndocumentedOpcodeActsAsNop) {
    CPU cpu;
    cpu.memory().writeByte(0x0000U, 0x08U);

    cpu.step();

    EXPECT_EQ(cpu.getState().pc, 0x0001U);
    EXPECT_EQ(cpu.getElapsedCycles(), 4U);
}

TEST(CPUTest, InputPortValuesAreStoredThroughCpuApi) {
    CPU cpu;

    cpu.setInputPort(0x10U, 0x5AU);

    EXPECT_EQ(cpu.ioBus().readInputPort(0x10U), 0x5AU);
}

}  // namespace
