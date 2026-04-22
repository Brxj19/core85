#include <array>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "core/memory.h"

namespace {

using Core85::AddressRange;
using Core85::Memory;
using Core85::Span;

TEST(MemoryTest, DefaultsToFloatingBusValue) {
    Memory memory;

    EXPECT_EQ(memory.readByte(0x0000U), 0xFFU);
    EXPECT_EQ(memory.readByte(0x8000U), 0xFFU);
    EXPECT_EQ(memory.readByte(0xFFFFU), 0xFFU);
}

TEST(MemoryTest, AllowsReadWriteAtLowerBoundary) {
    Memory memory;
    memory.writeByte(0x0000U, 0x12U);

    EXPECT_EQ(memory.readByte(0x0000U), 0x12U);
}

TEST(MemoryTest, AllowsReadWriteAtUpperBoundary) {
    Memory memory;
    memory.writeByte(0xFFFFU, 0x34U);

    EXPECT_EQ(memory.readByte(0xFFFFU), 0x34U);
}

TEST(MemoryTest, ResetRestoresDefaultValueAcrossMemory) {
    Memory memory;
    memory.writeByte(0x0100U, 0x99U);
    memory.writeByte(0x0200U, 0x77U);

    memory.reset();

    EXPECT_EQ(memory.readByte(0x0100U), 0xFFU);
    EXPECT_EQ(memory.readByte(0x0200U), 0xFFU);
}

TEST(MemoryTest, RomRangesCanBeRegisteredIndividually) {
    Memory memory;
    memory.addRomRange(0x1000U, 0x10FFU);

    EXPECT_TRUE(memory.isRom(0x1000U));
    EXPECT_TRUE(memory.isRom(0x1080U));
    EXPECT_TRUE(memory.isRom(0x10FFU));
    EXPECT_FALSE(memory.isRom(0x1100U));
}

TEST(MemoryTest, RomWriteIsIgnored) {
    Memory memory;
    memory.addRomRange(0x2000U, 0x2000U);

    memory.writeByte(0x2000U, 0x42U);

    EXPECT_EQ(memory.readByte(0x2000U), 0xFFU);
}

TEST(MemoryTest, WritableAddressesOutsideRomStillChange) {
    Memory memory;
    memory.addRomRange(0x2000U, 0x2000U);

    memory.writeByte(0x2001U, 0x42U);

    EXPECT_EQ(memory.readByte(0x2001U), 0x42U);
}

TEST(MemoryTest, RomRangeSetterReplacesPreviousRanges) {
    Memory memory;
    memory.addRomRange(0x1000U, 0x10FFU);
    memory.setRomRanges({AddressRange{0x2000U, 0x20FFU}});

    EXPECT_FALSE(memory.isRom(0x1000U));
    EXPECT_TRUE(memory.isRom(0x2000U));
}

TEST(MemoryTest, ReversedRomRangeIsNormalized) {
    Memory memory;
    memory.addRomRange(0x3004U, 0x3000U);

    EXPECT_TRUE(memory.isRom(0x3002U));
}

TEST(MemoryTest, BulkLoadWritesSequentialBytes) {
    Memory memory;
    const std::array<uint8_t, 4> bytes{0x01U, 0x02U, 0x03U, 0x04U};

    memory.loadBytes(Span<const uint8_t>(bytes.data(), bytes.size()), 0x4000U);

    EXPECT_EQ(memory.readByte(0x4000U), 0x01U);
    EXPECT_EQ(memory.readByte(0x4001U), 0x02U);
    EXPECT_EQ(memory.readByte(0x4002U), 0x03U);
    EXPECT_EQ(memory.readByte(0x4003U), 0x04U);
}

TEST(MemoryTest, BulkLoadRespectsRomProtection) {
    Memory memory;
    memory.addRomRange(0x5001U, 0x5001U);
    const std::vector<uint8_t> bytes{0x10U, 0x20U, 0x30U};

    memory.loadBytes(Span<const uint8_t>(bytes.data(), bytes.size()), 0x5000U);

    EXPECT_EQ(memory.readByte(0x5000U), 0x10U);
    EXPECT_EQ(memory.readByte(0x5001U), 0xFFU);
    EXPECT_EQ(memory.readByte(0x5002U), 0x30U);
}

}  // namespace
