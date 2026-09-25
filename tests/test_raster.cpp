#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "palette.hpp"
#include "raster.hpp"
#include "test_framework.hpp"
#include "test_util.hpp"

using hh::error_code;
using hh::frame_style;
using hh::image_shape;
using hh_test::fingerprint_of;

namespace {

// Every figure in every colour: squares, circles, then the triangles, and two empty cells.
std::vector<std::uint8_t> sampler_bytes() {
    const std::uint8_t cells[16] = {0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70, 0x78,
                                    0x80, 0xA8, 0xD0, 0xF8, 0x98, 0xB0, 0x00, 0x20};
    std::vector<std::uint8_t> bytes(32, 0);
    std::copy(cells, cells + 16, bytes.begin());
    return bytes;
}

const std::uint8_t* pixel(const hh::image& img, std::uint32_t x, std::uint32_t y) {
    return img.rgba.data() + (static_cast<std::size_t>(y) * img.width + x) * 4;
}

const frame_style square_styles[] = {
    frame_style::none,        frame_style::plain, frame_style::rounded, frame_style::chamfered,
    frame_style::double_line, frame_style::thick, frame_style::brackets};
const frame_style round_styles[] = {frame_style::none,        frame_style::plain,
                                    frame_style::double_line, frame_style::thick,
                                    frame_style::ticks,       frame_style::gaps};

}  // namespace

TEST_CASE("raster: geometry of the square shape") {
    struct row {
        std::uint32_t size;
        int line, cell, grid, offset;
    };
    // w = max(1, S/48), m = max(4w, S/12), t = (S - 2m - 3g) / 4, o = (S - G) / 2.
    const row rows[] = {{16, 1, 1, 7, 4},      {32, 1, 5, 23, 4},       {48, 1, 9, 39, 4},
                        {64, 1, 12, 51, 6},    {96, 2, 18, 78, 9},      {128, 2, 25, 106, 11},
                        {256, 5, 49, 211, 22}, {1024, 21, 197, 851, 86}};
    for (const row& r : rows) {
        hh::detail::geometry g{};
        EXPECT_TRUE(hh::detail::make_geometry(r.size, image_shape::square, frame_style::none, g));
        EXPECT_EQ(g.line, r.line);
        EXPECT_EQ(g.cell, r.cell);
        EXPECT_EQ(g.grid, r.grid);
        EXPECT_EQ(g.offset, r.offset);
    }
}

TEST_CASE("raster: geometry of the round shape") {
    struct row {
        std::uint32_t size;
        frame_style style;
        int cell;
    };
    const row rows[] = {{16, frame_style::none, 1},   {32, frame_style::plain, 4},
                        {48, frame_style::ticks, 7},  {64, frame_style::gaps, 9},
                        {128, frame_style::none, 19}, {1024, frame_style::plain, 150},
                        {48, frame_style::thick, 6},  {128, frame_style::double_line, 18},
                        {18, frame_style::thick, 1}};
    for (const row& r : rows) {
        hh::detail::geometry g{};
        EXPECT_TRUE(hh::detail::make_geometry(r.size, image_shape::round, r.style, g));
        EXPECT_EQ(g.cell, r.cell);
        // The grid corners stay inside the circle that leaves the margin free.
        const int margin =
            (r.style == frame_style::thick || r.style == frame_style::double_line ? 3 * g.line
                                                                                  : g.line) +
            g.gutter;
        const int room = static_cast<int>(r.size) - 2 * margin;
        EXPECT_TRUE(2 * g.grid * g.grid <= room * room);
        const int next = g.grid + 4;
        EXPECT_TRUE(2 * next * next > room * room);
    }
    hh::detail::geometry g{};
    EXPECT_FALSE(hh::detail::make_geometry(16, image_shape::round, frame_style::thick, g));
    EXPECT_FALSE(hh::detail::make_geometry(17, image_shape::round, frame_style::double_line, g));
}

