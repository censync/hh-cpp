// Keyed-mode marker candidates at 32, 48 and 128 px on white and transparent-on-dark, plus list
// rows that alternate universal and keyed images the way a host application's
// list would show them. Round images (the grid inscribed in a circle) get their own candidates,
// list rows, a contact sheet and a table of cell sizes against the square form.

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

// Universal images have no frame by default (a plain frame is an option); keyed images carry
// one of the marker styles.
constexpr frame_style styles[] = {
    frame_style::none,        frame_style::single, frame_style::rounded,  frame_style::octagon,
    frame_style::double_line, frame_style::thick,  frame_style::brackets,
};

constexpr frame_style round_styles[] = {
    frame_style::disc,       frame_style::ring,       frame_style::ring_double,
    frame_style::ring_thick, frame_style::ring_ticks, frame_style::ring_gapped,
};

bool is_universal(frame_style s) {
    return s == frame_style::none || s == frame_style::single || s == frame_style::disc ||
           s == frame_style::ring;
}

// A white-background image on a mid-grey host surface shows the outline.
canvas on_grey(const canvas& img, int pad) {
    canvas out(img.width() + 2 * pad, img.height() + 2 * pad, {0x9A, 0xA0, 0xA6, 255});
    out.draw(img, pad, pad);
    return out;
}

// The image on a patch of the host surface, so dark-background renders sit on #121212.
canvas on_surface(const canvas& img, background bg, int pad) {
    canvas out(img.width() + 2 * pad, img.height() + 2 * pad,
               bg == background::white ? rgba8{255, 255, 255, 255} : rgba8{0x12, 0x12, 0x12, 255});
    out.draw(img, pad, pad);
    return out;
}

std::string caption_of(frame_style s) {
    switch (s) {
        case frame_style::none:
            return "universal (default): no frame";
        case frame_style::single:
            return "universal, frame on (option)";
        case frame_style::disc:
            return "universal (default): no ring";
        case frame_style::ring:
            return "universal, ring on (option)";
        default:
            return std::string("keyed: ") + frame_name(s);
    }
}

}  // namespace

