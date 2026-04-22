#include "core/memory.h"

#include <algorithm>

namespace Core85 {

Memory::Memory() {
    reset();
}

uint8_t Memory::readByte(uint16_t address) const noexcept {
    return data_[address];
}

void Memory::writeByte(uint16_t address, uint8_t value) noexcept {
    if (isRom(address)) {
        return;
    }

    data_[address] = value;
}

void Memory::loadBytes(Span<const uint8_t> bytes, uint16_t address) noexcept {
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        const auto target =
            static_cast<uint16_t>(address + static_cast<uint16_t>(index));
        writeByte(target, bytes[index]);
    }
}

void Memory::reset() noexcept {
    data_.fill(kDefaultValue);
}

void Memory::setRomRanges(std::vector<AddressRange> ranges) {
    romRanges_ = std::move(ranges);
}

void Memory::addRomRange(uint16_t start, uint16_t end) {
    if (start > end) {
        std::swap(start, end);
    }

    romRanges_.push_back(AddressRange{start, end});
}

bool Memory::isRom(uint16_t address) const noexcept {
    return std::any_of(
        romRanges_.begin(), romRanges_.end(),
        [address](const AddressRange& range) { return range.contains(address); });
}

}  // namespace Core85