TEST_CASE("raster: the grid shortcut equals the sample-by-sample reference") {
    hh::detail::raster_job job{};
    const auto fp = fingerprint_of(sampler_bytes(), hh::mode::keyed);
    job.cells = hh::describe(fp).cells;
    job.palette = hh::detail::palette;
    job.frame_rgb = hh::detail::frame_colour;
    const std::uint32_t sizes[] = {16, 17, 18, 19, 23, 31, 32, 47, 48, 63, 64, 97, 128, 200};
    struct surface {
        hh::rgb colour;
        std::uint8_t alpha;
        std::uint8_t frame_alpha;
    };
    const surface surfaces[] = {
        {{255, 255, 255}, 255, 255}, {{0, 0, 0}, 0, 255}, {{0x12, 0x34, 0x56}, 128, 77}};
    for (int round = 0; round < 2; ++round) {
        job.shape = round ? image_shape::round : image_shape::square;
        const auto* styles = round ? round_styles : square_styles;
        const std::size_t count = round ? 6 : 7;
        for (std::size_t s = 0; s < count; ++s) {
            job.frame = styles[s];
            for (const surface& sf : surfaces) {
                job.background = sf.colour;
                job.background_alpha = sf.alpha;
                job.frame_alpha = sf.frame_alpha;
                for (std::uint32_t size : sizes) {
                    std::vector<std::uint8_t> fast(static_cast<std::size_t>(size) * size * 4, 0xEE);
                    std::vector<std::uint8_t> reference(fast.size(), 0x11);
                    const error_code a = hh::detail::rasterise(job, size, fast.data(), false);
                    const error_code b = hh::detail::rasterise(job, size, reference.data(), true);
                    EXPECT_EQ(a, b);
                    if (a == error_code::ok) {
                        EXPECT_TRUE(fast == reference);
                    }
                }
            }
        }
    }
}

TEST_CASE("raster: default pixels follow the plain blend formulas") {
    const auto fp = fingerprint_of(sampler_bytes(), hh::mode::universal);
    hh::image white;
    EXPECT_EQ(hh::render(fp, 64, hh::render_options{}, white), error_code::ok);
    EXPECT_EQ(white.width, 64u);
    EXPECT_EQ(white.rgba.size(), std::size_t{64 * 64 * 4});
    hh::render_options transparent_options;
    transparent_options.background_alpha = 0;
    hh::image transparent;
    EXPECT_EQ(hh::render(fp, 64, transparent_options, transparent), error_code::ok);

    // On white every pixel is opaque; on a transparent surface the colour is the
    // figure colour and alpha = (n * 255 + 8) / 16, so both encode the same coverage n.
    bool consistent = true;
    for (std::uint32_t y = 0; y < 64 && consistent; ++y) {
        for (std::uint32_t x = 0; x < 64 && consistent; ++x) {
            const std::uint8_t* w = pixel(white, x, y);
            const std::uint8_t* t = pixel(transparent, x, y);
            consistent = w[3] == 255;
            if (t[3] == 0) {
                consistent = consistent && w[0] == 255 && w[1] == 255 && w[2] == 255 && t[0] == 0;
                continue;
            }
            bool found = false;
            for (unsigned n = 1; n <= 16 && !found; ++n) {
                if (t[3] != (n * 255 + 8) / 16) {
                    continue;
                }
                found = true;
                for (int c = 0; c < 3; ++c) {
                    consistent = consistent && w[c] == (n * t[c] + (16 - n) * 255 + 8) / 16;
                }
            }
            consistent = consistent && found;
        }
    }
    EXPECT_TRUE(consistent);
}

