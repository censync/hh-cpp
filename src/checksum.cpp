#include "checksum.hpp"

#include <algorithm>
#include <array>

namespace hh {
namespace detail {

namespace {

constexpr std::array<std::uint32_t, 256> make_crc_table() noexcept {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t n = 0; n < 256; ++n) {
        std::uint32_t c = n;
        for (int k = 0; k < 8; ++k) {
            c = (c & 1u) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        }
        table[n] = c;
    }
    return table;
}

constexpr std::array<std::uint32_t, 256> crc_table = make_crc_table();

}  // namespace

std::uint32_t crc32(const std::uint8_t* data, std::size_t size, std::uint32_t crc) noexcept {
    crc = ~crc;
    for (std::size_t i = 0; i < size; ++i) {
        crc = crc_table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    }
    return ~crc;
}

std::uint32_t adler32(const std::uint8_t* data, std::size_t size, std::uint32_t adler) noexcept {
    std::uint32_t a = adler & 0xFFFFu;
    std::uint32_t b = adler >> 16;
    // 5552 is the largest block for which the sums cannot overflow 32 bits.
    while (size != 0) {
        const std::size_t n = std::min<std::size_t>(size, 5552);
        for (std::size_t i = 0; i < n; ++i) {
            a += data[i];
            b += a;
        }
        a %= 65521u;
        b %= 65521u;
        data += n;
        size -= n;
    }
    return (b << 16) | a;
}

}  // namespace detail
}  // namespace hh
