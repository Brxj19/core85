#include "core/assembler.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace Core85 {

namespace {

struct ParsedLine {
    std::size_t lineNumber = 0U;
    std::string label;
    std::string mnemonic;
    std::vector<std::string> operands;
    uint16_t address = 0U;
    bool stopAssembly = false;
};

std::string trimCopy(std::string_view value) {
    std::size_t start = 0U;
    while (start < value.size() &&
           std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }

    std::size_t end = value.size();
    while (end > start &&
           std::isspace(static_cast<unsigned char>(value[end - 1U])) != 0) {
        --end;
    }

    return std::string(value.substr(start, end - start));
}

std::string toUpperCopy(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());

    for (const char ch : value) {
        normalized.push_back(static_cast<char>(
            std::toupper(static_cast<unsigned char>(ch))));
    }

    return normalized;
}

std::string normalizeKeyword(std::string_view value) {
    std::string normalized = toUpperCopy(trimCopy(value));

    if (!normalized.empty() && normalized.front() == '.') {
        normalized.erase(normalized.begin());
    }

    return normalized;
}

bool isValidSymbolName(std::string_view value) {
    if (value.empty()) {
        return false;
    }

    const auto isInitial = [](const char ch) {
        return std::isalpha(static_cast<unsigned char>(ch)) != 0 || ch == '_';
    };
    const auto isBody = [](const char ch) {
        return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_';
    };

    if (!isInitial(value.front())) {
        return false;
    }

    for (std::size_t index = 1U; index < value.size(); ++index) {
        if (!isBody(value[index])) {
            return false;
        }
    }

    return true;
}

void addError(std::vector<AssemblerError>& errors,
              const std::size_t lineNumber,
              std::string token,
              std::string message) {
    errors.push_back(AssemblerError{
        lineNumber,
        std::move(token),
        std::move(message)});
}

std::pair<std::string, std::string_view> splitFirstToken(std::string_view text) {
    while (!text.empty() &&
           std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1U);
    }

    std::size_t length = 0U;
    while (length < text.size() &&
           std::isspace(static_cast<unsigned char>(text[length])) == 0) {
        ++length;
    }

    const std::string token(text.substr(0U, length));
    text.remove_prefix(std::min(length, text.size()));

    return {token, text};
}

std::vector<std::string> splitOperands(std::string_view operandText) {
    std::vector<std::string> operands;
    std::size_t start = 0U;

    while (start <= operandText.size()) {
        std::size_t end = operandText.find(',', start);
        if (end == std::string_view::npos) {
            end = operandText.size();
        }

        std::string operand = trimCopy(operandText.substr(start, end - start));
        if (!operand.empty()) {
            operands.push_back(std::move(operand));
        }

        if (end == operandText.size()) {
            break;
        }

        start = end + 1U;
    }

    return operands;
}

const std::array<const char*, 88>& reservedKeywords() {
    static const std::array<const char*, 88> keywords{
        "MOV", "MVI", "LXI", "LDA", "STA", "LHLD", "SHLD", "LDAX", "STAX",
        "XCHG", "XTHL", "SPHL", "PCHL", "PUSH", "POP",
        "ADD", "ADC", "SUB", "SBB", "INR", "DCR", "INX", "DCX", "DAD", "DAA",
        "ADI", "ACI", "SUI", "SBI",
        "ANA", "ORA", "XRA", "CMA", "CMC", "STC", "ANI", "ORI", "XRI", "CPI", "CMP",
        "JMP", "CALL", "RET", "JC", "JNC", "JZ", "JNZ", "JP", "JM", "JPE", "JPO",
        "CC", "CNC", "CZ", "CNZ", "CP", "CM", "CPE", "CPO",
        "RC", "RNC", "RZ", "RNZ", "RP", "RM", "RPE", "RPO",
        "RST", "NOP", "HLT", "EI", "DI", "SIM", "RIM", "IN", "OUT",
        "RLC", "RRC", "RAL", "RAR",
        "ORG", "EQU", "DB", "DW", "END",
        "PSW", "SP", "M"};
    return keywords;
}

bool isReservedKeyword(std::string_view value) {
    const std::string normalized = normalizeKeyword(value);

    for (const char* const keyword : reservedKeywords()) {
        if (normalized == keyword) {
            return true;
        }
    }

    return false;
}

std::optional<ParsedLine> parseLine(std::size_t lineNumber,
                                    std::string_view sourceLine,
                                    std::vector<AssemblerError>& errors) {
    const std::size_t commentIndex = sourceLine.find(';');
    if (commentIndex != std::string_view::npos) {
        sourceLine = sourceLine.substr(0U, commentIndex);
    }

    std::string trimmed = trimCopy(sourceLine);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    ParsedLine parsed;
    parsed.lineNumber = lineNumber;
    std::string_view remainder = trimmed;

    const std::size_t colonIndex = remainder.find(':');
    if (colonIndex != std::string_view::npos) {
        parsed.label = trimCopy(remainder.substr(0U, colonIndex));
        remainder = remainder.substr(colonIndex + 1U);

        if (!isValidSymbolName(parsed.label)) {
            addError(errors, lineNumber, parsed.label, "Invalid label name.");
            return std::nullopt;
        }
    }

    const auto [firstToken, restAfterFirst] = splitFirstToken(remainder);
    if (firstToken.empty()) {
        return parsed;
    }

    const auto [secondToken, restAfterSecond] = splitFirstToken(restAfterFirst);
    const bool secondIsReserved = !secondToken.empty() && isReservedKeyword(secondToken);

    if (parsed.label.empty() && !secondToken.empty() &&
        !isReservedKeyword(firstToken) && secondIsReserved) {
        parsed.label = firstToken;
        parsed.mnemonic = normalizeKeyword(secondToken);
        parsed.operands = splitOperands(restAfterSecond);

        if (!isValidSymbolName(parsed.label)) {
            addError(errors, lineNumber, parsed.label, "Invalid label name.");
            return std::nullopt;
        }
    } else {
        parsed.mnemonic = normalizeKeyword(firstToken);
        parsed.operands = splitOperands(restAfterFirst);
    }

    parsed.stopAssembly = parsed.mnemonic == "END";
    return parsed;
}