TEST_CASE("raster: cells are where the geometry says and hold their colour") {
    const auto fp = fingerprint_of(sampler_bytes(), hh::mode::universal);
    hh::image img;
    EXPECT_EQ(hh::render(fp, 128, hh::render_options{}, img), error_code::ok);
    hh::detail::geometry g{};
    hh::detail::make_geometry(128, image_shape::square, frame_style::none, g);
    const hh::layout l = hh::describe(fp);
    for (std::uint32_t i = 0; i < 16; ++i) {
        const std::uint32_t cx = static_cast<std::uint32_t>(
            g.offset + static_cast<int>(i % 4) * (g.cell + g.gutter) + g.cell / 2);
        const std::uint32_t cy = static_cast<std::uint32_t>(
            g.offset + static_cast<int>(i / 4) * (g.cell + g.gutter) + g.cell / 2);
        const std::uint8_t* p = pixel(img, cx, cy);
        if (l.cells[i].figure == hh::figure::none) {
            EXPECT_TRUE(p[0] == 255 && p[1] == 255 && p[2] == 255);
        } else {
            // The centre of a cell belongs to every figure.
            const hh::rgb c = hh::detail::palette[l.cells[i].colour];
            EXPECT_TRUE(p[0] == c.r && p[1] == c.g && p[2] == c.b && p[3] == 255);
        }
    }
    // A universal image has no frame by default: the corner pixel is the plain surface.
    const std::uint8_t* corner = pixel(img, 0, 0);
    EXPECT_TRUE(corner[0] == 255 && corner[1] == 255 && corner[2] == 255 && corner[3] == 255);
}

TEST_CASE("raster: figures cover what their shape implies") {
    // One figure per image, measured by counting pixels of its colour at full coverage.
    auto coverage = [](std::uint8_t byte) {
        std::vector<std::uint8_t> bytes(32, 0);
        bytes[5] = byte;
        hh::image img;
        hh::render(fingerprint_of(bytes, hh::mode::universal), 256, hh::render_options{}, img);
        std::uint64_t sum = 0;  // in sixteenths of a pixel
        for (std::size_t p = 0; p < img.rgba.size(); p += 4) {
            // Colour 0 is 7A96C5; the red channel falls linearly from 255 to 0x7A with coverage.
            sum += (255u - img.rgba[p]) * 16u / (255u - 0x7Au);
        }
        return sum;
    };
    hh::detail::geometry g{};
    hh::detail::make_geometry(256, image_shape::square, frame_style::none, g);
    const std::uint64_t cell =
        static_cast<std::uint64_t>(g.cell) * static_cast<std::uint64_t>(g.cell) * 16u;
    const std::uint64_t square = coverage(0x40);
    EXPECT_EQ(square, cell);
    const std::uint64_t circle = coverage(0x60);
    EXPECT_TRUE(circle > cell * 77 / 100 && circle < cell * 80 / 100);  // pi / 4 = 0.785
    for (unsigned code : {0x80u, 0xA0u, 0xC0u, 0xE0u}) {
        const auto byte = static_cast<std::uint8_t>(code);
        const std::uint64_t triangle = coverage(byte);
        EXPECT_TRUE(triangle > cell * 48 / 100 && triangle < cell * 52 / 100);
    }
}

