#pragma once

#include <cstddef>
#include <cstdint>

namespace hh {

// A non-owning view of bytes. `data` may be null only if `size` is 0.
struct byte_view {
    const std::uint8_t* data;
    std::size_t size;
};

// An sRGB colour.
struct rgb {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
};

constexpr bool operator==(rgb a, rgb b) noexcept {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

constexpr bool operator!=(rgb a, rgb b) noexcept {
    return !(a == b);
}

}  // namespace hh