std::optional<uint8_t> parseRegister(std::string_view operand) {
    const std::string normalized = normalizeKeyword(operand);

    if (normalized == "B") {
        return 0x00U;
    }

    if (normalized == "C") {
        return 0x01U;
    }

    if (normalized == "D") {
        return 0x02U;
    }

    if (normalized == "E") {
        return 0x03U;
    }

    if (normalized == "H") {
        return 0x04U;
    }

    if (normalized == "L") {
        return 0x05U;
    }

    if (normalized == "M") {
        return 0x06U;
    }

    if (normalized == "A") {
        return 0x07U;
    }

    return std::nullopt;
}

std::optional<uint8_t> parseRegisterPair(std::string_view operand) {
    const std::string normalized = normalizeKeyword(operand);

    if (normalized == "B") {
        return 0x00U;
    }

    if (normalized == "D") {
        return 0x01U;
    }

    if (normalized == "H") {
        return 0x02U;
    }

    if (normalized == "SP") {
        return 0x03U;
    }

    return std::nullopt;
}

std::optional<uint8_t> parseStaxPair(std::string_view operand) {
    const std::string normalized = normalizeKeyword(operand);

    if (normalized == "B") {
        return 0x00U;
    }

    if (normalized == "D") {
        return 0x01U;
    }

    return std::nullopt;
}

std::optional<uint8_t> parseStackPair(std::string_view operand) {
    const std::string normalized = normalizeKeyword(operand);

    if (normalized == "B") {
        return 0x00U;
    }

    if (normalized == "D") {
        return 0x01U;
    }

    if (normalized == "H") {
        return 0x02U;
    }

    if (normalized == "PSW") {
        return 0x03U;
    }

    return std::nullopt;
}

std::optional<uint16_t> parseNumericLiteral(std::string_view token) {
    const std::string trimmed = trimCopy(token);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    if (trimmed.size() == 3U && trimmed.front() == '\'' && trimmed.back() == '\'') {
        return static_cast<uint16_t>(static_cast<unsigned char>(trimmed[1U]));
    }

    unsigned int base = 10U;
    std::string digits = trimmed;

    if (digits.size() > 2U && digits[0U] == '0' &&
        (digits[1U] == 'x' || digits[1U] == 'X')) {
        base = 16U;
        digits = digits.substr(2U);
    } else if (!digits.empty() &&
               (digits.back() == 'H' || digits.back() == 'h')) {
        base = 16U;
        digits.pop_back();
    } else if (!digits.empty() &&
               (digits.back() == 'D' || digits.back() == 'd')) {
        base = 10U;
        digits.pop_back();
    }

    if (digits.empty()) {
        return std::nullopt;
    }

    for (const char ch : digits) {
        if ((base == 16U &&
             std::isxdigit(static_cast<unsigned char>(ch)) == 0) ||
            (base == 10U &&
             std::isdigit(static_cast<unsigned char>(ch)) == 0)) {
            return std::nullopt;
        }
    }

    unsigned long value = 0UL;
    try {
        value = std::stoul(digits, nullptr, static_cast<int>(base));
    } catch (...) {
        return std::nullopt;
    }

    if (value > std::numeric_limits<uint16_t>::max()) {
        return std::nullopt;
    }

    return static_cast<uint16_t>(value);
}

std::optional<uint16_t> evaluateExpression(
    std::string_view expression,
    const std::unordered_map<std::string, uint16_t>& symbols,
    const bool allowUnresolved,
    const std::size_t lineNumber,
    std::vector<AssemblerError>& errors) {
    const std::string trimmed = trimCopy(expression);
    if (trimmed.empty()) {
        addError(errors, lineNumber, "", "Missing expression.");
        return std::nullopt;
    }

    std::int32_t total = 0;
    int sign = 1;
    bool expectTerm = true;
    std::size_t position = 0U;

    while (position < trimmed.size()) {
        while (position < trimmed.size() &&
               std::isspace(static_cast<unsigned char>(trimmed[position])) != 0) {
            ++position;
        }

        if (position >= trimmed.size()) {
            break;
        }

        const char marker = trimmed[position];
        if (marker == '+' || marker == '-') {
            if (expectTerm) {
                sign = marker == '+' ? 1 : -1;
                ++position;
                continue;
            }

            expectTerm = true;
            sign = marker == '+' ? 1 : -1;
            ++position;
            continue;
        }

        std::size_t end = position;
        while (end < trimmed.size() && trimmed[end] != '+' && trimmed[end] != '-') {
            ++end;
        }

        const std::string term = trimCopy(
            std::string_view(trimmed).substr(position, end - position));
        if (term.empty()) {
            addError(errors, lineNumber, trimmed, "Invalid expression syntax.");
            return std::nullopt;
        }

        std::optional<uint16_t> value = parseNumericLiteral(term);
        if (!value.has_value()) {
            const std::string symbolKey = toUpperCopy(term);
            const auto found = symbols.find(symbolKey);
            if (found != symbols.end()) {
                value = found->second;
            } else if (allowUnresolved) {
                value = 0U;
            } else {
                addError(errors, lineNumber, term, "Undefined symbol.");
                return std::nullopt;
            }
        }

        total += static_cast<std::int32_t>(sign) * static_cast<std::int32_t>(*value);
        expectTerm = false;
        sign = 1;
        position = end;
    }

    if (expectTerm) {
        addError(errors, lineNumber, trimmed, "Invalid expression syntax.");
        return std::nullopt;
    }

    if (total < 0 || total > static_cast<std::int32_t>(std::numeric_limits<uint16_t>::max())) {
        addError(errors, lineNumber, trimmed, "Expression value out of range.");
        return std::nullopt;
    }

    return static_cast<uint16_t>(total);
}