TEST_CASE("raster: triangles point where their name says") {
    // The centre of mass of the ink, in hundredths of a pixel.
    auto centre_of_mass = [](std::uint8_t byte, std::int64_t& mx, std::int64_t& my) {
        std::vector<std::uint8_t> bytes(32, 0);
        bytes[0] = byte;
        hh::image img;
        hh::render(fingerprint_of(bytes, hh::mode::universal), 128, hh::render_options{}, img);
        std::int64_t sx = 0, sy = 0, n = 0;
        for (std::uint32_t y = 0; y < 128; ++y) {
            for (std::uint32_t x = 0; x < 128; ++x) {
                const std::int64_t weight = 255 - pixel(img, x, y)[0];
                sx += std::int64_t{x} * weight;
                sy += std::int64_t{y} * weight;
                n += weight;
            }
        }
        mx = sx * 100 / n;
        my = sy * 100 / n;
    };
    hh::detail::geometry g{};
    hh::detail::make_geometry(128, image_shape::square, frame_style::none, g);
    const std::int64_t middle = (2 * g.offset + g.cell - 1) * 50;
    std::int64_t x = 0, y = 0;
    centre_of_mass(0x80, x, y);  // up: the mass sits at the bottom
    EXPECT_TRUE(y > middle + 200 && x > middle - 50 && x < middle + 50);
    centre_of_mass(0xC0, x, y);  // down
    EXPECT_TRUE(y < middle - 200 && x > middle - 50 && x < middle + 50);
    centre_of_mass(0xA0, x, y);  // right: the base is on the left
    EXPECT_TRUE(x < middle - 200 && y > middle - 50 && y < middle + 50);
    centre_of_mass(0xE0, x, y);  // left
    EXPECT_TRUE(x > middle + 200 && y > middle - 50 && y < middle + 50);
}

TEST_CASE("raster: outside the outline is transparent") {
    const auto keyed = fingerprint_of(sampler_bytes(), hh::mode::keyed);
    hh::image rounded;
    EXPECT_EQ(hh::render(keyed, 128, hh::render_options{}, rounded),
              error_code::ok);  // automatic: rounded
    EXPECT_EQ(static_cast<int>(pixel(rounded, 0, 0)[3]), 0);
    EXPECT_EQ(static_cast<int>(pixel(rounded, 127, 127)[3]), 0);
    EXPECT_EQ(static_cast<int>(pixel(rounded, 64, 0)[3]), 255);  // the frame on the top side
    EXPECT_EQ(static_cast<int>(pixel(rounded, 64, 0)[0]), 0x80);

    hh::render_options round_options;
    round_options.shape = image_shape::round;
    hh::image disc;
    EXPECT_EQ(hh::render(keyed, 128, round_options, disc), error_code::ok);
    EXPECT_EQ(static_cast<int>(pixel(disc, 0, 0)[3]), 0);
    EXPECT_EQ(static_cast<int>(pixel(disc, 64, 1)[3]), 255);
    EXPECT_EQ(static_cast<int>(pixel(disc, 64, 1)[0]),
              255);  // automatic is "none" for the round shape
}

TEST_CASE("raster: rounded corners never reach beyond the middle of a side") {
    // At 17 and 18 px twice the margin exceeds half the side; the radius is capped there, so
    // the middle of every side still carries the full frame line.
    const auto keyed = fingerprint_of(sampler_bytes(), hh::mode::keyed);
    for (std::uint32_t size : {16u, 17u, 18u, 19u, 20u}) {
        hh::image img;
        EXPECT_EQ(hh::render(keyed, size, hh::render_options{}, img), error_code::ok);
        const std::uint32_t mid = size / 2;
        for (const std::uint8_t* p : {pixel(img, mid, 0), pixel(img, mid, size - 1),
                                      pixel(img, 0, mid), pixel(img, size - 1, mid)}) {
            EXPECT_TRUE(p[0] == 0x80 && p[1] == 0x80 && p[2] == 0x80 && p[3] == 255);
        }
    }
}

TEST_CASE("raster: every style fits its shape in both modes") {
    const auto universal = fingerprint_of(sampler_bytes(), hh::mode::universal);
    const auto keyed = fingerprint_of(sampler_bytes(), hh::mode::keyed);
    hh::image img;
    for (int round = 0; round < 2; ++round) {
        for (int style = 1; style <= 9; ++style) {
            hh::render_options options;
            options.shape = round ? image_shape::round : image_shape::square;
            options.frame = static_cast<frame_style>(style);
            const bool square_only = style == 3 || style == 4 || style == 7;
            const bool round_only = style == 8 || style == 9;
            const bool fits = !(round && square_only) && !(!round && round_only);
            const error_code expected = fits ? error_code::ok : error_code::invalid_frame;
            EXPECT_EQ(hh::render(universal, 64, options, img), expected);
            EXPECT_EQ(hh::render(keyed, 64, options, img), expected);
        }
    }
}

