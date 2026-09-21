#pragma once

// Geometry and rasterisation (SPEC.md sections 6 to 8). Internal; the design
// lab in tools/lab draws its sheets through rasterise() as well, so what the
// sheets show is what the library renders.

#include <array>
#include <cstdint>

#include <hh/error.hpp>
#include <hh/fingerprint.hpp>
#include <hh/image.hpp>
#include <hh/types.hpp>

namespace hh {
namespace detail {

struct geometry {
    std::int32_t size;    // S
    std::int32_t line;    // w
    std::int32_t gutter;  // g
    std::int32_t cell;    // t
    std::int32_t grid;    // G
    std::int32_t offset;  // o
};

// `automatic` resolved for the mode and the shape; other styles unchanged.
frame_style resolve_frame(frame_style style, mode m, image_shape shape) noexcept;

// Whether a resolved style may be used with the shape and the mode.
bool frame_allowed(frame_style resolved, mode m, image_shape shape) noexcept;

// False if the size leaves no room for the cells (t = 0).
bool make_geometry(std::uint32_t size, image_shape shape, frame_style resolved,
                   geometry& out) noexcept;

// Everything a raster depends on. `frame` must be a resolved style.
struct raster_job {
    std::array<cell, 16> cells;
    std::array<rgb, 4> palette;
    rgb frame_rgb;
    image_shape shape;
    frame_style frame;
    rgb background;
    std::uint8_t background_alpha;
    std::uint8_t frame_alpha;
};

// Writes size * size * 4 bytes. Checks the size range and the geometry only;
// the mode and contrast rules belong to render(). With `reference` set every
// pixel is evaluated sample by sample (no shortcut for the grid area); the
// result is identical and the tests compare the two.
error_code rasterise(const raster_job& job, std::uint32_t size, std::uint8_t* rgba,
                     bool reference = false) noexcept;

}  // namespace detail
}  // namespace hh
