#include <cstdint>
#include <utility>

#include <gtest/gtest.h>

#include "core/io_bus.h"

namespace {

using Core85::IOBus;

TEST(IOBusTest, InputPortsDefaultToZero) {
    IOBus bus;

    EXPECT_EQ(bus.readInputPort(0x00U), 0x00U);
    EXPECT_EQ(bus.readInputPort(0xFFU), 0x00U);
}

TEST(IOBusTest, OutputPortsDefaultToZero) {
    IOBus bus;

    EXPECT_EQ(bus.getOutputPort(0x00U), 0x00U);
    EXPECT_EQ(bus.getOutputPort(0xFFU), 0x00U);
}

TEST(IOBusTest, InputPortsCanBeUpdatedIndependently) {
    IOBus bus;
    bus.setInputPort(0x01U, 0x12U);
    bus.setInputPort(0x02U, 0x34U);

    EXPECT_EQ(bus.readInputPort(0x01U), 0x12U);
    EXPECT_EQ(bus.readInputPort(0x02U), 0x34U);
}

TEST(IOBusTest, OutputPortsRetainLastWrittenValue) {
    IOBus bus;
    bus.writeOutputPort(0x20U, 0xA5U);

    EXPECT_EQ(bus.getOutputPort(0x20U), 0xA5U);
}

TEST(IOBusTest, OutputCallbackReceivesPortAndValue) {
    IOBus bus;
    std::pair<uint8_t, uint8_t> observed{0U, 0U};

    bus.setOnOutputWrite([&observed](uint8_t port, uint8_t value) {
        observed = {port, value};
    });

    bus.writeOutputPort(0x42U, 0x99U);

    EXPECT_EQ(observed.first, 0x42U);
    EXPECT_EQ(observed.second, 0x99U);
}

TEST(IOBusTest, ResetClearsBothPortArrays) {
    IOBus bus;
    bus.setInputPort(0x11U, 0x55U);
    bus.writeOutputPort(0x22U, 0x66U);

    bus.reset();

    EXPECT_EQ(bus.readInputPort(0x11U), 0x00U);
    EXPECT_EQ(bus.getOutputPort(0x22U), 0x00U);
}

}  // namespace