TEST_CASE("raster: an explicit frame draws the same for both modes") {
    // The frame depends on the style and the shape alone: two fingerprints with the same bytes
    // and different modes give identical pictures for every explicit style.
    const auto universal = fingerprint_of(sampler_bytes(), hh::mode::universal);
    const auto keyed = fingerprint_of(sampler_bytes(), hh::mode::keyed);
    for (int round = 0; round < 2; ++round) {
        for (int style = 1; style <= 9; ++style) {
            hh::render_options options;
            options.shape = round ? image_shape::round : image_shape::square;
            options.frame = static_cast<frame_style>(style);
            hh::image a;
            hh::image b;
            const error_code ea = hh::render(universal, 80, options, a);
            const error_code eb = hh::render(keyed, 80, options, b);
            EXPECT_EQ(ea, eb);
            EXPECT_TRUE(a.rgba == b.rgba);
        }
    }
    // Only `automatic` depends on the mode: keyed square pictures get rounded corners.
    hh::render_options automatic;
    hh::render_options rounded;
    rounded.frame = frame_style::rounded;
    hh::image k_auto;
    hh::image u_rounded;
    EXPECT_EQ(hh::render(keyed, 80, automatic, k_auto), error_code::ok);
    EXPECT_EQ(hh::render(universal, 80, rounded, u_rounded), error_code::ok);
    EXPECT_TRUE(k_auto.rgba == u_rounded.rgba);
}

TEST_CASE("raster: a marker changes the frame and nothing else") {
    const auto keyed = fingerprint_of(sampler_bytes(), hh::mode::keyed);
    hh::render_options none;
    none.frame = frame_style::none;
    hh::render_options thick;
    thick.frame = frame_style::thick;
    hh::image a;
    hh::image b;
    EXPECT_EQ(hh::render(keyed, 96, none, a), error_code::ok);
    EXPECT_EQ(hh::render(keyed, 96, thick, b), error_code::ok);
    hh::detail::geometry g{};
    hh::detail::make_geometry(96, image_shape::square, frame_style::thick, g);
    bool same_inside = true;
    bool differs_outside = false;
    for (std::uint32_t y = 0; y < 96; ++y) {
        for (std::uint32_t x = 0; x < 96; ++x) {
            const bool inside =
                static_cast<int>(x) >= g.offset && static_cast<int>(x) < g.offset + g.grid &&
                static_cast<int>(y) >= g.offset && static_cast<int>(y) < g.offset + g.grid;
            const bool equal = std::equal(pixel(a, x, y), pixel(a, x, y) + 4, pixel(b, x, y));
            same_inside = same_inside && (!inside || equal);
            differs_outside = differs_outside || (!inside && !equal);
        }
    }
    EXPECT_TRUE(same_inside);
    EXPECT_TRUE(differs_outside);
}

