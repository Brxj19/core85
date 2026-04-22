#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "core/cpu.h"

namespace {

using Core85::CPU;
using Core85::Flags;
using Core85::InterruptType;
using Core85::Span;

struct OpcodeCase {
    uint8_t opcode;
    const char* name;
};

struct RegisterPairCase {
    uint8_t opcode;
    uint8_t pairCode;
    const char* name;
};

struct ConditionCase {
    uint8_t opcode;
    uint8_t conditionCode;
    const char* name;
};

std::string makeOpcodeName(const testing::TestParamInfo<OpcodeCase>& info) {
    std::ostringstream name;
    name << info.param.name << "_" << std::hex << std::uppercase
         << std::setw(2) << std::setfill('0')
         << static_cast<int>(info.param.opcode);
    return name.str();
}

std::string makePairName(const testing::TestParamInfo<RegisterPairCase>& info) {
    std::ostringstream name;
    name << info.param.name << "_" << std::hex << std::uppercase
         << std::setw(2) << std::setfill('0')
         << static_cast<int>(info.param.opcode);
    return name.str();
}

std::string makeConditionName(const testing::TestParamInfo<ConditionCase>& info) {
    std::ostringstream name;
    name << info.param.name << "_" << std::hex << std::uppercase
         << std::setw(2) << std::setfill('0')
         << static_cast<int>(info.param.opcode);
    return name.str();
}

uint8_t getRegisterByCode(const CPU& cpu, uint8_t code) {
    switch (code & 0x07U) {
        case 0x00:
            return cpu.registers().b;
        case 0x01:
            return cpu.registers().c;
        case 0x02:
            return cpu.registers().d;
        case 0x03:
            return cpu.registers().e;
        case 0x04:
            return cpu.registers().h;
        case 0x05:
            return cpu.registers().l;
        case 0x06:
            return cpu.memory().readByte(cpu.registers().getHL());
        default:
            return cpu.registers().a;
    }
}

void setRegisterByCode(CPU& cpu, uint8_t code, uint8_t value) {
    switch (code & 0x07U) {
        case 0x00:
            cpu.registers().b = value;
            break;
        case 0x01:
            cpu.registers().c = value;
            break;
        case 0x02:
            cpu.registers().d = value;
            break;
        case 0x03:
            cpu.registers().e = value;
            break;
        case 0x04:
            cpu.registers().h = value;
            break;
        case 0x05:
            cpu.registers().l = value;
            break;
        case 0x06:
            cpu.memory().writeByte(cpu.registers().getHL(), value);
            break;
        default:
            cpu.registers().a = value;
            break;
    }
}

uint16_t getPairByCode(const CPU& cpu, uint8_t pairCode) {
    switch (pairCode & 0x03U) {
        case 0x00:
            return cpu.registers().getBC();
        case 0x01:
            return cpu.registers().getDE();
        case 0x02:
            return cpu.registers().getHL();
        default:
            return cpu.registers().sp;
    }
}

void setPairByCode(CPU& cpu, uint8_t pairCode, uint16_t value) {
    switch (pairCode & 0x03U) {
        case 0x00:
            cpu.registers().setBC(value);
            break;
        case 0x01:
            cpu.registers().setDE(value);
            break;
        case 0x02:
            cpu.registers().setHL(value);
            break;
        default:
            cpu.registers().sp = value;
            break;
    }
}

void seedRegisters(CPU& cpu) {
    cpu.registers().a = 0x81U;
    cpu.registers().b = 0x11U;
    cpu.registers().c = 0x22U;
    cpu.registers().d = 0x33U;
    cpu.registers().e = 0x44U;
    cpu.registers().h = 0x20U;
    cpu.registers().l = 0x10U;
    cpu.registers().sp = 0x4000U;
    cpu.registers().flags = Flags{true, false, true, false, true};
    cpu.memory().writeByte(cpu.registers().getHL(), 0x55U);
    cpu.memory().writeByte(0x1234U, 0x5AU);
    cpu.memory().writeByte(0x1235U, 0xA5U);
    cpu.setInputPort(0x20U, 0x4CU);
}

CPU makeCpu(const std::vector<uint8_t>& bytes) {
    CPU cpu;
    cpu.loadProgram(Span<const uint8_t>(bytes.data(), bytes.size()), 0x0000U);
    seedRegisters(cpu);
    cpu.registers().pc = 0x0000U;
    return cpu;
}

void setConditionFlags(CPU& cpu, uint8_t conditionCode, bool satisfied) {
    cpu.registers().flags.zero = false;
    cpu.registers().flags.carry = false;
    cpu.registers().flags.parity = false;
    cpu.registers().flags.sign = false;

    switch (conditionCode & 0x07U) {
        case 0x00:
            cpu.registers().flags.zero = !satisfied;
            break;
        case 0x01:
            cpu.registers().flags.zero = satisfied;
            break;
        case 0x02:
            cpu.registers().flags.carry = !satisfied;
            break;
        case 0x03:
            cpu.registers().flags.carry = satisfied;
            break;
        case 0x04:
            cpu.registers().flags.parity = !satisfied;
            break;
        case 0x05:
            cpu.registers().flags.parity = satisfied;
            break;
        case 0x06:
            cpu.registers().flags.sign = !satisfied;
            break;
        default:
            cpu.registers().flags.sign = satisfied;
            break;
    }
}

std::vector<OpcodeCase> makeMovCases() {
    std::vector<OpcodeCase> cases;

    for (uint16_t opcode = 0x40U; opcode <= 0x7FU; ++opcode) {
        if (opcode == 0x76U) {
            continue;
        }

        cases.push_back(OpcodeCase{static_cast<uint8_t>(opcode), "MOV"});
    }

    return cases;
}

std::vector<OpcodeCase> makeMviCases() {
    return {
        {0x06U, "MVI_B"}, {0x0EU, "MVI_C"}, {0x16U, "MVI_D"},
        {0x1EU, "MVI_E"}, {0x26U, "MVI_H"}, {0x2EU, "MVI_L"},
        {0x36U, "MVI_M"}, {0x3EU, "MVI_A"},
    };
}

std::vector<RegisterPairCase> makeLxiCases() {
    return {
        {0x01U, 0x00U, "LXI_B"}, {0x11U, 0x01U, "LXI_D"},
        {0x21U, 0x02U, "LXI_H"}, {0x31U, 0x03U, "LXI_SP"},
    };
}

std::vector<RegisterPairCase> makePairIncrementCases(uint8_t lowBits, const char* prefix) {
    return {
        {static_cast<uint8_t>(0x00U | lowBits), 0x00U, prefix},
        {static_cast<uint8_t>(0x10U | lowBits), 0x01U, prefix},
        {static_cast<uint8_t>(0x20U | lowBits), 0x02U, prefix},
        {static_cast<uint8_t>(0x30U | lowBits), 0x03U, prefix},
    };
}

std::vector<OpcodeCase> makeRegisterOpcodeCases(uint8_t base, const char* prefix) {
    std::vector<OpcodeCase> cases;

    for (uint8_t code = 0U; code < 8U; ++code) {
        cases.push_back(OpcodeCase{
            static_cast<uint8_t>(base | (code << 3U)),
            prefix});
    }

    return cases;
}

std::vector<OpcodeCase> makeAccumulatorFamilyCases(uint8_t base, const char* prefix) {
    std::vector<OpcodeCase> cases;

    for (uint8_t source = 0U; source < 8U; ++source) {
        cases.push_back(OpcodeCase{
            static_cast<uint8_t>(base | source),
            prefix});
    }

    return cases;
}

std::vector<ConditionCase> makeConditionCases(uint8_t base, const char* prefix) {
    return {
        {static_cast<uint8_t>(base | 0x00U), 0x00U, prefix},
        {static_cast<uint8_t>(base | 0x08U), 0x01U, prefix},
        {static_cast<uint8_t>(base | 0x10U), 0x02U, prefix},
        {static_cast<uint8_t>(base | 0x18U), 0x03U, prefix},
        {static_cast<uint8_t>(base | 0x20U), 0x04U, prefix},
        {static_cast<uint8_t>(base | 0x28U), 0x05U, prefix},
        {static_cast<uint8_t>(base | 0x30U), 0x06U, prefix},
        {static_cast<uint8_t>(base | 0x38U), 0x07U, prefix},
    };
}

class MovOpcodeTest : public testing::TestWithParam<OpcodeCase> {};

TEST_P(MovOpcodeTest, ExecutesOpcodeVariant) {
    const auto param = GetParam();
    CPU cpu = makeCpu({param.opcode});
    const auto before = cpu.getState();
    const uint8_t sourceCode = static_cast<uint8_t>(param.opcode & 0x07U);
    const uint8_t destinationCode = static_cast<uint8_t>((param.opcode >> 3U) & 0x07U);
    const uint8_t sourceValue = getRegisterByCode(cpu, sourceCode);

    cpu.step();

    EXPECT_EQ(getRegisterByCode(cpu, destinationCode), sourceValue);
    EXPECT_EQ(cpu.getState().f, before.f);
    EXPECT_EQ(cpu.getState().pc, 0x0001U);
    EXPECT_EQ(cpu.getElapsedCycles(),
              destinationCode == 0x06U || sourceCode == 0x06U ? 7U : 5U);
}

INSTANTIATE_TEST_SUITE_P(OpcodeMatrix, MovOpcodeTest,
                         testing::ValuesIn(makeMovCases()), makeOpcodeName);

class MviOpcodeTest : public testing::TestWithParam<OpcodeCase> {};

TEST_P(MviOpcodeTest, ExecutesOpcodeVariant) {
    const auto param = GetParam();
    CPU cpu = makeCpu({param.opcode, 0x5CU});
    const auto beforeFlags = cpu.getState().f;
    const uint8_t destinationCode = static_cast<uint8_t>((param.opcode >> 3U) & 0x07U);

    cpu.step();

    EXPECT_EQ(getRegisterByCode(cpu, destinationCode), 0x5CU);
    EXPECT_EQ(cpu.getState().f, beforeFlags);
    EXPECT_EQ(cpu.getState().pc, 0x0002U);
    EXPECT_EQ(cpu.getElapsedCycles(), destinationCode == 0x06U ? 10U : 7U);
}

INSTANTIATE_TEST_SUITE_P(OpcodeMatrix, MviOpcodeTest,
                         testing::ValuesIn(makeMviCases()), makeOpcodeName);

class LxiOpcodeTest : public testing::TestWithParam<RegisterPairCase> {};

TEST_P(LxiOpcodeTest, ExecutesOpcodeVariant) {
    const auto param = GetParam();
    CPU cpu = makeCpu({param.opcode, 0x34U, 0x12U});
    const auto beforeFlags = cpu.getState().f;

    cpu.step();

    EXPECT_EQ(getPairByCode(cpu, param.pairCode), 0x1234U);
    EXPECT_EQ(cpu.getState().f, beforeFlags);
    EXPECT_EQ(cpu.getState().pc, 0x0003U);
    EXPECT_EQ(cpu.getElapsedCycles(), 10U);
}

INSTANTIATE_TEST_SUITE_P(OpcodeMatrix, LxiOpcodeTest,
                         testing::ValuesIn(makeLxiCases()), makePairName);

class InxOpcodeTest : public testing::TestWithParam<RegisterPairCase> {};

TEST_P(InxOpcodeTest, ExecutesOpcodeVariant) {
    const auto param = GetParam();
    CPU cpu = makeCpu({param.opcode});
    const auto beforeFlags = cpu.getState().f;
    setPairByCode(cpu, param.pairCode, 0x12FFU);

    cpu.step();

    EXPECT_EQ(getPairByCode(cpu, param.pairCode), 0x1300U);
    EXPECT_EQ(cpu.getState().f, beforeFlags);
    EXPECT_EQ(cpu.getState().pc, 0x0001U);
    EXPECT_EQ(cpu.getElapsedCycles(), 5U);
}

INSTANTIATE_TEST_SUITE_P(
    OpcodeMatrix, InxOpcodeTest,
    testing::ValuesIn(makePairIncrementCases(0x03U, "INX")), makePairName);

class DcxOpcodeTest : public testing::TestWithParam<RegisterPairCase> {};

TEST_P(DcxOpcodeTest, ExecutesOpcodeVariant) {
    const auto param = GetParam();
    CPU cpu = makeCpu({param.opcode});
    const auto beforeFlags = cpu.getState().f;
    setPairByCode(cpu, param.pairCode, 0x1300U);

    cpu.step();

    EXPECT_EQ(getPairByCode(cpu, param.pairCode), 0x12FFU);
    EXPECT_EQ(cpu.getState().f, beforeFlags);
    EXPECT_EQ(cpu.getState().pc, 0x0001U);
    EXPECT_EQ(cpu.getElapsedCycles(), 5U);
}

INSTANTIATE_TEST_SUITE_P(
    OpcodeMatrix, DcxOpcodeTest,
    testing::ValuesIn(makePairIncrementCases(0x0BU, "DCX")), makePairName);

class DstRegisterOpcodeTest : public testing::TestWithParam<OpcodeCase> {};

TEST_P(DstRegisterOpcodeTest, InrExecutesOpcodeVariant) {
    const auto param = GetParam();
    CPU cpu = makeCpu({param.opcode});
    const uint8_t targetCode = static_cast<uint8_t>((param.opcode >> 3U) & 0x07U);
    cpu.registers().flags.carry = true;
    setRegisterByCode(cpu, targetCode, 0x0FU);

    cpu.step();

    EXPECT_EQ(getRegisterByCode(cpu, targetCode), 0x10U);
    EXPECT_TRUE(cpu.registers().flags.auxiliaryCarry);
    EXPECT_TRUE(cpu.registers().flags.carry);
    EXPECT_EQ(cpu.getState().pc, 0x0001U);
    EXPECT_EQ(cpu.getElapsedCycles(), targetCode == 0x06U ? 10U : 5U);
}

TEST_P(DstRegisterOpcodeTest, DcrExecutesOpcodeVariant) {
    const auto param = GetParam();
    CPU cpu = makeCpu({static_cast<uint8_t>(param.opcode | 0x01U)});
    const uint8_t targetCode =
        static_cast<uint8_t>(((param.opcode | 0x01U) >> 3U) & 0x07U);
    cpu.registers().flags.carry = true;
    setRegisterByCode(cpu, targetCode, 0x10U);

    cpu.step();

    EXPECT_EQ(getRegisterByCode(cpu, targetCode), 0x0FU);
    EXPECT_TRUE(cpu.registers().flags.auxiliaryCarry);
    EXPECT_TRUE(cpu.registers().flags.carry);
    EXPECT_EQ(cpu.getState().pc, 0x0001U);
    EXPECT_EQ(cpu.getElapsedCycles(), targetCode == 0x06U ? 10U : 5U);
}

INSTANTIATE_TEST_SUITE_P(
    OpcodeMatrix, DstRegisterOpcodeTest,
    testing::ValuesIn(makeRegisterOpcodeCases(0x04U, "REG")), makeOpcodeName);

class StaxLdaxOpcodeTest : public testing::TestWithParam<OpcodeCase> {};

TEST_P(StaxLdaxOpcodeTest, ExecutesOpcodeVariant) {
    const auto param = GetParam();
    CPU cpu = makeCpu({param.opcode});
    const uint16_t address = (param.opcode & 0x10U) == 0U ? 0x3456U : 0x4567U;

    if ((param.opcode & 0x08U) == 0U) {
        cpu.registers().a = 0x7BU;
        if ((param.opcode & 0x10U) == 0U) {
            cpu.registers().setBC(address);
        } else {
            cpu.registers().setDE(address);
        }

        cpu.step();
        EXPECT_EQ(cpu.memory().readByte(address), 0x7BU);
    } else {
        cpu.memory().writeByte(address, 0x39U);
        if ((param.opcode & 0x10U) == 0U) {
            cpu.registers().setBC(address);
        } else {
            cpu.registers().setDE(address);
        }

        cpu.step();
        EXPECT_EQ(cpu.registers().a, 0x39U);
    }

    EXPECT_EQ(cpu.getState().pc, 0x0001U);
    EXPECT_EQ(cpu.getElapsedCycles(), 7U);
}

INSTANTIATE_TEST_SUITE_P(
    OpcodeMatrix, StaxLdaxOpcodeTest,
    testing::Values(OpcodeCase{0x02U, "STAX_B"}, OpcodeCase{0x12U, "STAX_D"},
                    OpcodeCase{0x0AU, "LDAX_B"}, OpcodeCase{0x1AU, "LDAX_D"}),
    makeOpcodeName);

class DadOpcodeTest : public testing::TestWithParam<RegisterPairCase> {};

TEST_P(DadOpcodeTest, ExecutesOpcodeVariant) {
    const auto param = GetParam();
    CPU cpu = makeCpu({param.opcode});
    cpu.registers().setHL(0x1111U);
    setPairByCode(cpu, param.pairCode, 0x2222U);

    cpu.step();

    EXPECT_EQ(cpu.registers().getHL(), param.pairCode == 0x02U ? 0x4444U : 0x3333U);
    EXPECT_FALSE(cpu.registers().flags.carry);
    EXPECT_EQ(cpu.getState().pc, 0x0001U);
    EXPECT_EQ(cpu.getElapsedCycles(), 10U);
}

INSTANTIATE_TEST_SUITE_P(
    OpcodeMatrix, DadOpcodeTest,
    testing::ValuesIn(makePairIncrementCases(0x09U, "DAD")), makePairName);

class AluOpcodeTest : public testing::TestWithParam<OpcodeCase> {};

TEST_P(AluOpcodeTest, ExecutesOpcodeVariant) {
    const auto param = GetParam();
    CPU cpu = makeCpu({param.opcode});
    const uint8_t sourceCode = static_cast<uint8_t>(param.opcode & 0x07U);
    const uint8_t category = static_cast<uint8_t>(param.opcode & 0xF8U);
    const uint8_t initialAccumulator =
        (category == 0x90U || category == 0x98U || category == 0xB8U) ? 0x44U : 0x14U;
    cpu.registers().a = initialAccumulator;
    if (sourceCode != 0x07U) {
        setRegisterByCode(cpu, sourceCode, 0x22U);
    }
    const uint8_t sourceValue = getRegisterByCode(cpu, sourceCode);
    const uint8_t carryIn = cpu.registers().flags.carry ? 1U : 0U;

    cpu.step();

    EXPECT_EQ(cpu.getState().pc, 0x0001U);
    EXPECT_EQ(cpu.getElapsedCycles(), sourceCode == 0x06U ? 7U : 4U);

    switch (category) {
        case 0x80U:
            EXPECT_EQ(cpu.registers().a,
                      static_cast<uint8_t>(initialAccumulator + sourceValue));
            break;
        case 0x88U:
            EXPECT_EQ(cpu.registers().a,
                      static_cast<uint8_t>(initialAccumulator + sourceValue + carryIn));
            break;
        case 0x90U:
            EXPECT_EQ(cpu.registers().a,
                      static_cast<uint8_t>(initialAccumulator - sourceValue));
            break;
        case 0x98U:
            EXPECT_EQ(cpu.registers().a,
                      static_cast<uint8_t>(initialAccumulator - sourceValue - carryIn));
            break;
        case 0xA0U:
            EXPECT_EQ(cpu.registers().a,
                      static_cast<uint8_t>(initialAccumulator & sourceValue));
            EXPECT_TRUE(cpu.registers().flags.auxiliaryCarry);
            break;
        case 0xA8U:
            EXPECT_EQ(cpu.registers().a,
                      static_cast<uint8_t>(initialAccumulator ^ sourceValue));
            EXPECT_FALSE(cpu.registers().flags.carry);
            break;
        case 0xB0U:
            EXPECT_EQ(cpu.registers().a,
                      static_cast<uint8_t>(initialAccumulator | sourceValue));
            EXPECT_FALSE(cpu.registers().flags.carry);
            break;
        default:
            EXPECT_EQ(cpu.registers().a, initialAccumulator);
            EXPECT_EQ(cpu.registers().flags.zero, initialAccumulator == sourceValue);
            break;
    }
}

INSTANTIATE_TEST_SUITE_P(
    OpcodeMatrix, AluOpcodeTest,
    testing::ValuesIn([] {
        std::vector<OpcodeCase> cases;
        const std::array<uint8_t, 8> bases{
            0x80U, 0x88U, 0x90U, 0x98U, 0xA0U, 0xA8U, 0xB0U, 0xB8U};

        for (const uint8_t base : bases) {
            auto family = makeAccumulatorFamilyCases(base, "ALU");
            cases.insert(cases.end(), family.begin(), family.end());
        }

        return cases;
    }()),
    makeOpcodeName);

class ImmediateOpcodeTest : public testing::TestWithParam<OpcodeCase> {};

TEST_P(ImmediateOpcodeTest, ExecutesOpcodeVariant) {
    const auto param = GetParam();
    CPU cpu = makeCpu({param.opcode, 0x22U});
    cpu.registers().a = 0x44U;
    cpu.registers().flags.carry = true;

    cpu.step();

    EXPECT_EQ(cpu.getState().pc, 0x0002U);
    EXPECT_EQ(cpu.getElapsedCycles(), 7U);

    switch (param.opcode) {
        case 0xC6U:
            EXPECT_EQ(cpu.registers().a, 0x66U);
            break;
        case 0xCEU:
            EXPECT_EQ(cpu.registers().a, 0x67U);
            break;
        case 0xD6U:
            EXPECT_EQ(cpu.registers().a, 0x22U);
            break;
        case 0xDEU:
            EXPECT_EQ(cpu.registers().a, 0x21U);
            break;
        case 0xE6U:
            EXPECT_EQ(cpu.registers().a, 0x00U);
            EXPECT_TRUE(cpu.registers().flags.auxiliaryCarry);
            break;
        case 0xEEU:
            EXPECT_EQ(cpu.registers().a, 0x66U);
            break;
        case 0xF6U:
            EXPECT_EQ(cpu.registers().a, 0x66U);
            break;
        default:
            EXPECT_EQ(cpu.registers().a, 0x44U);
            EXPECT_FALSE(cpu.registers().flags.zero);
            break;
    }
}

INSTANTIATE_TEST_SUITE_P(
    OpcodeMatrix, ImmediateOpcodeTest,
    testing::Values(
        OpcodeCase{0xC6U, "ADI"}, OpcodeCase{0xCEU, "ACI"},
        OpcodeCase{0xD6U, "SUI"}, OpcodeCase{0xDEU, "SBI"},
        OpcodeCase{0xE6U, "ANI"}, OpcodeCase{0xEEU, "XRI"},
        OpcodeCase{0xF6U, "ORI"}, OpcodeCase{0xFEU, "CPI"}),
    makeOpcodeName);

class DirectOpcodeTest : public testing::TestWithParam<OpcodeCase> {};

TEST_P(DirectOpcodeTest, ExecutesOpcodeVariant) {
    const auto param = GetParam();
    CPU cpu = makeCpu({param.opcode, 0x34U, 0x12U});

    switch (param.opcode) {
        case 0x22U:
            cpu.registers().setHL(0xABCDU);
            break;
        case 0x2AU:
            cpu.memory().writeByte(0x1234U, 0xCDU);
            cpu.memory().writeByte(0x1235U, 0xABU);
            break;
        case 0x32U:
            cpu.registers().a = 0x5AU;
            break;
        case 0x3AU:
            cpu.memory().writeByte(0x1234U, 0x5AU);
            break;
        default:
            break;
    }

    cpu.step();

    EXPECT_EQ(cpu.getState().pc, 0x0003U);

    switch (param.opcode) {
        case 0x22U:
            EXPECT_EQ(cpu.memory().readByte(0x1234U), 0xCDU);
            EXPECT_EQ(cpu.memory().readByte(0x1235U), 0xABU);
            EXPECT_EQ(cpu.getElapsedCycles(), 16U);
            break;
        case 0x2AU:
            EXPECT_EQ(cpu.registers().getHL(), 0xABCDU);
            EXPECT_EQ(cpu.getElapsedCycles(), 16U);
            break;
        case 0x32U:
            EXPECT_EQ(cpu.memory().readByte(0x1234U), 0x5AU);
            EXPECT_EQ(cpu.getElapsedCycles(), 13U);
            break;
        default:
            EXPECT_EQ(cpu.registers().a, 0x5AU);
            EXPECT_EQ(cpu.getElapsedCycles(), 13U);
            break;
    }
}

INSTANTIATE_TEST_SUITE_P(
    OpcodeMatrix, DirectOpcodeTest,
    testing::Values(OpcodeCase{0x22U, "SHLD"}, OpcodeCase{0x2AU, "LHLD"},
                    OpcodeCase{0x32U, "STA"}, OpcodeCase{0x3AU, "LDA"}),
    makeOpcodeName);

class SimpleOpcodeTest : public testing::TestWithParam<OpcodeCase> {};

TEST_P(SimpleOpcodeTest, ExecutesOpcodeVariant) {
    const auto param = GetParam();
    CPU cpu = makeCpu({param.opcode});

    switch (param.opcode) {
        case 0x07U:
            cpu.registers().a = 0x81U;
            break;
        case 0x0FU:
            cpu.registers().a = 0x81U;
            break;
        case 0x17U:
        case 0x1FU:
            cpu.registers().a = 0x81U;
            cpu.registers().flags.carry = true;
            break;
        case 0x27U:
            cpu.registers().a = 0x12U;
            cpu.registers().flags.auxiliaryCarry = true;
            cpu.registers().flags.carry = false;
            break;
        case 0x2FU:
            cpu.registers().a = 0x55U;
            break;
        case 0x30U:
            cpu.registers().a = 0x0BU;
            break;
        case 0x37U:
            cpu.registers().flags.carry = false;
            break;
        case 0x3FU:
            cpu.registers().flags.carry = true;
            break;
        case 0x76U:
            break;
        case 0xE3U:
            cpu.registers().sp = 0x3000U;
            cpu.registers().setHL(0x5678U);
            cpu.memory().writeByte(0x3000U, 0x34U);
            cpu.memory().writeByte(0x3001U, 0x12U);
            break;
        case 0xE9U:
        case 0xF9U:
            cpu.registers().setHL(0x1234U);
            break;
        case 0xEBU:
            cpu.registers().setDE(0x5678U);
            cpu.registers().setHL(0x1234U);
            break;
        case 0xF3U:
            break;
        case 0xFBU:
            break;
        default:
            break;
    }

    if (param.opcode == 0x20U) {
        cpu.raiseInterrupt(InterruptType::Rst75);
    }

    cpu.step();

    switch (param.opcode) {
        case 0x00U:
            EXPECT_EQ(cpu.getState().pc, 0x0001U);
            EXPECT_EQ(cpu.getElapsedCycles(), 4U);
            break;
        case 0x07U:
            EXPECT_EQ(cpu.registers().a, 0x03U);
            EXPECT_TRUE(cpu.registers().flags.carry);
            break;
        case 0x0FU:
            EXPECT_EQ(cpu.registers().a, 0xC0U);
            EXPECT_TRUE(cpu.registers().flags.carry);
            break;
        case 0x17U:
            EXPECT_EQ(cpu.registers().a, 0x03U);
            EXPECT_TRUE(cpu.registers().flags.carry);
            break;
        case 0x1FU:
            EXPECT_EQ(cpu.registers().a, 0xC0U);
            EXPECT_TRUE(cpu.registers().flags.carry);
            break;
        case 0x20U:
            EXPECT_NE(cpu.registers().a & 0x40U, 0x00U);
            break;
        case 0x27U:
            EXPECT_EQ(cpu.registers().a, 0x18U);
            break;
        case 0x2FU:
            EXPECT_EQ(cpu.registers().a, 0xAAU);
            break;
        case 0x30U:
            EXPECT_EQ(cpu.getState().pc, 0x0001U);
            break;
        case 0x37U:
            EXPECT_TRUE(cpu.registers().flags.carry);
            break;
        case 0x3FU:
            EXPECT_FALSE(cpu.registers().flags.carry);
            break;
        case 0x76U:
            EXPECT_TRUE(cpu.isHalted());
            break;
        case 0xE3U:
            EXPECT_EQ(cpu.registers().getHL(), 0x1234U);
            break;
        case 0xE9U:
            EXPECT_EQ(cpu.getState().pc, 0x1234U);
            break;
        case 0xEBU:
            EXPECT_EQ(cpu.registers().getDE(), 0x1234U);
            EXPECT_EQ(cpu.registers().getHL(), 0x5678U);
            break;
        case 0xF3U:
        case 0xFBU:
            EXPECT_EQ(cpu.getElapsedCycles(), 4U);
            break;
        default:
            EXPECT_EQ(cpu.registers().sp, 0x1234U);
            break;
    }
}

INSTANTIATE_TEST_SUITE_P(
    OpcodeMatrix, SimpleOpcodeTest,
    testing::Values(
        OpcodeCase{0x00U, "NOP"}, OpcodeCase{0x07U, "RLC"},
        OpcodeCase{0x0FU, "RRC"}, OpcodeCase{0x17U, "RAL"},
        OpcodeCase{0x1FU, "RAR"}, OpcodeCase{0x20U, "RIM"},
        OpcodeCase{0x27U, "DAA"}, OpcodeCase{0x2FU, "CMA"},
        OpcodeCase{0x30U, "SIM"}, OpcodeCase{0x37U, "STC"},
        OpcodeCase{0x3FU, "CMC"}, OpcodeCase{0x76U, "HLT"},
        OpcodeCase{0xE3U, "XTHL"}, OpcodeCase{0xE9U, "PCHL"},
        OpcodeCase{0xEBU, "XCHG"}, OpcodeCase{0xF3U, "DI"},
        OpcodeCase{0xF9U, "SPHL"}, OpcodeCase{0xFBU, "EI"}),
    makeOpcodeName);

class InOutOpcodeTest : public testing::TestWithParam<OpcodeCase> {};

TEST_P(InOutOpcodeTest, ExecutesOpcodeVariant) {
    const auto param = GetParam();
    CPU cpu = makeCpu({param.opcode, 0x20U});

    if (param.opcode == 0xD3U) {
        cpu.registers().a = 0x99U;
    }

    cpu.step();

    EXPECT_EQ(cpu.getState().pc, 0x0002U);
    EXPECT_EQ(cpu.getElapsedCycles(), 10U);

    if (param.opcode == 0xD3U) {
        EXPECT_EQ(cpu.getOutputPort(0x20U), 0x99U);
    } else {
        EXPECT_EQ(cpu.registers().a, 0x4CU);
    }
}

INSTANTIATE_TEST_SUITE_P(
    OpcodeMatrix, InOutOpcodeTest,
    testing::Values(OpcodeCase{0xD3U, "OUT"}, OpcodeCase{0xDBU, "IN"}),
    makeOpcodeName);

class PushOpcodeTest : public testing::TestWithParam<RegisterPairCase> {};

TEST_P(PushOpcodeTest, ExecutesOpcodeVariant) {
    const auto param = GetParam();
    CPU cpu = makeCpu({param.opcode});
    cpu.registers().sp = 0x3000U;
    if (param.pairCode != 0x03U) {
        setPairByCode(cpu, param.pairCode, 0x1234U);
    }
    cpu.registers().a = 0x56U;
    cpu.registers().setFlagRegister(0xD7U);

    cpu.step();

    EXPECT_EQ(cpu.registers().sp, 0x2FFEU);
    EXPECT_EQ(cpu.getState().pc, 0x0001U);
    EXPECT_EQ(cpu.getElapsedCycles(), 12U);

    if (param.pairCode == 0x03U) {
        EXPECT_EQ(cpu.memory().readByte(0x2FFEU), cpu.registers().getFlagRegister());
        EXPECT_EQ(cpu.memory().readByte(0x2FFFU), 0x56U);
    } else {
        EXPECT_EQ(cpu.memory().readByte(0x2FFEU), 0x34U);
        EXPECT_EQ(cpu.memory().readByte(0x2FFFU), 0x12U);
    }
}

INSTANTIATE_TEST_SUITE_P(
    OpcodeMatrix, PushOpcodeTest,
    testing::Values(
        RegisterPairCase{0xC5U, 0x00U, "PUSH_B"},
        RegisterPairCase{0xD5U, 0x01U, "PUSH_D"},
        RegisterPairCase{0xE5U, 0x02U, "PUSH_H"},
        RegisterPairCase{0xF5U, 0x03U, "PUSH_PSW"}),
    makePairName);

class PopOpcodeTest : public testing::TestWithParam<RegisterPairCase> {};

TEST_P(PopOpcodeTest, ExecutesOpcodeVariant) {
    const auto param = GetParam();
    CPU cpu = makeCpu({param.opcode});
    cpu.registers().sp = 0x3000U;
    cpu.memory().writeByte(0x3000U, 0x34U);
    cpu.memory().writeByte(0x3001U, 0x12U);

    cpu.step();

    EXPECT_EQ(cpu.registers().sp, 0x3002U);
    EXPECT_EQ(cpu.getState().pc, 0x0001U);
    EXPECT_EQ(cpu.getElapsedCycles(), 10U);

    if (param.pairCode == 0x03U) {
        EXPECT_EQ(cpu.registers().a, 0x12U);
        EXPECT_EQ(cpu.registers().getFlagRegister(), 0x16U);
    } else {
        EXPECT_EQ(getPairByCode(cpu, param.pairCode), 0x1234U);
    }
}

INSTANTIATE_TEST_SUITE_P(
    OpcodeMatrix, PopOpcodeTest,
    testing::Values(
        RegisterPairCase{0xC1U, 0x00U, "POP_B"},
        RegisterPairCase{0xD1U, 0x01U, "POP_D"},
        RegisterPairCase{0xE1U, 0x02U, "POP_H"},
        RegisterPairCase{0xF1U, 0x03U, "POP_PSW"}),
    makePairName);

class JumpOpcodeTest : public testing::TestWithParam<ConditionCase> {};

TEST_P(JumpOpcodeTest, ExecutesTakenConditionalVariant) {
    const auto param = GetParam();
    CPU cpu = makeCpu({param.opcode, 0x34U, 0x12U});
    setConditionFlags(cpu, param.conditionCode, true);

    cpu.step();

    EXPECT_EQ(cpu.getState().pc, 0x1234U);
    EXPECT_EQ(cpu.getElapsedCycles(), 10U);
}

INSTANTIATE_TEST_SUITE_P(
    OpcodeMatrix, JumpOpcodeTest,
    testing::ValuesIn(makeConditionCases(0xC2U, "JCOND")), makeConditionName);

class CallOpcodeTest : public testing::TestWithParam<ConditionCase> {};

TEST_P(CallOpcodeTest, ExecutesTakenConditionalVariant) {
    const auto param = GetParam();
    CPU cpu = makeCpu({param.opcode, 0x34U, 0x12U});
    cpu.registers().sp = 0x3000U;
    setConditionFlags(cpu, param.conditionCode, true);

    cpu.step();

    EXPECT_EQ(cpu.getState().pc, 0x1234U);
    EXPECT_EQ(cpu.registers().sp, 0x2FFEU);
    EXPECT_EQ(cpu.memory().readByte(0x2FFEU), 0x03U);
    EXPECT_EQ(cpu.memory().readByte(0x2FFFU), 0x00U);
    EXPECT_EQ(cpu.getElapsedCycles(), 18U);
}

INSTANTIATE_TEST_SUITE_P(
    OpcodeMatrix, CallOpcodeTest,
    testing::ValuesIn(makeConditionCases(0xC4U, "CCOND")), makeConditionName);

class ReturnOpcodeTest : public testing::TestWithParam<ConditionCase> {};

TEST_P(ReturnOpcodeTest, ExecutesTakenConditionalVariant) {
    const auto param = GetParam();
    CPU cpu = makeCpu({param.opcode});
    cpu.registers().sp = 0x3000U;
    cpu.memory().writeByte(0x3000U, 0x34U);
    cpu.memory().writeByte(0x3001U, 0x12U);
    setConditionFlags(cpu, param.conditionCode, true);

    cpu.step();

    EXPECT_EQ(cpu.getState().pc, 0x1234U);
    EXPECT_EQ(cpu.registers().sp, 0x3002U);
    EXPECT_EQ(cpu.getElapsedCycles(), 12U);
}

INSTANTIATE_TEST_SUITE_P(
    OpcodeMatrix, ReturnOpcodeTest,
    testing::ValuesIn(makeConditionCases(0xC0U, "RCOND")), makeConditionName);

class UntakenConditionalOpcodeTest : public testing::TestWithParam<ConditionCase> {};

TEST_P(UntakenConditionalOpcodeTest, JumpNotTakenUsesUntakenTiming) {
    const auto param = GetParam();
    CPU cpu = makeCpu({static_cast<uint8_t>(param.opcode | 0x02U), 0x34U, 0x12U});
    setConditionFlags(cpu, param.conditionCode, false);

    cpu.step();

    EXPECT_EQ(cpu.getState().pc, 0x0003U);
    EXPECT_EQ(cpu.getElapsedCycles(), 10U);
}

TEST_P(UntakenConditionalOpcodeTest, CallNotTakenUsesUntakenTiming) {
    const auto param = GetParam();
    CPU cpu = makeCpu({static_cast<uint8_t>(param.opcode | 0x04U), 0x34U, 0x12U});
    cpu.registers().sp = 0x3000U;
    setConditionFlags(cpu, param.conditionCode, false);

    cpu.step();

    EXPECT_EQ(cpu.getState().pc, 0x0003U);
    EXPECT_EQ(cpu.registers().sp, 0x3000U);
    EXPECT_EQ(cpu.getElapsedCycles(), 9U);
}

TEST_P(UntakenConditionalOpcodeTest, ReturnNotTakenUsesUntakenTiming) {
    const auto param = GetParam();
    CPU cpu = makeCpu({static_cast<uint8_t>(param.opcode | 0x00U)});
    cpu.registers().sp = 0x3000U;
    setConditionFlags(cpu, param.conditionCode, false);

    cpu.step();

    EXPECT_EQ(cpu.getState().pc, 0x0001U);
    EXPECT_EQ(cpu.registers().sp, 0x3000U);
    EXPECT_EQ(cpu.getElapsedCycles(), 6U);
}

INSTANTIATE_TEST_SUITE_P(
    OpcodeMatrix, UntakenConditionalOpcodeTest,
    testing::Values(
        ConditionCase{0xC0U, 0x00U, "COND_NZ"},
        ConditionCase{0xC8U, 0x01U, "COND_Z"},
        ConditionCase{0xD0U, 0x02U, "COND_NC"},
        ConditionCase{0xD8U, 0x03U, "COND_C"},
        ConditionCase{0xE0U, 0x04U, "COND_PO"},
        ConditionCase{0xE8U, 0x05U, "COND_PE"},
        ConditionCase{0xF0U, 0x06U, "COND_P"},
        ConditionCase{0xF8U, 0x07U, "COND_M"}),
    makeConditionName);

class UnconditionalBranchOpcodeTest : public testing::TestWithParam<OpcodeCase> {};

TEST_P(UnconditionalBranchOpcodeTest, ExecutesOpcodeVariant) {
    const auto param = GetParam();
    CPU cpu = makeCpu({param.opcode, 0x34U, 0x12U});
    cpu.registers().sp = 0x3000U;
    cpu.memory().writeByte(0x3000U, 0x78U);
    cpu.memory().writeByte(0x3001U, 0x56U);

    cpu.step();

    switch (param.opcode) {
        case 0xC3U:
            EXPECT_EQ(cpu.getState().pc, 0x1234U);
            EXPECT_EQ(cpu.getElapsedCycles(), 10U);
            break;
        case 0xC9U:
            EXPECT_EQ(cpu.getState().pc, 0x5678U);
            EXPECT_EQ(cpu.getElapsedCycles(), 10U);
            break;
        default:
            EXPECT_EQ(cpu.getState().pc, 0x1234U);
            EXPECT_EQ(cpu.registers().sp, 0x2FFEU);
            EXPECT_EQ(cpu.getElapsedCycles(), 18U);
            break;
    }
}

INSTANTIATE_TEST_SUITE_P(
    OpcodeMatrix, UnconditionalBranchOpcodeTest,
    testing::Values(OpcodeCase{0xC3U, "JMP"}, OpcodeCase{0xC9U, "RET"},
                    OpcodeCase{0xCDU, "CALL"}),
    makeOpcodeName);

class RstOpcodeTest : public testing::TestWithParam<OpcodeCase> {};

TEST_P(RstOpcodeTest, ExecutesOpcodeVariant) {
    const auto param = GetParam();
    CPU cpu = makeCpu({param.opcode});
    cpu.registers().sp = 0x3000U;

    cpu.step();

    EXPECT_EQ(cpu.getState().pc, static_cast<uint16_t>(param.opcode & 0x38U));
    EXPECT_EQ(cpu.registers().sp, 0x2FFEU);
    EXPECT_EQ(cpu.memory().readByte(0x2FFEU), 0x01U);
    EXPECT_EQ(cpu.memory().readByte(0x2FFFU), 0x00U);
    EXPECT_EQ(cpu.getElapsedCycles(), 12U);
}

INSTANTIATE_TEST_SUITE_P(
    OpcodeMatrix, RstOpcodeTest,
    testing::Values(
        OpcodeCase{0xC7U, "RST_0"}, OpcodeCase{0xCFU, "RST_1"},
        OpcodeCase{0xD7U, "RST_2"}, OpcodeCase{0xDFU, "RST_3"},
        OpcodeCase{0xE7U, "RST_4"}, OpcodeCase{0xEFU, "RST_5"},
        OpcodeCase{0xF7U, "RST_6"}, OpcodeCase{0xFFU, "RST_7"}),
    makeOpcodeName);

TEST(OpcodeMatrixCoverageTest, ValidOpcodeCountIs246) {
    std::size_t validCount = 0U;

    const std::array<uint8_t, 10> invalidOpcodes{
        0x08U, 0x10U, 0x18U, 0x28U, 0x38U,
        0xCBU, 0xD9U, 0xDDU, 0xEDU, 0xFDU};

    for (uint16_t opcode = 0U; opcode < 256U; ++opcode) {
        bool invalid = false;

        for (const uint8_t candidate : invalidOpcodes) {
            if (candidate == opcode) {
                invalid = true;
                break;
            }
        }

        if (!invalid) {
            ++validCount;
        }
    }

    EXPECT_EQ(validCount, 246U);
}

}  // namespace
