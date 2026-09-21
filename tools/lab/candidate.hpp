#pragma once

// The renderer the lab's sheets are drawn with. It is the library's own
// rasteriser (src/raster.hpp), so a sheet shows exactly what hh renders; the
// lab only adds what the library fixes: other palettes and edited cells.

#include <array>
#include <cstdint>

#include "canvas.hpp"
#include "colour.hpp"

namespace hh {
namespace lab {

// The same values as hh::figure.
enum class figure : std::uint8_t {
    none,
    square,
    circle,
    up,
    right,
    down,
    left
};

const char* figure_name(figure f);
bool is_triangle(figure f);

// Figure family: 0 none, 1 square, 2 circle, 3 triangle.
int figure_family(figure f);

struct cell {
    figure fig;
    std::uint8_t colour;
};

using cells = std::array<cell, 16>;

// Triangle directions: the algorithm uses four.
enum class directions {
    four
};

// The cells of fingerprint bytes 0..15, as SPEC.md section 5.1 defines them.
cells features(const std::uint8_t* fp, directions dirs);

// The frame styles of the library under the names the sheets use. `none` and
// `single` (square) and `disc` and `ring` (round) are open to both modes; the
// rest are keyed-mode markers.
enum class frame_style {
    none,
    single,
    double_line,
    octagon,
    rounded,
    brackets,
    thick,
    disc,
    ring,
    ring_double,
    ring_thick,
    ring_gapped,
    ring_ticks,
};

const char* frame_name(frame_style s);
bool is_round(frame_style s);

enum class background {
    white,
    transparent
};

struct geometry {
    int size;
    int frame_width;
    int gutter;
    int cell;
    int offset;
    bool round;
};

// The library's geometry for a size and a frame style. A cell size of 0 means
// the style does not fit the size.
geometry make_geometry(int size, frame_style frame = frame_style::none);

struct design {
    std::array<rgb8, 4> palette;
    rgb8 frame_colour;
    directions dirs;
    std::uint8_t frame_alpha = 255;
};

// The palette the search started from.
design initial_design();

// The palette of the algorithm (SPEC.md section 5.2).
design proposed_design();

// The best palette of the frame-aware search.
design search_design();

// A host surface: any sRGB colour with straight alpha.
struct surface {
    rgb8 colour;
    std::uint8_t alpha;
};

surface surface_of(background bg);

// Draws through hh::detail::rasterise. The mode and contrast rules of
// hh::render() do not apply here: the lab shows what the library refuses too.
canvas render(const cells& c, const design& d, int size, const surface& bg, frame_style frame);
canvas render(const cells& c, const design& d, int size, background bg, frame_style frame);

}  // namespace lab
}  // namespace hh
