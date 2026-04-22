#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "core/assembler.h"

namespace {

using Core85::Assembler;

struct AssemblerCase {
    std::string name;
    std::string source;
    uint16_t origin = 0U;
    std::vector<uint8_t> bytes;
};

AssemblerCase makeCase(std::string name,
                       std::string body,
                       std::vector<uint8_t> bytes,
                       uint16_t origin = 0x1000U) {
    return AssemblerCase{
        std::move(name),
        ".ORG " + std::string(origin == 0x1000U ? "1000H" : origin == 0x2000U ? "2000H" : "1234H") +
            "\n" + std::move(body),
        origin,
        std::move(bytes)};
}

std::string sanitizeName(const testing::TestParamInfo<AssemblerCase>& info) {
    std::string name;
    name.reserve(info.param.name.size());

    for (const char ch : info.param.name) {
        if (std::isalnum(static_cast<unsigned char>(ch)) != 0) {
            name.push_back(ch);
        } else {
            name.push_back('_');
        }
    }

    return name;
}

std::vector<AssemblerCase> makeMnemonicCases() {
    return {
        makeCase("NOP", "NOP\n", {0x00U}),
        makeCase("MOV", "MOV A,M\n", {0x7EU}),
        makeCase("MVI", "MVI M,2AH\n", {0x36U, 0x2AU}),
        makeCase("LXI", "LXI SP,1234H\n", {0x31U, 0x34U, 0x12U}),
        makeCase("LDA", "LDA 2345H\n", {0x3AU, 0x45U, 0x23U}),
        makeCase("STA", "STA 2345H\n", {0x32U, 0x45U, 0x23U}),
        makeCase("LHLD", "LHLD 3456H\n", {0x2AU, 0x56U, 0x34U}),
        makeCase("SHLD", "SHLD 3456H\n", {0x22U, 0x56U, 0x34U}),
        makeCase("LDAX", "LDAX D\n", {0x1AU}),
        makeCase("STAX", "STAX B\n", {0x02U}),
        makeCase("XCHG", "XCHG\n", {0xEBU}),
        makeCase("XTHL", "XTHL\n", {0xE3U}),
        makeCase("SPHL", "SPHL\n", {0xF9U}),
        makeCase("PCHL", "PCHL\n", {0xE9U}),
        makeCase("PUSH", "PUSH PSW\n", {0xF5U}),
        makeCase("POP", "POP H\n", {0xE1U}),
        makeCase("ADD", "ADD M\n", {0x86U}),
        makeCase("ADC", "ADC C\n", {0x89U}),
        makeCase("SUB", "SUB D\n", {0x92U}),
        makeCase("SBB", "SBB E\n", {0x9BU}),
        makeCase("INR", "INR L\n", {0x2CU}),
        makeCase("DCR", "DCR M\n", {0x35U}),
        makeCase("INX", "INX H\n", {0x23U}),
        makeCase("DCX", "DCX SP\n", {0x3BU}),
        makeCase("DAD", "DAD B\n", {0x09U}),
        makeCase("DAA", "DAA\n", {0x27U}),
        makeCase("ADI", "ADI 11H\n", {0xC6U, 0x11U}),
        makeCase("ACI", "ACI 22H\n", {0xCEU, 0x22U}),
        makeCase("SUI", "SUI 33H\n", {0xD6U, 0x33U}),
        makeCase("SBI", "SBI 44H\n", {0xDEU, 0x44U}),
        makeCase("ANA", "ANA B\n", {0xA0U}),
        makeCase("ORA", "ORA M\n", {0xB6U}),
        makeCase("XRA", "XRA L\n", {0xADU}),
        makeCase("CMA", "CMA\n", {0x2FU}),
        makeCase("CMC", "CMC\n", {0x3FU}),
        makeCase("STC", "STC\n", {0x37U}),
        makeCase("ANI", "ANI 55H\n", {0xE6U, 0x55U}),
        makeCase("ORI", "ORI 66H\n", {0xF6U, 0x66U}),
        makeCase("XRI", "XRI 77H\n", {0xEEU, 0x77U}),
        makeCase("CPI", "CPI 88H\n", {0xFEU, 0x88U}),
        makeCase("CMP", "CMP A\n", {0xBFU}),
        makeCase("JMP", "JMP 4567H\n", {0xC3U, 0x67U, 0x45U}),
        makeCase("CALL", "CALL 4567H\n", {0xCDU, 0x67U, 0x45U}),
        makeCase("RET", "RET\n", {0xC9U}),
        makeCase("JC", "JC 1111H\n", {0xDAU, 0x11U, 0x11U}),
        makeCase("JNC", "JNC 1111H\n", {0xD2U, 0x11U, 0x11U}),
        makeCase("JZ", "JZ 1111H\n", {0xCAU, 0x11U, 0x11U}),
        makeCase("JNZ", "JNZ 1111H\n", {0xC2U, 0x11U, 0x11U}),
        makeCase("JP", "JP 1111H\n", {0xF2U, 0x11U, 0x11U}),
        makeCase("JM", "JM 1111H\n", {0xFAU, 0x11U, 0x11U}),
        makeCase("JPE", "JPE 1111H\n", {0xEAU, 0x11U, 0x11U}),
        makeCase("JPO", "JPO 1111H\n", {0xE2U, 0x11U, 0x11U}),
        makeCase("CC", "CC 1111H\n", {0xDCU, 0x11U, 0x11U}),
        makeCase("CNC", "CNC 1111H\n", {0xD4U, 0x11U, 0x11U}),
        makeCase("CZ", "CZ 1111H\n", {0xCCU, 0x11U, 0x11U}),
        makeCase("CNZ", "CNZ 1111H\n", {0xC4U, 0x11U, 0x11U}),
        makeCase("CP", "CP 1111H\n", {0xF4U, 0x11U, 0x11U}),
        makeCase("CM", "CM 1111H\n", {0xFCU, 0x11U, 0x11U}),
        makeCase("CPE", "CPE 1111H\n", {0xECU, 0x11U, 0x11U}),
        makeCase("CPO", "CPO 1111H\n", {0xE4U, 0x11U, 0x11U}),
        makeCase("RC", "RC\n", {0xD8U}),
        makeCase("RNC", "RNC\n", {0xD0U}),
        makeCase("RZ", "RZ\n", {0xC8U}),
        makeCase("RNZ", "RNZ\n", {0xC0U}),
        makeCase("RP", "RP\n", {0xF0U}),
        makeCase("RM", "RM\n", {0xF8U}),
        makeCase("RPE", "RPE\n", {0xE8U}),
        makeCase("RPO", "RPO\n", {0xE0U}),
        makeCase("RST", "RST 7\n", {0xFFU}),
        makeCase("HLT", "HLT\n", {0x76U}),
        makeCase("EI", "EI\n", {0xFBU}),
        makeCase("DI", "DI\n", {0xF3U}),
        makeCase("SIM", "SIM\n", {0x30U}),
        makeCase("RIM", "RIM\n", {0x20U}),
        makeCase("IN", "IN 10H\n", {0xDBU, 0x10U}),
        makeCase("OUT", "OUT 11H\n", {0xD3U, 0x11U}),
        makeCase("RLC", "RLC\n", {0x07U}),
        makeCase("RRC", "RRC\n", {0x0FU}),
        makeCase("RAL", "RAL\n", {0x17U}),
        makeCase("RAR", "RAR\n", {0x1FU}),
    };
}

std::string makeIntelHex(uint16_t origin, const std::vector<uint8_t>& bytes) {
    auto toHexDigit = [](uint8_t nibble) {
        return static_cast<char>(nibble < 10U ? ('0' + nibble) : ('A' + nibble - 10U));
    };
    auto toHexByte = [&](uint8_t value) {
        std::string text(2U, '0');
        text[0U] = toHexDigit(static_cast<uint8_t>((value >> 4U) & 0x0FU));
        text[1U] = toHexDigit(static_cast<uint8_t>(value & 0x0FU));
        return text;
    };

    std::string hex;
    std::size_t offset = 0U;
    while (offset < bytes.size()) {
        const std::size_t chunkSize = std::min<std::size_t>(16U, bytes.size() - offset);
        const uint8_t count = static_cast<uint8_t>(chunkSize);
        const uint16_t address = static_cast<uint16_t>(origin + offset);
        uint8_t checksum = static_cast<uint8_t>(
            count + static_cast<uint8_t>(address >> 8U) +
            static_cast<uint8_t>(address & 0x00FFU));

        hex += ":";
        hex += toHexByte(count);
        hex += toHexByte(static_cast<uint8_t>(address >> 8U));
        hex += toHexByte(static_cast<uint8_t>(address & 0x00FFU));
        hex += "00";

        for (std::size_t index = 0U; index < chunkSize; ++index) {
            checksum = static_cast<uint8_t>(checksum + bytes[offset + index]);
            hex += toHexByte(bytes[offset + index]);
        }

        checksum = static_cast<uint8_t>(~checksum + 1U);
        hex += toHexByte(checksum);
        hex += "\n";
        offset += chunkSize;
    }

    hex += ":00000001FF\n";
    return hex;
}

class AssemblerMnemonicTest : public testing::TestWithParam<AssemblerCase> {};

TEST_P(AssemblerMnemonicTest, AssemblesRepresentativeMnemonic) {
    const AssemblerCase& param = GetParam();
    Assembler assembler;

    const Core85::AssemblyResult result = assembler.assemble(param.source);

    ASSERT_TRUE(result.success) << "Expected assembly success for " << param.name;
    EXPECT_TRUE(result.errors.empty());
    EXPECT_EQ(result.origin, param.origin);
    EXPECT_EQ(result.bytes, param.bytes);
}

INSTANTIATE_TEST_SUITE_P(
    Phase3, AssemblerMnemonicTest,
    testing::ValuesIn(makeMnemonicCases()),
    sanitizeName);

TEST(AssemblerTest, SupportsDirectivesLabelsAndSourceMap) {
    Assembler assembler;
    const std::string source =
        ".ORG 0200H\n"
        "CONST EQU 0AH\n"
        "START: MVI A, CONST\n"
        "JMP DONE\n"
        "TABLE: DB 01H, 02H\n"
        "DONE: DW START\n";

    const Core85::AssemblyResult result = assembler.assemble(source);

    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.origin, 0x0200U);
    EXPECT_EQ(result.bytes, (std::vector<uint8_t>{
        0x3EU, 0x0AU, 0xC3U, 0x07U, 0x02U, 0x01U, 0x02U, 0x00U, 0x02U}));
    EXPECT_EQ(result.symbols.at("CONST"), 0x000AU);
    EXPECT_EQ(result.symbols.at("START"), 0x0200U);
    EXPECT_EQ(result.symbols.at("TABLE"), 0x0205U);
    EXPECT_EQ(result.symbols.at("DONE"), 0x0207U);