void appendWord(std::vector<uint8_t>& bytes, uint16_t value) {
    bytes.push_back(static_cast<uint8_t>(value & 0x00FFU));
    bytes.push_back(static_cast<uint8_t>(value >> 8U));
}

std::optional<uint8_t> requireRegister(
    const ParsedLine& line,
    const std::string& operand,
    std::vector<AssemblerError>& errors) {
    const std::optional<uint8_t> code = parseRegister(operand);
    if (!code.has_value()) {
        addError(errors, line.lineNumber, operand, "Expected register operand.");
    }
    return code;
}

std::optional<uint8_t> requireRegisterPair(
    const ParsedLine& line,
    const std::string& operand,
    std::vector<AssemblerError>& errors) {
    const std::optional<uint8_t> code = parseRegisterPair(operand);
    if (!code.has_value()) {
        addError(errors, line.lineNumber, operand, "Expected register-pair operand.");
    }
    return code;
}

std::optional<uint8_t> requireStaxPair(
    const ParsedLine& line,
    const std::string& operand,
    std::vector<AssemblerError>& errors) {
    const std::optional<uint8_t> code = parseStaxPair(operand);
    if (!code.has_value()) {
        addError(errors, line.lineNumber, operand, "Expected B or D register pair.");
    }
    return code;
}

std::optional<uint8_t> requireStackPair(
    const ParsedLine& line,
    const std::string& operand,
    std::vector<AssemblerError>& errors) {
    const std::optional<uint8_t> code = parseStackPair(operand);
    if (!code.has_value()) {
        addError(errors, line.lineNumber, operand, "Expected B, D, H, or PSW.");
    }
    return code;
}

std::optional<uint16_t> requireExpression(
    const ParsedLine& line,
    const std::string& operand,
    const std::unordered_map<std::string, uint16_t>& symbols,
    const bool allowUnresolved,
    std::vector<AssemblerError>& errors) {
    return evaluateExpression(
        operand,
        symbols,
        allowUnresolved,
        line.lineNumber,
        errors);
}

