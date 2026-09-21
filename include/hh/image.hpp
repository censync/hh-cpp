#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "hh/error.hpp"
#include "hh/export.hpp"
#include "hh/fingerprint.hpp"
#include "hh/types.hpp"

namespace hh {

// Pixels, not pictures: RGBA8888 with straight (non-premultiplied) alpha,
// row-major from the top left. Turning them into a platform image belongs to
// the host.
struct image {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgba;
};

enum class image_shape : std::uint8_t {
    square = 0,
    round = 1,  // the 4 x 4 grid inscribed in a circle, no cell clipped
};

// `none` and `plain` are available in both modes. Every other style is a
// keyed-mode marker: it tells the user that the picture is the private one,
// and render() refuses it for a universal fingerprint.
enum class frame_style : std::uint8_t {
    automatic = 0,  // keyed and square: rounded; otherwise none
    none = 1,
    plain = 2,      // a thin square frame, or a thin ring
    rounded = 3,    // square only: rounded corners
    chamfered = 4,  // square only: four cut corners
    double_line = 5,
    thick = 6,
    brackets = 7,  // square only: corner brackets
    ticks = 8,     // round only: ring with four ticks
    gaps = 9,      // round only: ring with four gaps
};

struct render_options {
    image_shape shape = image_shape::square;
    frame_style frame = frame_style::automatic;
    rgb background = {255, 255, 255};
    std::uint8_t background_alpha = 255;  // 0 transparent .. 255 opaque
    std::uint8_t frame_alpha = 255;       // the frame colour itself is fixed (808080)
};

constexpr std::uint32_t min_image_size = 16;
constexpr std::uint32_t max_image_size = 1024;

// Renders size x size pixels. Fails with invalid_fingerprint, invalid_size,
// invalid_frame or low_contrast as SPEC.md section 6 defines, or with
// out_of_memory; `out` is left empty on failure.
HH_API error_code render(const fingerprint& fp, std::uint32_t size, const render_options& options,
                         image& out) noexcept;

// The same into a caller's buffer of at least size * size * 4 bytes.
HH_API error_code render_into(const fingerprint& fp, std::uint32_t size,
                              const render_options& options, std::uint8_t* rgba,
                              std::size_t capacity) noexcept;

// WCAG contrast ratios times 100 (300 means 3:1), rounded down.
struct contrast_report {
    std::uint32_t figures_x100;  // the weakest palette colour against the background
    std::uint32_t frame_x100;    // the frame against the background
};

// Measures what the options give over a page of the colour `page`; for an
// opaque background the page does not matter. render() refuses an opaque
// background with figures_x100 < 200; hosts should warn below 300.
HH_API contrast_report measure_contrast(const render_options& options, rgb page) noexcept;

}  // namespace hh
