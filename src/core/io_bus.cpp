#include "core/io_bus.h"

namespace Core85 {

void IOBus::reset() noexcept {
    inputPorts_.fill(0U);
    outputPorts_.fill(0U);
}

void IOBus::setInputPort(uint8_t port, uint8_t value) noexcept {
    inputPorts_[port] = value;
}

uint8_t IOBus::readInputPort(uint8_t port) const noexcept {
    return inputPorts_[port];
}

void IOBus::writeOutputPort(uint8_t port, uint8_t value) {
    outputPorts_[port] = value;

    if (outputCallback_) {
        outputCallback_(port, value);
    }
}

uint8_t IOBus::getOutputPort(uint8_t port) const noexcept {
    return outputPorts_[port];
}

void IOBus::setOnOutputWrite(OutputCallback callback) {
    outputCallback_ = std::move(callback);
}

}  // namespace Core85