std::vector<uint8_t> encodeStatement(
    const ParsedLine& line,
    const std::unordered_map<std::string, uint16_t>& symbols,
    const bool allowUnresolved,
    std::vector<AssemblerError>& errors) {
    std::vector<uint8_t> bytes;

    if (line.mnemonic.empty() || line.mnemonic == "ORG" ||
        line.mnemonic == "EQU" || line.mnemonic == "END") {
        return bytes;
    }

    if (line.mnemonic == "DB") {
        if (line.operands.empty()) {
            addError(errors, line.lineNumber, "DB", "DB requires at least one operand.");
            return bytes;
        }

        for (const std::string& operand : line.operands) {
            const std::optional<uint16_t> value =
                requireExpression(line, operand, symbols, allowUnresolved, errors);
            if (!value.has_value()) {
                return {};
            }

            if (*value > 0x00FFU) {
                addError(errors, line.lineNumber, operand, "Byte value out of range.");
                return {};
            }

            bytes.push_back(static_cast<uint8_t>(*value));
        }

        return bytes;
    }

    if (line.mnemonic == "DW") {
        if (line.operands.empty()) {
            addError(errors, line.lineNumber, "DW", "DW requires at least one operand.");
            return bytes;
        }

        for (const std::string& operand : line.operands) {
            const std::optional<uint16_t> value =
                requireExpression(line, operand, symbols, allowUnresolved, errors);
            if (!value.has_value()) {
                return {};
            }

            appendWord(bytes, *value);
        }

        return bytes;
    }

    const auto requireOperandCount = [&](std::size_t expected) -> bool {
        if (line.operands.size() != expected) {
            addError(errors,
                     line.lineNumber,
                     line.mnemonic,
                     "Unexpected operand count.");
            return false;
        }

        return true;
    };

    const auto encodeSingleByte = [&](uint8_t opcode) {
        bytes.push_back(opcode);
    };

    if (line.mnemonic == "NOP") {
        if (!requireOperandCount(0U)) {
            return {};
        }
        encodeSingleByte(0x00U);
        return bytes;
    }

    if (line.mnemonic == "RIM") {
        if (!requireOperandCount(0U)) {
            return {};
        }
        encodeSingleByte(0x20U);
        return bytes;
    }

    if (line.mnemonic == "SIM") {
        if (!requireOperandCount(0U)) {
            return {};
        }
        encodeSingleByte(0x30U);
        return bytes;
    }

    if (line.mnemonic == "HLT") {
        if (!requireOperandCount(0U)) {
            return {};
        }
        encodeSingleByte(0x76U);
        return bytes;
    }

    if (line.mnemonic == "EI") {
        if (!requireOperandCount(0U)) {
            return {};
        }
        encodeSingleByte(0xFBU);
        return bytes;
    }

    if (line.mnemonic == "DI") {
        if (!requireOperandCount(0U)) {
            return {};
        }
        encodeSingleByte(0xF3U);
        return bytes;
    }

    if (line.mnemonic == "DAA") {
        if (!requireOperandCount(0U)) {
            return {};
        }
        encodeSingleByte(0x27U);
        return bytes;
    }

    if (line.mnemonic == "CMA") {
        if (!requireOperandCount(0U)) {
            return {};
        }
        encodeSingleByte(0x2FU);
        return bytes;
    }

    if (line.mnemonic == "CMC") {
        if (!requireOperandCount(0U)) {
            return {};
        }
        encodeSingleByte(0x3FU);
        return bytes;
    }

    if (line.mnemonic == "STC") {
        if (!requireOperandCount(0U)) {
            return {};
        }
        encodeSingleByte(0x37U);
        return bytes;
    }

    if (line.mnemonic == "RLC") {
        if (!requireOperandCount(0U)) {
            return {};
        }
        encodeSingleByte(0x07U);
        return bytes;
    }

    if (line.mnemonic == "RRC") {
        if (!requireOperandCount(0U)) {
            return {};
        }
        encodeSingleByte(0x0FU);
        return bytes;
    }

    if (line.mnemonic == "RAL") {
        if (!requireOperandCount(0U)) {
            return {};
        }
        encodeSingleByte(0x17U);
        return bytes;
    }

    if (line.mnemonic == "RAR") {
        if (!requireOperandCount(0U)) {
            return {};
        }
        encodeSingleByte(0x1FU);
        return bytes;
    }

    if (line.mnemonic == "XTHL") {
        if (!requireOperandCount(0U)) {
            return {};
        }
        encodeSingleByte(0xE3U);
        return bytes;
    }

    if (line.mnemonic == "XCHG") {
        if (!requireOperandCount(0U)) {
            return {};
        }
        encodeSingleByte(0xEBU);
        return bytes;
    }

    if (line.mnemonic == "SPHL") {
        if (!requireOperandCount(0U)) {
            return {};
        }
        encodeSingleByte(0xF9U);
        return bytes;
    }

    if (line.mnemonic == "PCHL") {
        if (!requireOperandCount(0U)) {
            return {};
        }
        encodeSingleByte(0xE9U);
        return bytes;
    }

    if (line.mnemonic == "MOV") {
        if (!requireOperandCount(2U)) {
            return {};
        }

        const std::optional<uint8_t> destination =
            requireRegister(line, line.operands[0U], errors);
        const std::optional<uint8_t> source =
            requireRegister(line, line.operands[1U], errors);
        if (!destination.has_value() || !source.has_value()) {
            return {};
        }

        if (*destination == 0x06U && *source == 0x06U) {
            addError(errors, line.lineNumber, "MOV M,M", "MOV M,M is not a valid 8085 instruction.");
            return {};
        }

        encodeSingleByte(static_cast<uint8_t>(0x40U | (*destination << 3U) | *source));
        return bytes;
    }

    if (line.mnemonic == "MVI") {
        if (!requireOperandCount(2U)) {
            return {};
        }

        const std::optional<uint8_t> destination =
            requireRegister(line, line.operands[0U], errors);
        const std::optional<uint16_t> immediate =
            requireExpression(line, line.operands[1U], symbols, allowUnresolved, errors);
        if (!destination.has_value() || !immediate.has_value()) {
            return {};
        }

        if (*immediate > 0x00FFU) {
            addError(errors, line.lineNumber, line.operands[1U], "Immediate byte out of range.");
            return {};
        }

        bytes.push_back(static_cast<uint8_t>(0x06U | (*destination << 3U)));
        bytes.push_back(static_cast<uint8_t>(*immediate));
        return bytes;
    }

    if (line.mnemonic == "LXI") {
        if (!requireOperandCount(2U)) {
            return {};
        }

        const std::optional<uint8_t> pair =
            requireRegisterPair(line, line.operands[0U], errors);
        const std::optional<uint16_t> immediate =
            requireExpression(line, line.operands[1U], symbols, allowUnresolved, errors);
        if (!pair.has_value() || !immediate.has_value()) {
            return {};
        }

        bytes.push_back(static_cast<uint8_t>(0x01U | (*pair << 4U)));
        appendWord(bytes, *immediate);
        return bytes;
    }

    if (line.mnemonic == "LDAX" || line.mnemonic == "STAX") {
        if (!requireOperandCount(1U)) {
            return {};
        }

        const std::optional<uint8_t> pair =
            requireStaxPair(line, line.operands[0U], errors);
        if (!pair.has_value()) {
            return {};
        }

        const uint8_t base = line.mnemonic == "STAX" ? 0x02U : 0x0AU;
        bytes.push_back(static_cast<uint8_t>(base | (*pair << 4U)));
        return bytes;
    }

    if (line.mnemonic == "INX" || line.mnemonic == "DCX" || line.mnemonic == "DAD") {
        if (!requireOperandCount(1U)) {
            return {};
        }

        const std::optional<uint8_t> pair =
            requireRegisterPair(line, line.operands[0U], errors);
        if (!pair.has_value()) {
            return {};
        }

        uint8_t base = 0x00U;
        if (line.mnemonic == "INX") {
            base = 0x03U;
        } else if (line.mnemonic == "DCX") {
            base = 0x0BU;
        } else {
            base = 0x09U;
        }

        bytes.push_back(static_cast<uint8_t>(base | (*pair << 4U)));
        return bytes;
    }

    if (line.mnemonic == "INR" || line.mnemonic == "DCR") {
        if (!requireOperandCount(1U)) {
            return {};
        }

        const std::optional<uint8_t> target =
            requireRegister(line, line.operands[0U], errors);
        if (!target.has_value()) {
            return {};
        }

        const uint8_t base = line.mnemonic == "INR" ? 0x04U : 0x05U;
        bytes.push_back(static_cast<uint8_t>(base | (*target << 3U)));
        return bytes;
    }

    if (line.mnemonic == "ADD" || line.mnemonic == "ADC" || line.mnemonic == "SUB" ||
        line.mnemonic == "SBB" || line.mnemonic == "ANA" || line.mnemonic == "XRA" ||
        line.mnemonic == "ORA" || line.mnemonic == "CMP") {
        if (!requireOperandCount(1U)) {
            return {};
        }

        const std::optional<uint8_t> source =
            requireRegister(line, line.operands[0U], errors);
        if (!source.has_value()) {
            return {};
        }

        uint8_t base = 0x80U;
        if (line.mnemonic == "ADD") {
            base = 0x80U;
        } else if (line.mnemonic == "ADC") {
            base = 0x88U;
        } else if (line.mnemonic == "SUB") {
            base = 0x90U;
        } else if (line.mnemonic == "SBB") {
            base = 0x98U;
        } else if (line.mnemonic == "ANA") {
            base = 0xA0U;
        } else if (line.mnemonic == "XRA") {
            base = 0xA8U;
        } else if (line.mnemonic == "ORA") {
            base = 0xB0U;
        } else {
            base = 0xB8U;
        }

        bytes.push_back(static_cast<uint8_t>(base | *source));
        return bytes;
    }

    if (line.mnemonic == "ADI" || line.mnemonic == "ACI" || line.mnemonic == "SUI" ||
        line.mnemonic == "SBI" || line.mnemonic == "ANI" || line.mnemonic == "XRI" ||
        line.mnemonic == "ORI" || line.mnemonic == "CPI") {
        if (!requireOperandCount(1U)) {
            return {};
        }

        const std::optional<uint16_t> immediate =
            requireExpression(line, line.operands[0U], symbols, allowUnresolved, errors);
        if (!immediate.has_value()) {
            return {};
        }

        if (*immediate > 0x00FFU) {
            addError(errors, line.lineNumber, line.operands[0U], "Immediate byte out of range.");
            return {};
        }

        uint8_t opcode = 0xC6U;
        if (line.mnemonic == "ADI") {
            opcode = 0xC6U;
        } else if (line.mnemonic == "ACI") {
            opcode = 0xCEU;
        } else if (line.mnemonic == "SUI") {
            opcode = 0xD6U;
        } else if (line.mnemonic == "SBI") {
            opcode = 0xDEU;
        } else if (line.mnemonic == "ANI") {
            opcode = 0xE6U;
        } else if (line.mnemonic == "XRI") {
            opcode = 0xEEU;
        } else if (line.mnemonic == "ORI") {
            opcode = 0xF6U;
        } else {
            opcode = 0xFEU;
        }

        bytes.push_back(opcode);
        bytes.push_back(static_cast<uint8_t>(*immediate));
        return bytes;
    }

    if (line.mnemonic == "LDA" || line.mnemonic == "STA" ||
        line.mnemonic == "LHLD" || line.mnemonic == "SHLD" ||
        line.mnemonic == "JMP" || line.mnemonic == "CALL") {
        if (!requireOperandCount(1U)) {
            return {};
        }

        const std::optional<uint16_t> address =
            requireExpression(line, line.operands[0U], symbols, allowUnresolved, errors);
        if (!address.has_value()) {
            return {};
        }

        uint8_t opcode = 0x3AU;
        if (line.mnemonic == "LDA") {
            opcode = 0x3AU;
        } else if (line.mnemonic == "STA") {
            opcode = 0x32U;
        } else if (line.mnemonic == "LHLD") {
            opcode = 0x2AU;
        } else if (line.mnemonic == "SHLD") {
            opcode = 0x22U;
        } else if (line.mnemonic == "JMP") {
            opcode = 0xC3U;
        } else {
            opcode = 0xCDU;
        }

        bytes.push_back(opcode);
        appendWord(bytes, *address);
        return bytes;
    }

    if (line.mnemonic == "PUSH" || line.mnemonic == "POP") {
        if (!requireOperandCount(1U)) {
            return {};
        }

        const std::optional<uint8_t> pair =
            requireStackPair(line, line.operands[0U], errors);
        if (!pair.has_value()) {
            return {};
        }

        const uint8_t base = line.mnemonic == "PUSH" ? 0xC5U : 0xC1U;
        bytes.push_back(static_cast<uint8_t>(base | (*pair << 4U)));
        return bytes;
    }

    if (line.mnemonic == "RET") {
        if (!requireOperandCount(0U)) {
            return {};
        }
        bytes.push_back(0xC9U);
        return bytes;
    }

    if (line.mnemonic == "IN" || line.mnemonic == "OUT") {
        if (!requireOperandCount(1U)) {
            return {};
        }

        const std::optional<uint16_t> port =
            requireExpression(line, line.operands[0U], symbols, allowUnresolved, errors);
        if (!port.has_value()) {
            return {};
        }

        if (*port > 0x00FFU) {
            addError(errors, line.lineNumber, line.operands[0U], "Port value out of range.");
            return {};
        }

        bytes.push_back(line.mnemonic == "IN" ? 0xDBU : 0xD3U);
        bytes.push_back(static_cast<uint8_t>(*port));
        return bytes;
    }

    if (line.mnemonic == "RST") {
        if (!requireOperandCount(1U)) {
            return {};
        }

        const std::optional<uint16_t> vector =
            requireExpression(line, line.operands[0U], symbols, allowUnresolved, errors);
        if (!vector.has_value()) {
            return {};
        }

        if (*vector > 7U) {
            addError(errors, line.lineNumber, line.operands[0U], "RST vector must be in the range 0..7.");
            return {};
        }

        bytes.push_back(static_cast<uint8_t>(0xC7U | (static_cast<uint8_t>(*vector) << 3U)));
        return bytes;
    }

    const std::array<std::pair<std::string_view, uint8_t>, 8> conditionalJumps{{
        {"JNZ", 0xC2U}, {"JZ", 0xCAU}, {"JNC", 0xD2U}, {"JC", 0xDAU},
        {"JPO", 0xE2U}, {"JPE", 0xEAU}, {"JP", 0xF2U}, {"JM", 0xFAU},
    }};
    for (const auto& [name, opcode] : conditionalJumps) {
        if (line.mnemonic == name) {
            if (!requireOperandCount(1U)) {
                return {};
            }

            const std::optional<uint16_t> address =
                requireExpression(line, line.operands[0U], symbols, allowUnresolved, errors);
            if (!address.has_value()) {
                return {};
            }

            bytes.push_back(opcode);
            appendWord(bytes, *address);
            return bytes;
        }
    }

    const std::array<std::pair<std::string_view, uint8_t>, 8> conditionalCalls{{
        {"CNZ", 0xC4U}, {"CZ", 0xCCU}, {"CNC", 0xD4U}, {"CC", 0xDCU},
        {"CPO", 0xE4U}, {"CPE", 0xECU}, {"CP", 0xF4U}, {"CM", 0xFCU},
    }};
    for (const auto& [name, opcode] : conditionalCalls) {
        if (line.mnemonic == name) {
            if (!requireOperandCount(1U)) {
                return {};
            }

            const std::optional<uint16_t> address =
                requireExpression(line, line.operands[0U], symbols, allowUnresolved, errors);
            if (!address.has_value()) {
                return {};
            }

            bytes.push_back(opcode);
            appendWord(bytes, *address);
            return bytes;
        }
    }

    const std::array<std::pair<std::string_view, uint8_t>, 8> conditionalReturns{{
        {"RNZ", 0xC0U}, {"RZ", 0xC8U}, {"RNC", 0xD0U}, {"RC", 0xD8U},
        {"RPO", 0xE0U}, {"RPE", 0xE8U}, {"RP", 0xF0U}, {"RM", 0xF8U},
    }};
    for (const auto& [name, opcode] : conditionalReturns) {
        if (line.mnemonic == name) {
            if (!requireOperandCount(0U)) {
                return {};
            }

            bytes.push_back(opcode);
            return bytes;
        }
    }

    addError(errors, line.lineNumber, line.mnemonic, "Unknown mnemonic or directive.");
    return {};
}