    ASSERT_EQ(result.sourceMap.size(), 9U);
    EXPECT_EQ(result.sourceMap.front().address, 0x0200U);
    EXPECT_EQ(result.sourceMap.front().line, 3U);
    EXPECT_EQ(result.sourceMap[2U].address, 0x0202U);
    EXPECT_EQ(result.sourceMap[2U].line, 4U);
    EXPECT_EQ(result.sourceMap.back().address, 0x0208U);
    EXPECT_EQ(result.sourceMap.back().line, 6U);
}

TEST(AssemblerTest, SupportsIntelHexRoundTrip) {
    Assembler assembler;
    const std::string source =
        ".ORG 0200H\n"
        "CONST EQU 0AH\n"
        "START: MVI A, CONST\n"
        "JMP DONE\n"
        "TABLE: DB 01H, 02H\n"
        "DONE: DW START\n";

    const Core85::AssemblyResult assembly = assembler.assemble(source);
    ASSERT_TRUE(assembly.success);

    const std::string expectedHex = makeIntelHex(assembly.origin, assembly.bytes);
    const std::string actualHex = assembler.toIntelHex(assembly);
    EXPECT_EQ(actualHex, expectedHex);

    const Core85::HexLoadResult hexImage = assembler.loadIntelHex(actualHex);
    ASSERT_TRUE(hexImage.success);
    EXPECT_EQ(hexImage.origin, assembly.origin);
    EXPECT_EQ(hexImage.bytes, assembly.bytes);
}

