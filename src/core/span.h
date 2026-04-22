#ifndef CORE85_SPAN_H
#define CORE85_SPAN_H

#include <cstddef>

namespace Core85 {

template <typename T>
class Span {
public:
    using element_type = T;
    using pointer = T*;
    using reference = T&;

    constexpr Span() noexcept = default;
    constexpr Span(pointer data, std::size_t size) noexcept
        : data_(data), size_(size) {}

    constexpr pointer data() const noexcept { return data_; }
    constexpr std::size_t size() const noexcept { return size_; }
    constexpr bool empty() const noexcept { return size_ == 0U; }

    constexpr reference operator[](std::size_t index) const noexcept {
        return data_[index];
    }

    constexpr pointer begin() const noexcept { return data_; }
    constexpr pointer end() const noexcept { return data_ + size_; }

private:
    pointer data_ = nullptr;
    std::size_t size_ = 0U;
};

}  // namespace Core85

#endif
