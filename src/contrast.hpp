#pragma once

// WCAG contrast in integer arithmetic (SPEC.md section 9). Internal.

#include <cstdint>

#include <hh/types.hpp>

namespace hh {
namespace detail {

// The relative luminance times 10^10.
std::uint64_t luminance(rgb c) noexcept;

// The contrast ratio times 100, rounded down.
std::uint32_t contrast_x100(rgb a, rgb b) noexcept;

// `c` with alpha `a` laid over `under`.
rgb over(rgb c, std::uint8_t a, rgb under) noexcept;

// The weakest palette colour against `background`.
std::uint32_t figures_contrast_x100(rgb background) noexcept;

}  // namespace detail
}  // namespace hh