char toHexDigit(uint8_t value) {
    return static_cast<char>(value < 10U ? ('0' + value) : ('A' + (value - 10U)));
}

std::string toHexByte(uint8_t value) {
    std::string text(2U, '0');
    text[0U] = toHexDigit(static_cast<uint8_t>((value >> 4U) & 0x0FU));
    text[1U] = toHexDigit(static_cast<uint8_t>(value & 0x0FU));
    return text;
}

std::string makeHexRecord(uint8_t recordLength,
                          uint16_t address,
                          uint8_t recordType,
                          std::string_view data) {
    uint8_t checksum = recordLength;
    checksum = static_cast<uint8_t>(checksum + static_cast<uint8_t>(address >> 8U));
    checksum = static_cast<uint8_t>(checksum + static_cast<uint8_t>(address & 0x00FFU));
    checksum = static_cast<uint8_t>(checksum + recordType);

    std::string record = ":";
    record += toHexByte(recordLength);
    record += toHexByte(static_cast<uint8_t>(address >> 8U));
    record += toHexByte(static_cast<uint8_t>(address & 0x00FFU));
    record += toHexByte(recordType);

    for (std::size_t index = 0U; index < data.size(); index += 2U) {
        const std::string_view pair = data.substr(index, 2U);
        const uint8_t byte = static_cast<uint8_t>(
            (std::stoi(std::string(pair), nullptr, 16)) & 0xFF);
        checksum = static_cast<uint8_t>(checksum + byte);
        record += std::string(pair);
    }

    checksum = static_cast<uint8_t>(~checksum + 1U);
    record += toHexByte(checksum);
    return record;
}

