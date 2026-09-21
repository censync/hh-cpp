#include "candidate.hpp"

#include <vector>

#include "features.hpp"
#include "raster.hpp"

namespace hh {
namespace lab {

namespace {

hh::frame_style library_style(frame_style s) {
    switch (s) {
        case frame_style::none:
        case frame_style::disc:
            return hh::frame_style::none;
        case frame_style::single:
        case frame_style::ring:
            return hh::frame_style::plain;
        case frame_style::double_line:
        case frame_style::ring_double:
            return hh::frame_style::double_line;
        case frame_style::thick:
        case frame_style::ring_thick:
            return hh::frame_style::thick;
        case frame_style::octagon:
            return hh::frame_style::chamfered;
        case frame_style::rounded:
            return hh::frame_style::rounded;
        case frame_style::brackets:
            return hh::frame_style::brackets;
        case frame_style::ring_gapped:
            return hh::frame_style::gaps;
        case frame_style::ring_ticks:
            return hh::frame_style::ticks;
    }
    return hh::frame_style::none;
}

hh::image_shape library_shape(frame_style s) {
    return is_round(s) ? hh::image_shape::round : hh::image_shape::square;
}

}  // namespace

const char* figure_name(figure f) {
    switch (f) {
        case figure::none:
            return "none";
        case figure::square:
            return "square";
        case figure::circle:
            return "circle";
        case figure::up:
            return "up";
        case figure::right:
            return "right";
        case figure::down:
            return "down";
        case figure::left:
            return "left";
    }
    return "?";
}

bool is_triangle(figure f) {
    return f == figure::up || f == figure::right || f == figure::down || f == figure::left;
}

int figure_family(figure f) {
    switch (f) {
        case figure::none:
            return 0;
        case figure::square:
            return 1;
        case figure::circle:
            return 2;
        default:
            return 3;
    }
}

cells features(const std::uint8_t* fp, directions) {
    const auto library_cells = hh::detail::cells_of(fp);
    cells out{};
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i].fig = static_cast<figure>(library_cells[i].figure);
        // The lab keeps the colour bits of empty cells, so that an edited cell has a colour.
        out[i].colour = static_cast<std::uint8_t>((fp[i] >> 3) & 3u);
    }
    return out;
}

const char* frame_name(frame_style s) {
    switch (s) {
        case frame_style::none:
            return "no frame";
        case frame_style::single:
            return "plain frame";
        case frame_style::double_line:
            return "double frame";
        case frame_style::octagon:
            return "chamfered";
        case frame_style::rounded:
            return "rounded corners";
        case frame_style::brackets:
            return "corner brackets";
        case frame_style::thick:
            return "thick frame";
        case frame_style::disc:
            return "no ring";
        case frame_style::ring:
            return "plain ring";
        case frame_style::ring_double:
            return "double ring";
        case frame_style::ring_thick:
            return "thick ring";
        case frame_style::ring_gapped:
            return "ring with four gaps";
        case frame_style::ring_ticks:
            return "ring with four ticks";
    }
    return "?";
}

bool is_round(frame_style s) {
    return s == frame_style::disc || s == frame_style::ring || s == frame_style::ring_double ||
           s == frame_style::ring_thick || s == frame_style::ring_gapped ||
           s == frame_style::ring_ticks;
}

geometry make_geometry(int size, frame_style frame) {
    hh::detail::geometry g{};
    hh::detail::make_geometry(static_cast<std::uint32_t>(size), library_shape(frame),
                              library_style(frame), g);
    return {g.size, g.line, g.gutter, g.cell, g.offset, is_round(frame)};
}

design initial_design() {
    return {{{{0xAB, 0x37, 0x45}, {0x00, 0x48, 0xFF}, {0xEE, 0x71, 0x03}, {0x18, 0xA1, 0xCB}}},
            {0x80, 0x80, 0x80},
            directions::four};
}

design proposed_design() {
    return {{{{0x7A, 0x96, 0xC5}, {0x89, 0x0A, 0xF0}, {0xC1, 0x04, 0x45}, {0xD4, 0x82, 0x00}}},
            {0x80, 0x80, 0x80},
            directions::four};
}

design search_design() {
    return {{{{0x30, 0xA3, 0xB0}, {0x88, 0x06, 0xF5}, {0xBD, 0x0D, 0x59}, {0xBB, 0x8E, 0x05}}},
            {0x80, 0x80, 0x80},
            directions::four};
}

surface surface_of(background bg) {
    return bg == background::white ? surface{{255, 255, 255}, 255} : surface{{0, 0, 0}, 0};
}

canvas render(const cells& c, const design& d, int size, background bg, frame_style frame) {
    return render(c, d, size, surface_of(bg), frame);
}

canvas render(const cells& c, const design& d, int size, const surface& bg, frame_style frame) {
    hh::detail::raster_job job{};
    for (std::size_t i = 0; i < c.size(); ++i) {
        job.cells[i].figure = static_cast<hh::figure>(c[i].fig);
        job.cells[i].colour = c[i].colour;
    }
    for (std::size_t i = 0; i < job.palette.size(); ++i) {
        job.palette[i] = {d.palette[i].r, d.palette[i].g, d.palette[i].b};
    }
    job.frame_rgb = {d.frame_colour.r, d.frame_colour.g, d.frame_colour.b};
    job.shape = library_shape(frame);
    job.frame = library_style(frame);
    job.background = {bg.colour.r, bg.colour.g, bg.colour.b};
    job.background_alpha = bg.alpha;
    job.frame_alpha = d.frame_alpha;

    canvas out(size, size, {0, 0, 0, 0});
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(size) * static_cast<std::size_t>(size) *
                                   4);
    if (hh::detail::rasterise(job, static_cast<std::uint32_t>(size), rgba.data()) !=
        hh::error_code::ok) {
        return out;
    }
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const std::uint8_t* p =
                rgba.data() + (static_cast<std::size_t>(y) * static_cast<std::size_t>(size) +
                               static_cast<std::size_t>(x)) *
                                  4;
            out.set(x, y, {p[0], p[1], p[2], p[3]});
        }
    }
    return out;
}

}  // namespace lab
}  // namespace hh
