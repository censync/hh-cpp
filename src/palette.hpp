#pragma once

// The fixed colours of the algorithm (SPEC.md section 5.2). Internal.

#include <array>

#include <hh/types.hpp>

namespace hh {
namespace detail {

constexpr std::array<rgb, 4> palette = {{
    {0x7A, 0x96, 0xC5},
    {0x89, 0x0A, 0xF0},
    {0xC1, 0x04, 0x45},
    {0xD4, 0x82, 0x00},
}};

constexpr rgb frame_colour = {0x80, 0x80, 0x80};

}  // namespace detail
}  // namespace hh
