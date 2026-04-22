#ifndef CORE85_ASSEMBLER_H
#define CORE85_ASSEMBLER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Core85 {

struct AssemblerError {
    std::size_t line = 0U;
    std::string token;
    std::string message;
};

struct SourceMappingEntry {
    uint16_t address = 0U;
    std::size_t line = 0U;
};

struct AssemblyResult {
    bool success = false;
    uint16_t origin = 0U;
    std::vector<uint8_t> bytes;
    std::vector<AssemblerError> errors;
    std::vector<SourceMappingEntry> sourceMap;
    std::unordered_map<std::string, uint16_t> symbols;
};

class Assembler {
public:
    AssemblyResult assemble(std::string_view source) const;
};

}  // namespace Core85

#endif
