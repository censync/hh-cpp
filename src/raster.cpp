#include "raster.hpp"

#include <algorithm>
#include <new>

#include "contrast.hpp"
#include "features.hpp"
#include "palette.hpp"

namespace hh {
namespace detail {

namespace {

using i32 = std::int32_t;
using u32 = std::uint32_t;

i32 iabs(i32 v) noexcept {
    return v < 0 ? -v : v;
}

// The per-sample tests of SPEC.md sections 8.2 and 8.3.
class frame_tester {
public:
    frame_tester(const geometry& g, image_shape shape, frame_style style) noexcept
        : round_(shape == image_shape::round),
          style_(style),
          s8_(8 * g.size),
          w8_(8 * g.line),
          r_(4 * g.size),
          corner_(std::min<i32>(16 * g.offset, 4 * g.size)),
          diagonal_((8 * g.line * 1414 + 500) / 1000),
          chamfer_(std::max<i32>(8, (16 * g.offset - diagonal_ - w8_) / 8 * 8)),
          bracket_(8 * (g.size / 4)),
          gap_(8 * std::max<i32>(1, g.size / 24)),
          tick_end_(0) {
        const i32 inner = r_ - w8_;
        tick_end_ = inner - std::max<i32>(8, (inner - 4 * g.grid) * 6 / 10);
    }

    bool in_outline(i32 u, i32 v) const noexcept {
        if (round_) {
            const i32 dx = u - r_;
            const i32 dy = v - r_;
            return dx * dx + dy * dy <= r_ * r_;
        }
        const i32 du = std::min(u, s8_ - u);
        const i32 dv = std::min(v, s8_ - v);
        if (style_ == frame_style::rounded) {
            if (du < corner_ && dv < corner_) {
                const i32 ex = corner_ - du;
                const i32 ey = corner_ - dv;
                return ex * ex + ey * ey <= corner_ * corner_;
            }
            return true;
        }
        if (style_ == frame_style::chamfered) {
            return du + dv >= chamfer_;
        }
        return true;
    }