TEST(AssemblerTest, ReportsInvalidSourceClearly) {
    Assembler assembler;

    const Core85::AssemblyResult undefinedSymbol = assembler.assemble(
        ".ORG 1000H\n"
        "JMP MISSING\n");
    ASSERT_FALSE(undefinedSymbol.success);
    EXPECT_FALSE(undefinedSymbol.errors.empty());
    EXPECT_EQ(undefinedSymbol.errors.front().token, "MISSING");

    const Core85::AssemblyResult duplicateLabel = assembler.assemble(
        ".ORG 1000H\n"
        "LOOP: NOP\n"
        "LOOP: HLT\n");
    ASSERT_FALSE(duplicateLabel.success);
    EXPECT_FALSE(duplicateLabel.errors.empty());

    const Core85::AssemblyResult outOfRangeImmediate = assembler.assemble(
        ".ORG 1000H\n"
        "MVI A,1234H\n");
    ASSERT_FALSE(outOfRangeImmediate.success);
    EXPECT_FALSE(outOfRangeImmediate.errors.empty());

    const Core85::AssemblyResult invalidMnemonic = assembler.assemble(
        ".ORG 1000H\n"
        "MUV A,B\n");
    ASSERT_FALSE(invalidMnemonic.success);
    EXPECT_FALSE(invalidMnemonic.errors.empty());
}

