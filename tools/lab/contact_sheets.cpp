// Contact sheets for the look and the figure distribution: 200 random 4x4
// images at 32, 48, 64 and 128 px, on white and
// transparent-on-dark, each also as greyscale and CVD-simulated copies. Plus
// the figure statistics over 10^6 fingerprints.

#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "candidate.hpp"
#include "lab.hpp"
#include "sheet.hpp"

namespace hh {
namespace lab {

namespace {

constexpr int sheet_count = 200;
constexpr int columns = 20;

struct variant {
    const char* name;
    bool grey;
    vision v;
};

constexpr variant variants[] = {
    {"normal", false, vision::normal}, {"grey", true, vision::normal},
    {"protan", false, vision::protan}, {"deutan", false, vision::deutan},
    {"tritan", false, vision::tritan},
};

canvas contact_sheet(const design& d, const char* palette_name, int size, background bg) {
    const page_style style = bg == background::white ? light_page() : dark_page();
    std::vector<tile> tiles;
    for (int i = 0; i < sheet_count; ++i) {
        const auto fp = sample_fingerprint("hh-lab/contact", static_cast<std::uint64_t>(i));
        tiles.push_back({render(features(fp.data(), d.dirs), d, size, bg, frame_style::none), ""});
    }
    const int pad = std::max(12, size / 4);
    char line1[200];
    std::snprintf(line1, sizeof(line1),
                  "hh candidate, contact sheet: %d random fingerprints at %d px", sheet_count,
                  size);
    char line2[200];
    std::snprintf(
        line2, sizeof(line2), "%s, universal (default: no frame), palette %s",
        bg == background::white ? "white background" : "transparent background on #121212",
        palette_name);
    return grid_sheet(
        {line1, line2,
         "fingerprint i = sha-256(\"hh-lab/contact/i\"), row-major from the top left"},
        tiles, columns, size, size, pad, style);
}

void write_statistics(const std::string& path) {
    constexpr int n = 1000000;
    std::array<long, 8> codes{};
    std::array<long, 4> colours{};
    std::array<long, 17> empty_hist{};
    std::array<long, 17> same_colour_max{};
    for (int i = 0; i < n; ++i) {
        const auto fp = sample_fingerprint("hh-lab/stats", static_cast<std::uint64_t>(i));
        int empty = 0;
        std::array<int, 4> per_colour{};
        for (int c = 0; c < 16; ++c) {
            const unsigned code = fp[static_cast<std::size_t>(c)] >> 5;
            ++codes[code];
            if (code < 2) {
                ++empty;
            } else {
                const unsigned colour = (fp[static_cast<std::size_t>(c)] >> 3) & 3u;
                ++colours[colour];
                ++per_colour[colour];
            }
        }
        ++empty_hist[static_cast<std::size_t>(empty)];
        int mx = 0;
        for (int v : per_colour) {
            mx = std::max(mx, v);
        }
        ++same_colour_max[static_cast<std::size_t>(mx)];
    }
    std::FILE* f = std::fopen(path.c_str(), "w");
    if (f == nullptr) {
        return;
    }
    const double cells = 16.0 * n;
    static const char* code_names[8] = {"none (0)", "none (1)", "square", "circle",
                                        "up",       "right",    "down",   "left"};
    std::fprintf(
        f,
        "# Figure statistics over %d fingerprints (sha-256 of hh-lab/stats/i), four directions\n",
        n);
    std::fprintf(f, "item\tobserved\texpected\n");
    for (int c = 0; c < 8; ++c) {
        std::fprintf(f, "figure code %s\t%.5f\t%.5f\n", code_names[c],
                     static_cast<double>(codes[static_cast<std::size_t>(c)]) / cells, 0.125);
    }
    long filled = 0;
    for (long v : colours) {
        filled += v;
    }
    for (int c = 0; c < 4; ++c) {
        std::fprintf(
            f, "colour %d among filled cells\t%.5f\t%.5f\n", c,
            static_cast<double>(colours[static_cast<std::size_t>(c)]) / static_cast<double>(filled),
            0.25);
    }
    for (int k = 0; k <= 16; ++k) {
        const double p = std::tgamma(17.0) / (std::tgamma(k + 1.0) * std::tgamma(17.0 - k)) *
                         std::pow(0.25, k) * std::pow(0.75, 16 - k);
        std::fprintf(f, "images with %d empty cells\t%.6f\t%.6f\n", k,
                     static_cast<double>(empty_hist[static_cast<std::size_t>(k)]) / n, p);
    }
    for (int k = 0; k <= 16; ++k) {
        if (same_colour_max[static_cast<std::size_t>(k)] != 0) {
            std::fprintf(f, "images whose most frequent colour covers %d cells\t%.6f\t\n", k,
                         static_cast<double>(same_colour_max[static_cast<std::size_t>(k)]) / n);
        }
    }
    std::fclose(f);
    std::printf("wrote %s\n", path.c_str());
}

}  // namespace

int run_contact_sheets(const options& opt) {
    const std::string dir = opt.out_dir + "/contact";
    make_dirs(dir);
    const design d = proposed_design();
    bool ok = true;
    const int sizes[] = {32, 48, 64, 128};
    for (int size : sizes) {
        for (int b = 0; b < 2; ++b) {
            const background bg = b == 0 ? background::white : background::transparent;
            const canvas base = contact_sheet(d, "final palette", size, bg);
            for (const variant& v : variants) {
                canvas c = base;
                if (v.grey) {
                    c.transform(greyscale_transform());
                } else if (v.v != vision::normal) {
                    c.transform(vision_transform(v.v));
                }
                const std::string name = dir + "/contact_" + std::to_string(size) + "px_" +
                                         (b == 0 ? "white" : "dark") + "_" + v.name + ".png";
                ok = save_sheet(name, c) && ok;
            }
            if (opt.quick) {
                break;
            }
        }
    }
    // The initial palette on the same fingerprints, for comparison.
    ok = save_sheet(dir + "/contact_48px_white_normal_initial-palette.png",
                    contact_sheet(initial_design(), "initial palette", 48, background::white)) &&
         ok;
    // The first 40 images at 32 px magnified four times, to inspect the pixels.
    {
        std::vector<tile> tiles;
        for (int i = 0; i < 40; ++i) {
            const auto fp = sample_fingerprint("hh-lab/contact", static_cast<std::uint64_t>(i));
            tiles.push_back(
                {render(features(fp.data(), d.dirs), d, 32, background::white, frame_style::none)
                     .magnified(4),
                 ""});
        }
        ok = save_sheet(
                 dir + "/contact_32px_white_normal_x4.png",
                 grid_sheet(
                     {"hh candidate: the first 40 contact-sheet images at 32 px, magnified 4x"},
                     tiles, 10, 128, 128, 16, light_page())) &&
             ok;
    }
    write_statistics(dir + "/figure_statistics.tsv");
    return ok ? 0 : 1;
}

}  // namespace lab
}  // namespace hh
