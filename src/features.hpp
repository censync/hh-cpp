#pragma once

// Feature extraction (SPEC.md section 5). Internal.

#include <array>
#include <cstdint>

#include <hh/fingerprint.hpp>

namespace hh {
namespace detail {

// The 16 cells of the 32 fingerprint bytes.
std::array<cell, 16> cells_of(const std::uint8_t* fp) noexcept;

// The 6-character tag and a terminating zero.
std::array<char, 7> tag_of(const std::uint8_t* fp) noexcept;

}  // namespace detail
}  // namespace hh
