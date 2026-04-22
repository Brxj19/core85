#ifndef CORE85_IO_BUS_H
#define CORE85_IO_BUS_H

#include <array>
#include <cstdint>
#include <functional>

namespace Core85 {

class IOBus {
public:
    using OutputCallback = std::function<void(uint8_t port, uint8_t value)>;

    void reset() noexcept;

    void setInputPort(uint8_t port, uint8_t value) noexcept;
    uint8_t readInputPort(uint8_t port) const noexcept;

    void writeOutputPort(uint8_t port, uint8_t value);
    uint8_t getOutputPort(uint8_t port) const noexcept;

    void setOnOutputWrite(OutputCallback callback);

private:
    std::array<uint8_t, 256> inputPorts_{};
    std::array<uint8_t, 256> outputPorts_{};
    OutputCallback outputCallback_{};
};

}  // namespace Core85

#endif