TEST_CASE("raster: errors come in the specified order") {
    const auto universal = fingerprint_of(sampler_bytes(), hh::mode::universal);
    const auto keyed = fingerprint_of(sampler_bytes(), hh::mode::keyed);
    hh::image img;
    hh::render_options bad;  // wrong frame for the shape and a background without contrast
    bad.frame = frame_style::ticks;
    bad.background = {0x7A, 0x96, 0xC5};
    EXPECT_EQ(hh::render(hh::fingerprint{}, 64, bad, img), error_code::invalid_fingerprint);
    EXPECT_EQ(hh::render(universal, 15, bad, img), error_code::invalid_size);
    EXPECT_EQ(hh::render(universal, 1025, bad, img), error_code::invalid_size);
    EXPECT_EQ(hh::render(universal, 64, bad, img), error_code::invalid_frame);
    EXPECT_EQ(hh::render(keyed, 64, bad, img), error_code::invalid_frame);
    bad.frame = frame_style::thick;
    EXPECT_EQ(hh::render(universal, 64, bad, img), error_code::low_contrast);
    EXPECT_EQ(hh::render(keyed, 64, bad, img), error_code::low_contrast);
    EXPECT_TRUE(img.rgba.empty());
    EXPECT_EQ(img.width, 0u);

    hh::render_options tight;  // fine except that a thick ring leaves no room at 16 px
    tight.shape = image_shape::round;
    tight.frame = frame_style::thick;
    EXPECT_EQ(hh::render(keyed, 16, tight, img), error_code::invalid_size);
    EXPECT_EQ(hh::render(keyed, 18, tight, img), error_code::ok);

    hh::render_options unknown;
    unknown.frame = static_cast<frame_style>(42);
    EXPECT_EQ(hh::render(keyed, 64, unknown, img), error_code::invalid_argument);
    unknown = hh::render_options{};
    unknown.shape = static_cast<image_shape>(7);
    EXPECT_EQ(hh::render(keyed, 64, unknown, img), error_code::invalid_argument);
}

TEST_CASE("raster: the contrast rule guards opaque backgrounds only") {
    const auto fp = fingerprint_of(sampler_bytes(), hh::mode::universal);
    hh::image img;
    hh::render_options options;
    options.background = {0x9E, 0x9E, 0x9E};
    EXPECT_EQ(hh::render(fp, 64, options, img), error_code::low_contrast);
    options.background_alpha = 254;  // translucent: the host answers for its page
    EXPECT_EQ(hh::render(fp, 64, options, img), error_code::ok);
    options.background_alpha = 255;
    options.background = {0xF2, 0xF2, 0xF2};  // 2.68:1, allowed but below the 3:1 advice
    EXPECT_EQ(hh::render(fp, 64, options, img), error_code::ok);
    EXPECT_TRUE(hh::measure_contrast(options, {0, 0, 0}).figures_x100 < 300u);
    options.background = {0, 0, 0};
    EXPECT_EQ(hh::render(fp, 64, options, img), error_code::ok);
}

TEST_CASE("raster: render_into checks its buffer") {
    const auto fp = fingerprint_of(sampler_bytes(), hh::mode::universal);
    std::vector<std::uint8_t> buffer(32 * 32 * 4 + 1, 0xAB);
    EXPECT_EQ(hh::render_into(fp, 32, hh::render_options{}, buffer.data(), buffer.size() - 2),
              error_code::buffer_too_small);
    EXPECT_EQ(hh::render_into(fp, 32, hh::render_options{}, nullptr, buffer.size()),
              error_code::invalid_argument);
    EXPECT_EQ(hh::render_into(fp, 32, hh::render_options{}, buffer.data(), buffer.size()),
              error_code::ok);
    EXPECT_EQ(static_cast<int>(buffer.back()), 0xAB);  // nothing beyond size * size * 4
    hh::image img;
    EXPECT_EQ(hh::render(fp, 32, hh::render_options{}, img), error_code::ok);
    EXPECT_TRUE(std::equal(img.rgba.begin(), img.rgba.end(), buffer.begin()));
}

TEST_CASE("raster: every size renders in both shapes") {
    const auto keyed = fingerprint_of(sampler_bytes(), hh::mode::keyed);
    hh::image img;
    for (std::uint32_t size = hh::min_image_size; size <= 160; ++size) {
        hh::render_options options;
        EXPECT_EQ(hh::render(keyed, size, options, img), error_code::ok);
        options.shape = image_shape::round;
        options.frame = frame_style::ticks;
        EXPECT_EQ(hh::render(keyed, size, options, img), error_code::ok);
    }
    EXPECT_EQ(hh::render(keyed, hh::max_image_size, hh::render_options{}, img), error_code::ok);
    EXPECT_EQ(img.rgba.size(), std::size_t{1024} * 1024 * 4);
}