    // Only meaningful for samples inside the outline.
    bool on_frame(i32 u, i32 v) const noexcept {
        if (style_ == frame_style::none) {
            return false;
        }
        if (round_) {
            const i32 dx = u - r_;
            const i32 dy = v - r_;
            const i32 d2 = dx * dx + dy * dy;
            const bool ring = d2 > (r_ - w8_) * (r_ - w8_);
            switch (style_) {
                case frame_style::plain:
                    return ring;
                case frame_style::double_line:
                    return ring || (d2 > (r_ - 3 * w8_) * (r_ - 3 * w8_) &&
                                    d2 <= (r_ - 2 * w8_) * (r_ - 2 * w8_));
                case frame_style::thick:
                    return d2 > (r_ - 3 * w8_) * (r_ - 3 * w8_);
                case frame_style::gaps:
                    return ring && iabs(iabs(dx) - iabs(dy)) >= gap_;
                case frame_style::ticks:
                    return ring || (iabs(dx) < w8_ && iabs(dy) >= tick_end_) ||
                           (iabs(dy) < w8_ && iabs(dx) >= tick_end_);
                default:
                    return false;
            }
        }
        const i32 du = std::min(u, s8_ - u);
        const i32 dv = std::min(v, s8_ - v);
        const i32 e = std::min(du, dv);
        switch (style_) {
            case frame_style::plain:
                return e < w8_;
            case frame_style::double_line:
                return e < w8_ || (e >= 2 * w8_ && e < 3 * w8_);
            case frame_style::thick:
                return e < 3 * w8_;
            case frame_style::brackets:
                return e < w8_ && std::max(du, dv) < bracket_;
            case frame_style::chamfered:
                return e < w8_ || du + dv - chamfer_ < diagonal_;
            case frame_style::rounded:
                if (du < corner_ && dv < corner_) {
                    const i32 ex = corner_ - du;
                    const i32 ey = corner_ - dv;
                    return ex * ex + ey * ey > (corner_ - w8_) * (corner_ - w8_);
                }
                return e < w8_;
            default:
                return false;
        }
    }

private:
    bool round_;
    frame_style style_;
    i32 s8_;
    i32 w8_;
    i32 r_;
    i32 corner_;    // R
    i32 diagonal_;  // D
    i32 chamfer_;   // C
    i32 bracket_;
    i32 gap_;
    i32 tick_end_;  // Q
};

// SPEC.md section 8.4; `h` is H = 4 t.
bool in_figure(figure f, i32 u, i32 v, i32 h) noexcept {
    switch (f) {
        case figure::none:
            return false;
        case figure::square:
            return true;
        case figure::circle:
            return (u - h) * (u - h) + (v - h) * (v - h) <= h * h;
        case figure::triangle_up:
            return 2 * iabs(u - h) <= v;
        case figure::triangle_down:
            return 2 * iabs(u - h) <= 2 * h - v;
        case figure::triangle_right:
            return 2 * iabs(v - h) <= 2 * h - u;
        case figure::triangle_left:
            return 2 * iabs(v - h) <= u;
    }
    return false;
}

struct pixel {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
    std::uint8_t a;
};

// MIX of SPEC.md section 8.5.
pixel mix(rgb fg, u32 fg_alpha, u32 n_fg, rgb bg, u32 bg_alpha, u32 n_bg) noexcept {
    const u32 a = n_fg * (255u * fg_alpha + bg_alpha * (255u - fg_alpha)) + n_bg * 255u * bg_alpha;
    const u32 alpha = (a + 2040u) / 4080u;
    if (alpha == 0) {
        return {0, 0, 0, 0};
    }
    auto channel = [&](std::uint8_t f, std::uint8_t b) {
        const u32 p = n_fg * (255u * fg_alpha * f + bg_alpha * (255u - fg_alpha) * b) +
                      n_bg * 255u * bg_alpha * b;
        return static_cast<std::uint8_t>((p + a / 2u) / a);
    };
    return {channel(fg.r, bg.r), channel(fg.g, bg.g), channel(fg.b, bg.b),
            static_cast<std::uint8_t>(alpha)};
}

void put(std::uint8_t* rgba, i32 size, i32 x, i32 y, pixel p) noexcept {
    std::uint8_t* q = rgba + (static_cast<std::size_t>(y) * static_cast<std::size_t>(size) +
                              static_cast<std::size_t>(x)) *
                                 4;
    q[0] = p.r;
    q[1] = p.g;
    q[2] = p.b;
    q[3] = p.a;
}

bool valid_shape(image_shape s) noexcept {
    return s == image_shape::square || s == image_shape::round;
}

bool valid_style(frame_style s) noexcept {
    return static_cast<unsigned>(s) <= static_cast<unsigned>(frame_style::gaps);
}

}  // namespace

frame_style resolve_frame(frame_style style, mode m, image_shape shape) noexcept {
    if (style != frame_style::automatic) {
        return style;
    }
    return (m == mode::keyed && shape == image_shape::square) ? frame_style::rounded
                                                              : frame_style::none;
}

bool frame_allowed(frame_style resolved, image_shape shape) noexcept {
    switch (resolved) {
        case frame_style::none:
        case frame_style::plain:
        case frame_style::double_line:
        case frame_style::thick:
            return true;
        case frame_style::rounded:
        case frame_style::chamfered:
        case frame_style::brackets:
            return shape == image_shape::square;
        case frame_style::ticks:
        case frame_style::gaps:
            return shape == image_shape::round;
        case frame_style::automatic:
            return false;
    }
    return false;
}

bool make_geometry(std::uint32_t size, image_shape shape, frame_style resolved,
                   geometry& out) noexcept {
    const i32 s = static_cast<i32>(size);
    out.size = s;
    out.line = std::max<i32>(1, s / 48);
    out.gutter = std::max<i32>(1, s / 48);
    if (shape == image_shape::round) {
        const i32 k =
            (resolved == frame_style::double_line || resolved == frame_style::thick) ? 3 : 1;
        const i32 margin = k * out.line + out.gutter;
        const i32 limit = (s - 2 * margin) * (s - 2 * margin);
        i32 t = 0;
        while (2 * (4 * (t + 1) + 3 * out.gutter) * (4 * (t + 1) + 3 * out.gutter) <= limit) {
            ++t;
        }
        out.cell = t;
    } else {
        const i32 margin = std::max<i32>(4 * out.line, s / 12);
        out.cell = (s - 2 * margin - 3 * out.gutter) / 4;
    }
    out.grid = 4 * out.cell + 3 * out.gutter;
    out.offset = (s - out.grid) / 2;
    return out.cell >= 1;
}

error_code rasterise(const raster_job& job, std::uint32_t size, std::uint8_t* rgba,
                     bool reference) noexcept {
    if (rgba == nullptr || !valid_shape(job.shape) || !valid_style(job.frame) ||
        job.frame == frame_style::automatic) {
        return error_code::invalid_argument;
    }
    if (size < min_image_size || size > max_image_size) {
        return error_code::invalid_size;
    }
    geometry g{};
    if (!make_geometry(size, job.shape, job.frame, g)) {
        return error_code::invalid_size;
    }
    const frame_tester tester(g, job.shape, job.frame);
    const u32 ab = job.background_alpha;

    // Step 1: the surface and the frame. Inside the grid area every sample is
    // inside the outline and off the frame, so those pixels are the plain surface.
    pixel frame_pixels[17][17];
    bool frame_known[17][17] = {};
    const pixel surface = mix(job.frame_rgb, job.frame_alpha, 0, job.background, ab, 16);
    for (i32 y = 0; y < g.size; ++y) {
        const bool grid_row = y >= g.offset && y < g.offset + g.grid;
        for (i32 x = 0; x < g.size; ++x) {
            if (!reference && grid_row && x >= g.offset && x < g.offset + g.grid) {
                put(rgba, g.size, x, y, surface);
                continue;
            }
            u32 n_in = 0;
            u32 n_frame = 0;
            for (i32 j = 0; j < 4; ++j) {
                const i32 v = 2 * (4 * y + j) + 1;
                for (i32 i = 0; i < 4; ++i) {
                    const i32 u = 2 * (4 * x + i) + 1;
                    if (tester.in_outline(u, v)) {
                        ++n_in;
                        if (tester.on_frame(u, v)) {
                            ++n_frame;
                        }
                    }
                }
            }
            if (!frame_known[n_in][n_frame]) {
                frame_pixels[n_in][n_frame] = mix(job.frame_rgb, job.frame_alpha, n_frame,
                                                  job.background, ab, n_in - n_frame);
                frame_known[n_in][n_frame] = true;
            }
            put(rgba, g.size, x, y, frame_pixels[n_in][n_frame]);
        }
    }

    // Step 2: the figures.
    const i32 h = 4 * g.cell;
    for (std::size_t index = 0; index < job.cells.size(); ++index) {
        const cell& c = job.cells[index];
        if (c.figure == figure::none) {
            continue;
        }
        const rgb colour = job.palette[c.colour & 3u];
        pixel shades[17];
        for (u32 n = 0; n <= 16; ++n) {
            shades[n] = mix(colour, 255, n, job.background, ab, 16 - n);
        }
        const i32 x0 = g.offset + static_cast<i32>(index % 4) * (g.cell + g.gutter);
        const i32 y0 = g.offset + static_cast<i32>(index / 4) * (g.cell + g.gutter);
        for (i32 py = 0; py < g.cell; ++py) {
            for (i32 px = 0; px < g.cell; ++px) {
                u32 n = 0;
                if (c.figure == figure::square) {
                    n = 16;
                } else {
                    for (i32 j = 0; j < 4; ++j) {
                        const i32 v = 2 * (4 * py + j) + 1;
                        for (i32 i = 0; i < 4; ++i) {
                            const i32 u = 2 * (4 * px + i) + 1;
                            if (in_figure(c.figure, u, v, h)) {
                                ++n;
                            }
                        }
                    }
                }
                put(rgba, g.size, x0 + px, y0 + py, shades[n]);
            }
        }
    }
    return error_code::ok;
}

}  // namespace detail

namespace {

error_code prepare(const fingerprint& fp, std::uint32_t size, const render_options& options,
                   detail::raster_job& job) noexcept {
    if (fp.empty()) {
        return error_code::invalid_fingerprint;
    }
    if (!detail::valid_shape(options.shape) || !detail::valid_style(options.frame)) {
        return error_code::invalid_argument;
    }
    if (size < min_image_size || size > max_image_size) {
        return error_code::invalid_size;
    }
    const frame_style resolved = detail::resolve_frame(options.frame, fp.mode(), options.shape);
    if (!detail::frame_allowed(resolved, options.shape)) {
        return error_code::invalid_frame;
    }
    if (options.background_alpha == 255 &&
        detail::figures_contrast_x100(options.background) < 200) {
        return error_code::low_contrast;
    }
    detail::geometry g{};
    if (!detail::make_geometry(size, options.shape, resolved, g)) {
        return error_code::invalid_size;
    }
    job.cells = detail::cells_of(fp.bytes().data());
    job.palette = detail::palette;
    job.frame_rgb = detail::frame_colour;
    job.shape = options.shape;
    job.frame = resolved;
    job.background = options.background;
    job.background_alpha = options.background_alpha;
    job.frame_alpha = options.frame_alpha;
    return error_code::ok;
}

}  // namespace

error_code render_into(const fingerprint& fp, std::uint32_t size, const render_options& options,
                       std::uint8_t* rgba, std::size_t capacity) noexcept {
    if (rgba == nullptr) {
        return error_code::invalid_argument;
    }
    detail::raster_job job{};
    const error_code ec = prepare(fp, size, options, job);
    if (ec != error_code::ok) {
        return ec;
    }
    if (capacity < static_cast<std::size_t>(size) * size * 4) {
        return error_code::buffer_too_small;
    }
    return detail::rasterise(job, size, rgba);
}

error_code render(const fingerprint& fp, std::uint32_t size, const render_options& options,
                  image& out) noexcept {
    out.width = 0;
    out.height = 0;
    out.rgba.clear();
    detail::raster_job job{};
    error_code ec = prepare(fp, size, options, job);
    if (ec != error_code::ok) {
        return ec;
    }
    try {
        out.rgba.resize(static_cast<std::size_t>(size) * size * 4);
    } catch (const std::bad_alloc&) {
        return error_code::out_of_memory;
    } catch (...) {
        return error_code::out_of_memory;
    }
    ec = detail::rasterise(job, size, out.rgba.data());
    if (ec != error_code::ok) {
        out.rgba.clear();
        return ec;
    }
    out.width = size;
    out.height = size;
    return error_code::ok;
}

}  // namespace hh
