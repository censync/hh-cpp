#pragma once

// CRC-32 (ISO 3309, as PNG uses it) and Adler-32 (RFC 1950). Internal.

#include <cstddef>
#include <cstdint>

namespace hh {
namespace detail {

// Continues a CRC-32 over more bytes; start with crc = 0.
std::uint32_t crc32(const std::uint8_t* data, std::size_t size, std::uint32_t crc = 0) noexcept;

// Continues an Adler-32 over more bytes; start with adler = 1.
std::uint32_t adler32(const std::uint8_t* data, std::size_t size, std::uint32_t adler = 1) noexcept;

}  // namespace detail
}  // namespace hh