int run_markers(const options& opt) {
    const std::string dir = opt.out_dir + "/markers";
    make_dirs(dir);
    const design d = proposed_design();
    const auto fp = sample_fingerprint("hh-lab/markers", 0);
    const cells c = features(fp.data(), d.dirs);
    bool ok = true;

    // One fingerprint in every candidate style and size.
    {
        std::vector<sheet_row> rows;
        for (frame_style s : styles) {
            sheet_row row;
            row.caption = caption_of(s);
            for (int b = 0; b < 2; ++b) {
                const background bg = b == 0 ? background::white : background::transparent;
                for (int size : {32, 48, 128}) {
                    row.tiles.push_back(
                        {on_surface(render(c, d, size, bg, s), bg, 6),
                         std::to_string(size) + (b == 0 ? " px white" : " px dark")});
                }
            }
            row.tiles.push_back(
                {on_grey(render(c, d, 48, background::white, s), 6), "48 px white on grey"});
            row.tiles.push_back({render(c, d, 32, background::white, s).magnified(4), "32 px x4"});
            rows.push_back(row);
        }
        ok = save_sheet(
                 dir + "/markers_sizes.png",
                 row_sheet({"hh mode marker candidates: the same fingerprint in every frame style",
                            "the frame is #808080; geometry: line w = max(1, S/48), margin >= 4w"},
                           rows, 340, 14, light_page())) &&
             ok;
    }

    // List rows: universal and keyed alternate with different fingerprints, as in an address list.
    for (int b = 0; b < 2; ++b) {
        const background bg = b == 0 ? background::white : background::transparent;
        for (int size : {32, 48}) {
            std::vector<sheet_row> rows;
            for (frame_style s : styles) {
                if (is_universal(s)) {
                    continue;
                }
                sheet_row row;
                row.caption = std::string("u / ") + frame_name(s);
                for (int i = 0; i < 10; ++i) {
                    const auto f =
                        sample_fingerprint("hh-lab/markers-list", static_cast<std::uint64_t>(i));
                    const frame_style fs = (i % 2 == 0) ? frame_style::none : s;
                    row.tiles.push_back(
                        {on_surface(render(features(f.data(), d.dirs), d, size, bg, fs), bg, 4),
                         i % 2 == 0 ? "u" : "k"});
                }
                rows.push_back(row);
            }
            const std::string name = dir + "/markers_list_" + std::to_string(size) + "px_" +
                                     (b == 0 ? "white" : "dark") + ".png";
            ok =
                save_sheet(
                    name,
                    row_sheet({"hh mode marker candidates in a list at " + std::to_string(size) +
                                   " px: universal (u) and keyed (k) alternate",
                               "each image has a different fingerprint, as two modes of one address"
                               " never share a picture"},
                              rows, 300, 10, b == 0 ? light_page() : dark_page())) &&
                ok;
        }
    }
    // Round images.
    {
        std::vector<sheet_row> rows;
        for (frame_style s : round_styles) {
            const design& dz = d;
            sheet_row row;
            row.caption = caption_of(s);
            for (int b = 0; b < 2; ++b) {
                const background bg = b == 0 ? background::white : background::transparent;
                for (int size : {32, 48, 64, 128}) {
                    row.tiles.push_back(
                        {on_surface(render(c, dz, size, bg, s), bg, 6),
                         std::to_string(size) + (b == 0 ? " px white" : " px dark")});
                }
            }
            row.tiles.push_back(
                {on_grey(render(c, dz, 64, background::white, s), 6), "64 px white on grey"});
            rows.push_back(row);
        }
        ok = save_sheet(
                 dir + "/markers_round_sizes.png",
                 row_sheet(
                     {"hh round images: the 4 x 4 grid inscribed in a circle, no cell clipped",
                      "the same fingerprint in every ring style; outside the disc is transparent"},
                     rows, 300, 14, light_page())) &&
             ok;
    }
    for (int b = 0; b < 2; ++b) {
        const background bg = b == 0 ? background::white : background::transparent;
        for (int size : {48, 64}) {
            std::vector<sheet_row> rows;
            // The round default: keyed images have no marker either, so both modes look alike.
            {
                sheet_row row;
                row.caption = "u / k default: no marker";
                for (int i = 0; i < 10; ++i) {
                    const auto f =
                        sample_fingerprint("hh-lab/markers-list", static_cast<std::uint64_t>(i));
                    row.tiles.push_back({on_surface(render(features(f.data(), d.dirs), d, size, bg,
                                                           frame_style::disc),
                                                    bg, 4),
                                         i % 2 == 0 ? "u" : "k"});
                }
                rows.push_back(row);
            }
            for (frame_style s : round_styles) {
                if (is_universal(s)) {
                    continue;
                }
                sheet_row row;
                row.caption = std::string("u / ") + frame_name(s);
                for (int i = 0; i < 10; ++i) {
                    const auto f =
                        sample_fingerprint("hh-lab/markers-list", static_cast<std::uint64_t>(i));
                    const frame_style fs = (i % 2 == 0) ? frame_style::disc : s;
                    row.tiles.push_back(
                        {on_surface(render(features(f.data(), d.dirs), d, size, bg, fs), bg, 4),
                         i % 2 == 0 ? "u" : "k"});
                }
                rows.push_back(row);
            }
            const std::string name = dir + "/markers_round_list_" + std::to_string(size) + "px_" +
                                     (b == 0 ? "white" : "dark") + ".png";
            ok = save_sheet(name,
                            row_sheet({"hh round mode marker candidates in a list at " +
                                       std::to_string(size) + " px: universal (u) and keyed (k)"},
                                      rows, 280, 10, b == 0 ? light_page() : dark_page())) &&
                 ok;
        }
    }
    for (int b = 0; b < 2; ++b) {
        const background bg = b == 0 ? background::white : background::transparent;
        for (int size : {64, 96}) {
            std::vector<tile> tiles;
            for (int i = 0; i < 100; ++i) {
                const auto f = sample_fingerprint("hh-lab/contact", static_cast<std::uint64_t>(i));
                tiles.push_back(
                    {render(features(f.data(), d.dirs), d, size, bg, frame_style::disc), ""});
            }
            const std::string name = dir + "/round_contact_" + std::to_string(size) + "px_" +
                                     (b == 0 ? "white" : "dark") + ".png";
            ok = save_sheet(name, grid_sheet({"hh round images without a ring (margin w + g), "
                                              "the first 100 contact-sheet "
                                              "fingerprints at " +
                                              std::to_string(size) + " px"},
                                             tiles, 10, size, size, std::max(12, size / 4),
                                             b == 0 ? light_page() : dark_page())) &&
                 ok;
        }
    }
    // Cell sizes of the square and the round form.
    std::FILE* f = std::fopen((dir + "/round_geometry.tsv").c_str(), "w");
    if (f != nullptr) {
        std::fprintf(f, "size_px\tsquare_cell_px\tround_cell_px\tround_double_or_thick_cell_px\n");
        for (int size : {16, 32, 48, 64, 96, 128, 192, 256, 512, 1024}) {
            std::fprintf(f, "%d\t%d\t%d\t%d\n", size, make_geometry(size, frame_style::none).cell,
                         make_geometry(size, frame_style::disc).cell,
                         make_geometry(size, frame_style::ring_thick).cell);
        }
        std::fclose(f);
        std::printf("wrote %s\n", (dir + "/round_geometry.tsv").c_str());
    }
    return ok ? 0 : 1;
}

}  // namespace lab
}  // namespace hh
