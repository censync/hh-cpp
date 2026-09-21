// Host-chosen surfaces: the host or its user may set the background colour and alpha and the
// frame alpha; the frame colour stays #808080. The palette passes 3:1
// against white and #121212 only, so this sheet shows what other surfaces do to the figures, with
// the WCAG contrast of every palette colour against the effective background.

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "candidate.hpp"
#include "lab.hpp"
#include "sheet.hpp"

namespace hh {
namespace lab {

namespace {

struct host_surface {
    const char* name;
    surface bg;
    rgb8 frame;
    rgb8 page;  // what the host paints behind the image
    std::uint8_t frame_alpha = 255;
};

const host_surface surfaces[] = {
    {"white (default)", {{255, 255, 255}, 255}, {0x80, 0x80, 0x80}, {255, 255, 255}},
    {"transparent on #121212", {{0, 0, 0}, 0}, {0x80, 0x80, 0x80}, {0x12, 0x12, 0x12}},
    {"light grey #F2F2F2", {{0xF2, 0xF2, 0xF2}, 255}, {0x80, 0x80, 0x80}, {0xF2, 0xF2, 0xF2}},
    {"paper #F4ECD8", {{0xF4, 0xEC, 0xD8}, 255}, {0x80, 0x80, 0x80}, {0xF4, 0xEC, 0xD8}},
    {"light blue #E3F0FB", {{0xE3, 0xF0, 0xFB}, 255}, {0x80, 0x80, 0x80}, {0xE3, 0xF0, 0xFB}},
    {"mid grey #9E9E9E", {{0x9E, 0x9E, 0x9E}, 255}, {0x80, 0x80, 0x80}, {0x9E, 0x9E, 0x9E}},
    {"palette blue #7A96C5", {{0x7A, 0x96, 0xC5}, 255}, {0x80, 0x80, 0x80}, {0x7A, 0x96, 0xC5}},
    {"dark navy #0F1B2D", {{0x0F, 0x1B, 0x2D}, 255}, {0x80, 0x80, 0x80}, {0x0F, 0x1B, 0x2D}},
    {"black #000000", {{0, 0, 0}, 255}, {0x80, 0x80, 0x80}, {0, 0, 0}},
    {"white 50 % on #121212", {{255, 255, 255}, 128}, {0x80, 0x80, 0x80}, {0x12, 0x12, 0x12}},
    {"white, frame at 50 %", {{255, 255, 255}, 255}, {0x80, 0x80, 0x80}, {255, 255, 255}, 128},
    {"transparent on #121212, frame at 50 %",
     {{0, 0, 0}, 0},
     {0x80, 0x80, 0x80},
     {0x12, 0x12, 0x12},
     128},
};

// The colour the eye sees behind the figures: the surface over the page, mixed in sRGB code
// space as the renderer does.
rgb8 effective(const host_surface& h) {
    auto ch = [&](std::uint8_t s, std::uint8_t p) {
        return static_cast<std::uint8_t>((h.bg.alpha * s + (255 - h.bg.alpha) * p + 127) / 255);
    };
    return {ch(h.bg.colour.r, h.page.r), ch(h.bg.colour.g, h.page.g), ch(h.bg.colour.b, h.page.b)};
}

double contrast(rgb8 a, rgb8 b) {
    return contrast_ratio(relative_luminance(a), relative_luminance(b));
}

canvas on_page(const canvas& img, rgb8 page) {
    canvas out(img.width() + 12, img.height() + 12, {page.r, page.g, page.b, 255});
    out.draw(img, 6, 6);
    return out;
}

}  // namespace

int run_surfaces(const options& opt) {
    const std::string dir = opt.out_dir + "/surfaces";
    make_dirs(dir);
    const design base = proposed_design();
    const auto fp = sample_fingerprint("hh-lab/markers", 0);
    const cells c = features(fp.data(), base.dirs);
    // Every colour on every figure, as on the palette sheet.
    const std::uint8_t all_bytes[16] = {0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70, 0x78,
                                        0x80, 0xA8, 0xD0, 0xF8, 0x98, 0xB0, 0xC8, 0xE0};
    std::uint8_t all_fp[32] = {};
    std::copy(all_bytes, all_bytes + 16, all_fp);
    const cells all = features(all_fp, base.dirs);

    std::FILE* f = std::fopen((dir + "/surfaces.tsv").c_str(), "w");
    if (f == nullptr) {
        return 1;
    }
    std::fprintf(f, "surface\teffective_background");
    for (const rgb8& p : base.palette) {
        std::fprintf(f, "\tcontrast_%s", to_hex(p).c_str());
    }
    std::fprintf(f, "\tmin_palette_contrast\tframe\tframe_contrast\n");

    std::vector<sheet_row> rows;
    for (const host_surface& h : surfaces) {
        design d = base;
        d.frame_colour = h.frame;
        d.frame_alpha = h.frame_alpha;
        const rgb8 eff = effective(h);
        double worst = 1e9;
        std::string worst_colour;
        std::fprintf(f, "%s\t%s", h.name, to_hex(eff).c_str());
        for (const rgb8& p : base.palette) {
            const double cr = contrast(p, eff);
            std::fprintf(f, "\t%.2f", cr);
            if (cr < worst) {
                worst = cr;
                worst_colour = to_hex(p);
            }
        }
        auto over = [&](std::uint8_t fg, std::uint8_t bgc) {
            return static_cast<std::uint8_t>(
                (h.frame_alpha * fg + (255 - h.frame_alpha) * bgc + 127) / 255);
        };
        const rgb8 frame_seen = {over(h.frame.r, eff.r), over(h.frame.g, eff.g),
                                 over(h.frame.b, eff.b)};
        const double frame_cr = contrast(frame_seen, eff);
        std::fprintf(f, "\t%.2f\t%s\t%.2f\n", worst, to_hex(h.frame).c_str(), frame_cr);

        sheet_row row;
        char caption[200];
        std::snprintf(caption, sizeof(caption), "%s\nfigures >= %.2f:1 (%s)\nframe %s %.2f:1",
                      h.name, worst, worst_colour.c_str(), to_hex(h.frame).c_str(), frame_cr);
        row.caption = caption;
        row.tiles.push_back(
            {on_page(render(all, d, 96, h.bg, frame_style::none), h.page), "all colours"});
        row.tiles.push_back(
            {on_page(render(c, d, 96, h.bg, frame_style::none), h.page), "universal"});
        row.tiles.push_back(
            {on_page(render(c, d, 96, h.bg, frame_style::rounded), h.page), "keyed, rounded"});
        row.tiles.push_back(
            {on_page(render(c, d, 96, h.bg, frame_style::disc), h.page), "round universal"});
        row.tiles.push_back({on_page(render(c, d, 96, h.bg, frame_style::ring_double), h.page),
                             "round keyed, double"});
        row.tiles.push_back(
            {on_page(render(c, d, 48, h.bg, frame_style::rounded), h.page), "48 px keyed"});
        rows.push_back(row);
    }
    std::fclose(f);
    std::printf("wrote %s\n", (dir + "/surfaces.tsv").c_str());
    const bool ok = save_sheet(
        dir + "/surfaces.png",
        row_sheet({"hh on host-chosen surfaces and frame transparency",
                   "the palette is tuned to 3:1 on white and #121212; the caption gives the lowest "
                   "wcag contrast"},
                  rows, 330, 14, light_page()));
    return ok ? 0 : 1;
}

}  // namespace lab
}  // namespace hh