std::optional<uint8_t> parseHexByte(std::string_view text) {
    if (text.size() != 2U) {
        return std::nullopt;
    }

    uint8_t value = 0U;
    for (const char ch : text) {
        value = static_cast<uint8_t>(value << 4U);

        if (ch >= '0' && ch <= '9') {
            value = static_cast<uint8_t>(value + static_cast<uint8_t>(ch - '0'));
        } else if (ch >= 'A' && ch <= 'F') {
            value = static_cast<uint8_t>(value + static_cast<uint8_t>(10 + ch - 'A'));
        } else if (ch >= 'a' && ch <= 'f') {
            value = static_cast<uint8_t>(value + static_cast<uint8_t>(10 + ch - 'a'));
        } else {
            return std::nullopt;
        }
    }

    return value;
}

}  // namespace

AssemblyResult Assembler::assemble(std::string_view source) const {
    AssemblyResult result;
    std::vector<ParsedLine> parsedLines;
    std::unordered_map<std::string, uint16_t> symbolTable;

    uint16_t currentAddress = 0U;
    bool allowBackwardsOrg = true;
    bool stopAssembly = false;

    std::size_t lineNumber = 1U;
    std::size_t lineStart = 0U;
    while (lineStart <= source.size()) {
        std::size_t lineEnd = source.find('\n', lineStart);
        if (lineEnd == std::string_view::npos) {
            lineEnd = source.size();
        }

        std::string_view line = source.substr(lineStart, lineEnd - lineStart);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1U);
        }

        const std::size_t errorCountBeforeLine = result.errors.size();
        const std::optional<ParsedLine> maybeParsed = parseLine(lineNumber, line, result.errors);
        if (maybeParsed.has_value()) {
            ParsedLine parsed = *maybeParsed;
            parsed.address = currentAddress;

            const std::string symbolKey = toUpperCopy(parsed.label);

            if (!parsed.label.empty() && parsed.mnemonic != "EQU") {
                if (symbolTable.find(symbolKey) != symbolTable.end()) {
                    addError(result.errors, lineNumber, parsed.label, "Duplicate label definition.");
                } else {
                    symbolTable.emplace(symbolKey, currentAddress);
                }
            }

            if (parsed.mnemonic == "EQU") {
                if (parsed.label.empty()) {
                    addError(result.errors, lineNumber, "EQU", "EQU requires a symbol name.");
                } else if (parsed.operands.size() != 1U) {
                    addError(result.errors, lineNumber, "EQU", "EQU requires exactly one expression.");
                } else if (symbolTable.find(symbolKey) != symbolTable.end()) {
                    addError(result.errors, lineNumber, parsed.label, "Duplicate label definition.");
                } else {
                    const std::optional<uint16_t> value = evaluateExpression(
                        parsed.operands.front(),
                        symbolTable,
                        false,
                        lineNumber,
                        result.errors);
                    if (value.has_value()) {
                        symbolTable.emplace(symbolKey, *value);
                    }
                }
            } else if (parsed.mnemonic == "ORG") {
                if (parsed.operands.size() != 1U) {
                    addError(result.errors, lineNumber, "ORG", "ORG requires exactly one expression.");
                } else {
                    const std::optional<uint16_t> address = evaluateExpression(
                        parsed.operands.front(),
                        symbolTable,
                        false,
                        lineNumber,
                        result.errors);
                    if (address.has_value()) {
                        if (!allowBackwardsOrg && *address < currentAddress) {
                            addError(result.errors, lineNumber, "ORG", "ORG cannot move backwards after code emission.");
                        } else {
                            currentAddress = *address;
                            parsed.address = *address;
                        }
                    }
                }
            } else if (!parsed.mnemonic.empty() && parsed.mnemonic != "END") {
                const std::vector<uint8_t> encoded =
                    encodeStatement(parsed, symbolTable, true, result.errors);
                if (result.errors.size() == errorCountBeforeLine) {
                    currentAddress = static_cast<uint16_t>(currentAddress + encoded.size());
                    if (!encoded.empty()) {
                        allowBackwardsOrg = false;
                    }
                }
            }

            parsed.stopAssembly = parsed.mnemonic == "END";
            parsedLines.push_back(std::move(parsed));
            if (!parsedLines.empty() && parsedLines.back().stopAssembly) {
                stopAssembly = true;
            }
        }

        if (stopAssembly || lineEnd == source.size()) {
            break;
        }

        lineStart = lineEnd + 1U;
        ++lineNumber;
    }

    if (!result.errors.empty()) {
        result.symbols = symbolTable;
        return result;
    }

    for (const ParsedLine& parsed : parsedLines) {
        if (parsed.stopAssembly) {
            break;
        }

        if (parsed.mnemonic.empty() || parsed.mnemonic == "ORG" || parsed.mnemonic == "EQU") {
            continue;
        }

        const std::vector<uint8_t> encoded =
            encodeStatement(parsed, symbolTable, false, result.errors);
        if (!result.errors.empty()) {
            result.symbols = symbolTable;
            return result;
        }

        if (encoded.empty()) {
            continue;
        }

        if (result.bytes.empty()) {
            result.origin = parsed.address;
        }

        const uint16_t currentEndAddress =
            static_cast<uint16_t>(result.origin + result.bytes.size());
        if (parsed.address > currentEndAddress) {
            result.bytes.resize(static_cast<std::size_t>(parsed.address - result.origin), 0xFFU);
        }

        for (std::size_t index = 0U; index < encoded.size(); ++index) {
            const uint16_t byteAddress = static_cast<uint16_t>(parsed.address + index);
            const std::size_t offset = static_cast<std::size_t>(byteAddress - result.origin);

            if (offset < result.bytes.size()) {
                result.bytes[offset] = encoded[index];
            } else {
                result.bytes.push_back(encoded[index]);
            }

            result.sourceMap.push_back(SourceMappingEntry{
                byteAddress,
                parsed.lineNumber});
        }
    }

    result.success = result.errors.empty();
    result.symbols = std::move(symbolTable);
    return result;
}