TEST(AssemblerTest, LoadsReferenceProgramsFromCorpus) {
    struct ReferenceCase {
        std::string fileName;
        uint16_t origin;
        std::vector<uint8_t> bytes;
    };

    const std::vector<ReferenceCase> references{
        {
            "blink.asm",
            0x1000U,
            {0x3EU, 0x00U, 0xD3U, 0x01U, 0x3CU, 0xFEU, 0x0AU, 0xCAU,
             0x0FU, 0x10U, 0xD3U, 0x00U, 0xC3U, 0x04U, 0x10U, 0x76U},
        },
        {
            "table.asm",
            0x2000U,
            {0x21U, 0x0FU, 0x20U, 0x7EU, 0x23U, 0x46U, 0x22U, 0x0DU,
             0x20U, 0x3AU, 0x0DU, 0x20U, 0x76U, 0x00U, 0x00U, 0x12U, 0x34U},
        },
    };

    const std::filesystem::path corpusDir =
        std::filesystem::path(__FILE__).parent_path() / "asm";
    Assembler assembler;

    for (const ReferenceCase& reference : references) {
        const std::filesystem::path filePath = corpusDir / reference.fileName;
        std::ifstream input(filePath);
        ASSERT_TRUE(input.is_open()) << "Failed to open " << filePath;

        const std::string source{
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
        const Core85::AssemblyResult result = assembler.assemble(source);

        ASSERT_TRUE(result.success) << "Reference program failed: " << reference.fileName;
        EXPECT_EQ(result.origin, reference.origin);
        EXPECT_EQ(result.bytes, reference.bytes);

        const Core85::HexLoadResult roundTrip =
            assembler.loadIntelHex(assembler.toIntelHex(result));
        ASSERT_TRUE(roundTrip.success);
        EXPECT_EQ(roundTrip.origin, reference.origin);
        EXPECT_EQ(roundTrip.bytes, reference.bytes);
    }
}

}  // namespace
