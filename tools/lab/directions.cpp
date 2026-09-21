// Triangle direction check:
// pairs of images that differ only in one triangle's direction, left-right or
// up-down, at 48, 64 and 96 px. Controls: pairs that differ in figure kind, and
// identical pairs. Each size comes as a labelled sheet and as a shuffled blind
// sheet with an answer key, so the sheets can also be taken as a test before
// the key is read.

#include <algorithm>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "candidate.hpp"
#include "lab.hpp"
#include "sheet.hpp"

namespace hh {
namespace lab {

namespace {

enum class pair_kind {
    left_right,
    up_down,
    kind_change,
    identical
};

const char* kind_name(pair_kind k) {
    switch (k) {
        case pair_kind::left_right:
            return "left-right";
        case pair_kind::up_down:
            return "up-down";
        case pair_kind::kind_change:
            return "figure kind";
        case pair_kind::identical:
            return "identical";
    }
    return "?";
}

struct image_pair {
    pair_kind kind;
    cells a;
    cells b;
    int cell;  // the cell that differs, -1 for identical pairs
};

bool horizontal(figure f) {
    return f == figure::left || f == figure::right;
}

bool vertical(figure f) {
    return f == figure::up || f == figure::down;
}

figure flipped(figure f) {
    switch (f) {
        case figure::left:
            return figure::right;
        case figure::right:
            return figure::left;
        case figure::up:
            return figure::down;
        case figure::down:
            return figure::up;
        default:
            return f;
    }
}

// Builds `count` pairs of one kind from consecutive sample fingerprints.
std::vector<image_pair> make_pairs(pair_kind kind, int count, std::uint64_t& next) {
    std::vector<image_pair> out;
    while (static_cast<int>(out.size()) < count) {
        const auto fp = sample_fingerprint("hh-lab/directions", next++);
        const cells c = features(fp.data(), directions::four);
        std::vector<int> eligible;
        for (int i = 0; i < 16; ++i) {
            const figure f = c[static_cast<std::size_t>(i)].fig;
            const bool ok = (kind == pair_kind::left_right && horizontal(f)) ||
                            (kind == pair_kind::up_down && vertical(f)) ||
                            (kind == pair_kind::kind_change && is_triangle(f)) ||
                            kind == pair_kind::identical;
            if (ok) {
                eligible.push_back(i);
            }
        }
        if (eligible.empty()) {
            continue;
        }
        image_pair p{kind, c, c, -1};
        if (kind != pair_kind::identical) {
            // Byte 31 is not used by the image; it picks the cell.
            p.cell = eligible[fp[31] % eligible.size()];
            cell& target = p.b[static_cast<std::size_t>(p.cell)];
            if (kind == pair_kind::kind_change) {
                target.fig = (fp[30] & 1) ? figure::square : figure::circle;
            } else {
                target.fig = flipped(target.fig);
            }
        }
        out.push_back(p);
    }
    return out;
}

canvas pair_tile(const image_pair& p, const design& d, int size) {
    const int pad = 6;
    const int gap = std::max(8, size / 6);
    canvas out(2 * size + gap + 2 * pad, size + 2 * pad, {0xEC, 0xEC, 0xEC, 255});
    out.draw(render(p.a, d, size, background::white, frame_style::none), pad, pad);
    out.draw(render(p.b, d, size, background::white, frame_style::none), pad + size + gap, pad);
    return out;
}

std::string cell_name(int cell) {
    return cell < 0
               ? "-"
               : "row " + std::to_string(cell / 4 + 1) + " col " + std::to_string(cell % 4 + 1);
}

}  // namespace

int run_directions(const options& opt) {
    const std::string dir = opt.out_dir + "/directions";
    make_dirs(dir);
    const design d = proposed_design();

    std::uint64_t next = 0;
    std::vector<image_pair> pairs;
    for (const auto& [kind, count] :
         std::vector<std::pair<pair_kind, int>>{{pair_kind::left_right, 12},
                                                {pair_kind::up_down, 12},
                                                {pair_kind::kind_change, 6},
                                                {pair_kind::identical, 6}}) {
        const auto made = make_pairs(kind, count, next);
        pairs.insert(pairs.end(), made.begin(), made.end());
    }

    // A fixed Fisher-Yates shuffle for the blind sheets (std::shuffle is not
    // specified exactly, so its order would depend on the standard library).
    std::vector<std::size_t> order(pairs.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::mt19937 rng(14);
    for (std::size_t i = order.size() - 1; i > 0; --i) {
        std::swap(order[i], order[static_cast<std::size_t>(rng()) % (i + 1)]);
    }

    bool ok = true;
    for (int size : {48, 64, 96}) {
        const int per_row = size >= 96 ? 4 : 6;
        // Labelled: one kind per row group.
        std::vector<sheet_row> rows;
        for (std::size_t start = 0; start < pairs.size();) {
            sheet_row row;
            row.caption = kind_name(pairs[start].kind);
            std::size_t i = start;
            while (i < pairs.size() && pairs[i].kind == pairs[start].kind &&
                   static_cast<int>(row.tiles.size()) < per_row) {
                row.tiles.push_back({pair_tile(pairs[i], d, size), cell_name(pairs[i].cell)});
                ++i;
            }
            rows.push_back(row);
            start = i;
        }
        ok = save_sheet(
                 dir + "/directions_" + std::to_string(size) + "px_labelled.png",
                 row_sheet({"hh candidate: pairs that differ in one triangle direction only, " +
                                std::to_string(size) + " px",
                            "left-right: a left triangle becomes right or back; up-down likewise;",
                            "controls: a triangle becomes a square or circle, and identical pairs"},
                           rows, 170, 12, light_page())) &&
             ok;

        // Blind: shuffled and numbered.
        std::vector<tile> tiles;
        for (std::size_t n = 0; n < order.size(); ++n) {
            tiles.push_back({pair_tile(pairs[order[n]], d, size), "pair " + std::to_string(n + 1)});
        }
        const canvas sample = pair_tile(pairs[0], d, size);
        ok = save_sheet(
                 dir + "/directions_" + std::to_string(size) + "px_blind.png",
                 grid_sheet(
                     {"hh candidate, blind test at " + std::to_string(size) +
                          " px: which pairs differ?",
                      "answer key: directions_key.tsv (same pairs and numbering at every size)"},
                     tiles, per_row, sample.width(), sample.height(), 14, light_page())) &&
             ok;
    }

    std::FILE* f = std::fopen((dir + "/directions_key.tsv").c_str(), "w");
    if (f == nullptr) {
        return 1;
    }
    std::fprintf(f, "pair\tkind\tdiffering cell\n");
    for (std::size_t n = 0; n < order.size(); ++n) {
        const image_pair& p = pairs[order[n]];
        std::fprintf(f, "%zu\t%s\t%s\n", n + 1, kind_name(p.kind), cell_name(p.cell).c_str());
    }
    std::fclose(f);
    std::printf("wrote %s\n", (dir + "/directions_key.tsv").c_str());
    return ok ? 0 : 1;
}

}  // namespace lab
}  // namespace hh