std::string Assembler::toIntelHex(const AssemblyResult& result) const {
    if (!result.success) {
        return {};
    }

    std::ostringstream output;
    std::size_t offset = 0U;

    while (offset < result.bytes.size()) {
        const std::size_t chunkSize = std::min<std::size_t>(16U, result.bytes.size() - offset);
        std::string data;
        data.reserve(chunkSize * 2U);

        for (std::size_t index = 0U; index < chunkSize; ++index) {
            data += toHexByte(result.bytes[offset + index]);
        }

        output << makeHexRecord(
            static_cast<uint8_t>(chunkSize),
            static_cast<uint16_t>(result.origin + offset),
            0x00U,
            data)
               << '\n';
        offset += chunkSize;
    }

    output << ":00000001FF\n";
    return output.str();
}

HexLoadResult Assembler::loadIntelHex(std::string_view source) const {
    HexLoadResult result;
    std::map<uint16_t, uint8_t> image;
    bool sawEof = false;

    std::size_t lineNumber = 1U;
    std::size_t lineStart = 0U;
    while (lineStart <= source.size()) {
        std::size_t lineEnd = source.find('\n', lineStart);
        if (lineEnd == std::string_view::npos) {
            lineEnd = source.size();
        }

        std::string line = trimCopy(source.substr(lineStart, lineEnd - lineStart));
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (!line.empty()) {
            if (line.front() != ':') {
                addError(result.errors, lineNumber, line, "Intel HEX record must start with ':'.");
            } else if (((line.size() - 1U) % 2U) != 0U) {
                addError(result.errors, lineNumber, line, "Intel HEX record length is invalid.");
            } else {
                const std::optional<uint8_t> byteCount = parseHexByte(std::string_view(line).substr(1U, 2U));
                const std::optional<uint8_t> addressHigh = parseHexByte(std::string_view(line).substr(3U, 2U));
                const std::optional<uint8_t> addressLow = parseHexByte(std::string_view(line).substr(5U, 2U));
                const std::optional<uint8_t> recordType = parseHexByte(std::string_view(line).substr(7U, 2U));

                if (!byteCount.has_value() || !addressHigh.has_value() ||
                    !addressLow.has_value() || !recordType.has_value()) {
                    addError(result.errors, lineNumber, line, "Intel HEX record header is invalid.");
                } else {
                    const std::size_t expectedLength = 11U + (static_cast<std::size_t>(*byteCount) * 2U);
                    if (line.size() != expectedLength) {
                        addError(result.errors, lineNumber, line, "Intel HEX record size does not match byte count.");
                    } else {
                        uint8_t checksum = 0U;
                        for (std::size_t index = 1U; index < line.size(); index += 2U) {
                            const std::optional<uint8_t> byte =
                                parseHexByte(std::string_view(line).substr(index, 2U));
                            if (!byte.has_value()) {
                                addError(result.errors, lineNumber, line, "Intel HEX record contains non-hex characters.");
                                checksum = 1U;
                                break;
                            }
                            checksum = static_cast<uint8_t>(checksum + *byte);
                        }

                        if (checksum != 0U) {
                            addError(result.errors, lineNumber, line, "Intel HEX checksum mismatch.");
                        } else {
                            const uint16_t address = static_cast<uint16_t>(
                                (static_cast<uint16_t>(*addressHigh) << 8U) | *addressLow);

                            if (*recordType == 0x00U) {
                                for (std::size_t index = 0U; index < *byteCount; ++index) {
                                    const std::optional<uint8_t> byte = parseHexByte(
                                        std::string_view(line).substr(9U + (index * 2U), 2U));
                                    const uint16_t targetAddress =
                                        static_cast<uint16_t>(address + index);

                                    if (image.find(targetAddress) != image.end()) {
                                        addError(result.errors, lineNumber, line, "Intel HEX contains overlapping data records.");
                                        break;
                                    }

                                    image.emplace(targetAddress, *byte);
                                }
                            } else if (*recordType == 0x01U) {
                                sawEof = true;
                            } else {
                                addError(result.errors, lineNumber, line, "Unsupported Intel HEX record type.");
                            }
                        }
                    }
                }
            }
        }

        if (lineEnd == source.size() || sawEof) {
            break;
        }

        lineStart = lineEnd + 1U;
        ++lineNumber;
    }

    if (!result.errors.empty()) {
        return result;
    }

    if (!image.empty()) {
        result.origin = image.begin()->first;
        const uint16_t finalAddress = image.rbegin()->first;
        result.bytes.assign(static_cast<std::size_t>(finalAddress - result.origin) + 1U, 0xFFU);

        for (const auto& [address, value] : image) {
            result.bytes[static_cast<std::size_t>(address - result.origin)] = value;
        }
    }

    result.success = true;
    return result;
}

}  // namespace Core85
