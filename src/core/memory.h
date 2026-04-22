#ifndef CORE85_MEMORY_H
#define CORE85_MEMORY_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/span.h"

namespace Core85 {

struct AddressRange {
    uint16_t start = 0U;
    uint16_t end = 0U;

    bool contains(uint16_t address) const noexcept {
        return address >= start && address <= end;
    }
};

class Memory {
public:
    static constexpr std::size_t kSize = 65536U;
    static constexpr uint8_t kDefaultValue = 0xFFU;

    Memory();

    uint8_t readByte(uint16_t address) const noexcept;
    void writeByte(uint16_t address, uint8_t value) noexcept;
    void loadBytes(Span<const uint8_t> bytes, uint16_t address) noexcept;

    void reset() noexcept;

    void setRomRanges(std::vector<AddressRange> ranges);
    void addRomRange(uint16_t start, uint16_t end);
    bool isRom(uint16_t address) const noexcept;

    const std::array<uint8_t, kSize>& raw() const noexcept { return data_; }

private:
    std::array<uint8_t, kSize> data_{};
    std::vector<AddressRange> romRanges_;
};

}  // namespace Core85

#endif
