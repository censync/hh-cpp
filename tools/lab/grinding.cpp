// Lookalike grinding: for several targets,
// grind 2^24 candidates with stretching disabled and show the nearest images,
// to see how close a cheap attacker gets.
//
// hh is searched salience-ordered, as real attackers match the big picture
// first (Tan et al. 2017): the key is lexicographic over (cells whose
// emptiness differs, filled cells whose colour differs, filled cells whose
// figure family differs, triangles whose direction differs).
//
// For a common yardstick every scheme is also searched by a pixel "gist"
// distance: the image on a 24 x 24 grid, compared pixel by pixel in OKLab
// (mean Euclidean distance x 100 over the icon area). hh is taken on an
// idealised grid of 6 x 6 pixel cells without margins; Blockies on its 8 x 8
// blocks; Jazzicon inside its circle.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "candidate.hpp"
#include "lab.hpp"
#include "prior_art.hpp"
#include "sha256.hpp"
#include "sheet.hpp"

namespace hh {
namespace lab {

namespace {

constexpr int target_count = 6;
constexpr int keep = 5;
constexpr int gist = 24;

struct hit {
    double score;
    std::uint64_t index;
};

// Keeps the `keep` lowest scores.
struct best_list {
    std::vector<hit> hits;

    void offer(double score, std::uint64_t index) {
        if (hits.size() == keep && score >= hits.back().score) {
            return;
        }
        hits.push_back({score, index});
        std::sort(hits.begin(), hits.end(), [](const hit& a, const hit& b) {
            return a.score < b.score || (a.score == b.score && a.index < b.index);
        });
        if (hits.size() > keep) {
            hits.pop_back();
        }
    }

    void merge(const best_list& other) {
        for (const hit& h : other.hits) {
            offer(h.score, h.index);
        }
    }
};

// Candidate j: SHA-256 of "hh-lab/grind" || scheme byte || u64be(j).
fingerprint_bytes candidate_bytes(std::uint8_t scheme, std::uint64_t j) {
    std::uint8_t msg[21] = {'h', 'h', '-', 'l', 'a', 'b', '/', 'g', 'r', 'i', 'n', 'd', scheme};
    for (int i = 0; i < 8; ++i) {
        msg[13 + i] = static_cast<std::uint8_t>(j >> (56 - 8 * i));
    }
    const auto d = hh::detail::sha256_hash(msg, sizeof(msg));
    fingerprint_bytes out{};
    std::copy(d.begin(), d.end(), out.begin());
    return out;
}

std::string address_hex(const fingerprint_bytes& b) {
    static const char digits[] = "0123456789abcdef";
    std::string s = "0x";
    for (int i = 0; i < 20; ++i) {
        s.push_back(digits[b[static_cast<std::size_t>(i)] >> 4]);
        s.push_back(digits[b[static_cast<std::size_t>(i)] & 15]);
    }
    return s;
}

std::uint32_t jazzicon_seed(const fingerprint_bytes& b) {
    return (std::uint32_t(b[0]) << 24) | (std::uint32_t(b[1]) << 16) | (std::uint32_t(b[2]) << 8) |
           b[3];
}

// Runs `count` candidates on the thread pool. `fn(j, locals)` scores candidate j
// into per-thread best lists; the lists are merged at the end.
template <typename Fn>
std::vector<best_list> grind(std::uint64_t count, unsigned threads, std::size_t lists, Fn fn) {
    std::vector<std::vector<best_list>> per_thread(threads, std::vector<best_list>(lists));
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < threads; ++t) {
        pool.emplace_back([&, t] {
            for (std::uint64_t j = t; j < count; j += threads) {
                fn(j, per_thread[t]);
            }
        });
    }
    for (auto& th : pool) {
        th.join();
    }
    std::vector<best_list> out(lists);
    for (const auto& v : per_thread) {
        for (std::size_t i = 0; i < lists; ++i) {
            out[i].merge(v[i]);
        }
    }
    return out;
}

// ---- hh --------------------------------------------------------------------

struct salience {
    int empty;
    int colour;
    int family;
    int direction;
};

salience compare_cells(const cells& target, const cells& cand) {
    salience s{0, 0, 0, 0};
    for (std::size_t i = 0; i < 16; ++i) {
        const bool te = target[i].fig == figure::none;
        const bool ce = cand[i].fig == figure::none;
        if (te != ce) {
            ++s.empty;
            continue;
        }
        if (te) {
            continue;
        }
        if (target[i].colour != cand[i].colour) {
            ++s.colour;
        }
        if (figure_family(target[i].fig) != figure_family(cand[i].fig)) {
            ++s.family;
        } else if (is_triangle(target[i].fig) && target[i].fig != cand[i].fig) {
            ++s.direction;
        }
    }
    return s;
}

double salience_score(const salience& s) {
    return ((s.empty * 32.0 + s.colour) * 32.0 + s.family) * 32.0 + s.direction;
}

salience salience_of(double score) {
    int v = static_cast<int>(score);
    salience s{};
    s.direction = v % 32;
    v /= 32;
    s.family = v % 32;
    v /= 32;
    s.colour = v % 32;
    s.empty = v / 32;
    return s;
}

int cell_state(const cell& c) {
    return c.fig == figure::none ? 0 : 1 + (static_cast<int>(c.fig) - 1) * 4 + c.colour;
}

// OKLab distance x 100, summed over the 6 x 6 pixels of one cell, for every
// pair of the 25 cell states.
using state_table = std::array<std::array<double, 25>, 25>;

state_table make_state_table(const design& d) {
    std::array<std::array<vec3, 36>, 25> pixels{};
    for (int s = 0; s < 25; ++s) {
        cells one{};
        if (s != 0) {
            one[0].fig = static_cast<figure>(1 + (s - 1) / 4);
            one[0].colour = static_cast<std::uint8_t>((s - 1) % 4);
        }
        // A 64 px image that holds only cell 0, sampled inside that cell on a 6 x 6 grid.
        const canvas img = render(one, d, 64, background::white, frame_style::single);
        const geometry g = make_geometry(64, frame_style::none);
        for (int y = 0; y < 6; ++y) {
            for (int x = 0; x < 6; ++x) {
                const int px = g.offset + (2 * x + 1) * g.cell / 12;
                const int py = g.offset + (2 * y + 1) * g.cell / 12;
                const rgba8 p = img.get(px, py);
                pixels[static_cast<std::size_t>(s)][static_cast<std::size_t>(y * 6 + x)] =
                    oklab_from_linear(to_linear({p.r, p.g, p.b}));
            }
        }
    }
    state_table t{};
    for (std::size_t a = 0; a < 25; ++a) {
        for (std::size_t b = 0; b < 25; ++b) {
            double sum = 0;
            for (std::size_t k = 0; k < 36; ++k) {
                sum += 100.0 * distance(pixels[a][k], pixels[b][k]);
            }
            t[a][b] = sum;
        }
    }
    return t;
}

// ---- Blockies and Jazzicon gist -------------------------------------------

struct gist_image {
    std::array<std::uint8_t, gist * gist> index;  // colour index per pixel
    std::array<vec3, 5> colours;                  // OKLab
    int n_colours;
    std::array<bool, gist * gist> mask;  // pixels that count
    int mask_count;
};

gist_image blockie_gist(const blockie& b) {
    gist_image g{};
    g.colours[0] = oklab_from_linear(to_linear(b.background));
    g.colours[1] = oklab_from_linear(to_linear(b.colour));
    g.colours[2] = oklab_from_linear(to_linear(b.spot));
    g.n_colours = 3;
    for (int y = 0; y < gist; ++y) {
        for (int x = 0; x < gist; ++x) {
            const std::uint8_t v =
                b.data[static_cast<std::size_t>((y * 8 / gist) * 8 + x * 8 / gist)];
            g.index[static_cast<std::size_t>(y * gist + x)] = v == 0 ? 0 : (v == 1 ? 1 : 2);
            g.mask[static_cast<std::size_t>(y * gist + x)] = true;
        }
    }
    g.mask_count = gist * gist;
    return g;
}

gist_image jazzicon_gist(const jazzicon& j) {
    gist_image g{};
    const rgb8 cs[4] = {j.background, j.shapes[0].colour, j.shapes[1].colour, j.shapes[2].colour};
    for (int i = 0; i < 4; ++i) {
        g.colours[static_cast<std::size_t>(i)] = oklab_from_linear(to_linear(cs[i]));
    }
    g.n_colours = 4;
    g.mask_count = 0;
    double cos_a[3];
    double sin_a[3];
    for (int s = 0; s < 3; ++s) {
        const double a =
            j.shapes[static_cast<std::size_t>(s)].rotation_degrees * 3.14159265358979323846 / 180.0;
        cos_a[s] = std::cos(a);
        sin_a[s] = std::sin(a);
    }
    for (int y = 0; y < gist; ++y) {
        for (int x = 0; x < gist; ++x) {
            const double px = (x + 0.5) / gist;
            const double py = (y + 0.5) / gist;
            const double dx = px - 0.5;
            const double dy = py - 0.5;
            const std::size_t k = static_cast<std::size_t>(y * gist + x);
            g.mask[k] = dx * dx + dy * dy <= 0.25;
            g.mask_count += g.mask[k] ? 1 : 0;
            std::uint8_t idx = 0;
            for (int s = 0; s < 3; ++s) {
                const jazzicon_shape& sh = j.shapes[static_cast<std::size_t>(s)];
                const double qx = px - sh.tx - 0.5;
                const double qy = py - sh.ty - 0.5;
                const double ux = cos_a[s] * qx + sin_a[s] * qy + 0.5;
                const double uy = -sin_a[s] * qx + cos_a[s] * qy + 0.5;
                if (ux >= 0.0 && ux <= 1.0 && uy >= 0.0 && uy <= 1.0) {
                    idx = static_cast<std::uint8_t>(s + 1);
                }
            }
            g.index[k] = idx;
        }
    }
    return g;
}

double gist_distance(const gist_image& target, const gist_image& cand) {
    double table[5][5];
    for (int a = 0; a < target.n_colours; ++a) {
        for (int b = 0; b < cand.n_colours; ++b) {
            table[a][b] = 100.0 * distance(target.colours[static_cast<std::size_t>(a)],
                                           cand.colours[static_cast<std::size_t>(b)]);
        }
    }
    double sum = 0;
    for (std::size_t k = 0; k < target.index.size(); ++k) {
        if (target.mask[k]) {
            sum += table[target.index[k]][cand.index[k]];
        }
    }
    return sum / target.mask_count;
}

// ---- sheets ----------------------------------------------------------------

canvas on_white(const canvas& img) {
    canvas out(img.width(), img.height(), {255, 255, 255, 255});
    out.draw(img, 0, 0);
    return out;
}

void write_sheet(const std::string& path, const std::vector<std::string>& title,
                 const std::vector<sheet_row>& rows) {
    save_sheet(path, row_sheet(title, rows, 150, 12, light_page()));
}

}  // namespace

int run_grinding(const options& opt) {
    const std::string dir = opt.out_dir + "/grinding";
    make_dirs(dir);
    const unsigned threads = thread_count(opt);
    const std::uint64_t count = opt.quick ? (1u << 18) : (1u << 24);
    const design d = proposed_design();
    const int show = 96;

    std::FILE* tsv = std::fopen((dir + "/grinding.tsv").c_str(), "w");
    if (tsv == nullptr) {
        return 1;
    }
    std::fprintf(
        tsv, "# %llu candidates per scheme; gist = mean OKLab distance x 100 on a 24 x 24 grid\n",
        static_cast<unsigned long long>(count));
    std::fprintf(tsv, "scheme\ttarget\trank\tscore\tdetail\n");

    // hh targets.
    std::vector<cells> targets;
    for (int t = 0; t < target_count; ++t) {
        const auto fp = sample_fingerprint("hh-lab/grind-target", static_cast<std::uint64_t>(t));
        targets.push_back(features(fp.data(), d.dirs));
    }
    const state_table table = make_state_table(d);

    // hh, salience-ordered and gist-ordered in one pass. Lists 0..5 salience, 6..11 gist;
    // counters of how many candidates match the empty pattern (and more) come from a second array.
    std::vector<std::array<std::uint64_t, 4>> stage_counts(target_count, {0, 0, 0, 0});
    std::mutex count_mutex;
    const auto hh_best = grind(
        count, threads, 2 * target_count, [&](std::uint64_t j, std::vector<best_list>& lists) {
            const auto fp = candidate_bytes('h', j);
            const cells c = features(fp.data(), d.dirs);
            std::array<int, 16> states{};
            for (std::size_t i = 0; i < 16; ++i) {
                states[i] = cell_state(c[i]);
            }
            for (std::size_t t = 0; t < targets.size(); ++t) {
                const salience s = compare_cells(targets[t], c);
                lists[t].offer(salience_score(s), j);
                double g = 0;
                for (std::size_t i = 0; i < 16; ++i) {
                    g += table[static_cast<std::size_t>(cell_state(targets[t][i]))]
                              [static_cast<std::size_t>(states[i])];
                }
                lists[target_count + t].offer(g / (16.0 * 36.0), j);
                if (s.empty == 0) {
                    std::lock_guard<std::mutex> lock(count_mutex);
                    ++stage_counts[t][0];
                    if (s.colour == 0) {
                        ++stage_counts[t][1];
                        if (s.family == 0) {
                            ++stage_counts[t][2];
                            if (s.direction == 0) {
                                ++stage_counts[t][3];
                            }
                        }
                    }
                }
            }
        });

    auto hh_tile = [&](const cells& c) {
        return render(c, d, show, background::white, frame_style::none);
    };
    {
        std::vector<sheet_row> rows;
        for (int t = 0; t < target_count; ++t) {
            sheet_row row;
            row.caption = "target " + std::to_string(t + 1);
            int empty = 0;
            for (const cell& cl : targets[static_cast<std::size_t>(t)]) {
                empty += cl.fig == figure::none ? 1 : 0;
            }
            row.tiles.push_back({hh_tile(targets[static_cast<std::size_t>(t)]),
                                 "target, " + std::to_string(empty) + " empty"});
            int rank = 0;
            for (const hit& h : hh_best[static_cast<std::size_t>(t)].hits) {
                const auto fp = candidate_bytes('h', h.index);
                const salience s = salience_of(h.score);
                char label[64];
                std::snprintf(label, sizeof(label), "e%d c%d f%d d%d", s.empty, s.colour, s.family,
                              s.direction);
                row.tiles.push_back({hh_tile(features(fp.data(), d.dirs)), label});
                std::fprintf(
                    tsv, "hh salience\t%d\t%d\t%s\tempty-pattern matches %llu, +colours %llu\n",
                    t + 1, ++rank, label,
                    static_cast<unsigned long long>(stage_counts[static_cast<std::size_t>(t)][0]),
                    static_cast<unsigned long long>(stage_counts[static_cast<std::size_t>(t)][1]));
            }
            rows.push_back(row);
        }
        write_sheet(dir + "/grind_hh_salience.png",
                    {"hh candidate: nearest of " + std::to_string(count) +
                         " candidates per target, salience-ordered",
                     "e = cells whose emptiness differs, c = colour, f = figure family, d = "
                     "triangle direction",
                     "ordered by e, then c, then f, then d; no stretching; 96 px"},
                    rows);
    }
    {
        std::vector<sheet_row> rows;
        for (int t = 0; t < target_count; ++t) {
            sheet_row row;
            row.caption = "target " + std::to_string(t + 1);
            row.tiles.push_back({hh_tile(targets[static_cast<std::size_t>(t)]), "target"});
            int rank = 0;
            for (const hit& h : hh_best[static_cast<std::size_t>(target_count + t)].hits) {
                const auto fp = candidate_bytes('h', h.index);
                const cells c = features(fp.data(), d.dirs);
                const salience s = compare_cells(targets[static_cast<std::size_t>(t)], c);
                char label[64];
                std::snprintf(label, sizeof(label), "gist %.1f", h.score);
                row.tiles.push_back({hh_tile(c), label});
                std::fprintf(tsv, "hh gist\t%d\t%d\t%.2f\te%d c%d f%d d%d\n", t + 1, ++rank,
                             h.score, s.empty, s.colour, s.family, s.direction);
            }
            rows.push_back(row);
        }
        write_sheet(dir + "/grind_hh_gist.png",
                    {"hh candidate: nearest of " + std::to_string(count) +
                         " candidates per target by pixel gist",
                     "gist = mean oklab distance x 100 on a 24 x 24 grid (6 x 6 px per cell); no "
                     "stretching; 96 px"},
                    rows);
    }

    // Blockies-style.
    {
        std::vector<blockie> bt;
        std::vector<gist_image> gt;
        for (int t = 0; t < target_count; ++t) {
            const auto b =
                sample_fingerprint("hh-lab/grind-target-address", static_cast<std::uint64_t>(t));
            bt.push_back(make_blockie(address_hex(b)));
            gt.push_back(blockie_gist(bt.back()));
        }
        const auto best = grind(
            count, threads, target_count, [&](std::uint64_t j, std::vector<best_list>& lists) {
                const gist_image g =
                    blockie_gist(make_blockie(address_hex(candidate_bytes('b', j))));
                for (std::size_t t = 0; t < gt.size(); ++t) {
                    lists[t].offer(gist_distance(gt[t], g), j);
                }
            });
        std::vector<sheet_row> rows;
        for (int t = 0; t < target_count; ++t) {
            sheet_row row;
            row.caption = "target " + std::to_string(t + 1);
            row.tiles.push_back({render_blockie(bt[static_cast<std::size_t>(t)], show), "target"});
            int rank = 0;
            for (const hit& h : best[static_cast<std::size_t>(t)].hits) {
                char label[64];
                std::snprintf(label, sizeof(label), "gist %.1f", h.score);
                row.tiles.push_back(
                    {render_blockie(make_blockie(address_hex(candidate_bytes('b', h.index))), show),
                     label});
                std::fprintf(tsv, "blockies gist\t%d\t%d\t%.2f\t\n", t + 1, ++rank, h.score);
            }
            rows.push_back(row);
        }
        write_sheet(dir + "/grind_blockies.png",
                    {"blockies style: nearest of " + std::to_string(count) +
                         " candidates per target by pixel gist",
                     "seed = lowercase 0x address string, as usually seeded; gist on the 8 x 8 blocks at "
                     "24 x 24; 96 px"},
                    rows);
    }

    // Jazzicon-style.
    {
        std::vector<jazzicon> jt;
        std::vector<gist_image> gt;
        for (int t = 0; t < target_count; ++t) {
            const auto b =
                sample_fingerprint("hh-lab/grind-target-address", static_cast<std::uint64_t>(t));
            jt.push_back(make_jazzicon(jazzicon_seed(b)));
            gt.push_back(jazzicon_gist(jt.back()));
        }
        const auto best = grind(
            count, threads, target_count, [&](std::uint64_t j, std::vector<best_list>& lists) {
                const gist_image g =
                    jazzicon_gist(make_jazzicon(jazzicon_seed(candidate_bytes('j', j))));
                for (std::size_t t = 0; t < gt.size(); ++t) {
                    lists[t].offer(gist_distance(gt[t], g), j);
                }
            });
        std::vector<sheet_row> rows;
        for (int t = 0; t < target_count; ++t) {
            sheet_row row;
            row.caption = "target " + std::to_string(t + 1);
            row.tiles.push_back(
                {on_white(render_jazzicon(jt[static_cast<std::size_t>(t)], show)), "target"});
            int rank = 0;
            for (const hit& h : best[static_cast<std::size_t>(t)].hits) {
                char label[64];
                std::snprintf(label, sizeof(label), "gist %.1f", h.score);
                row.tiles.push_back(
                    {on_white(render_jazzicon(
                         make_jazzicon(jazzicon_seed(candidate_bytes('j', h.index))), show)),
                     label});
                std::fprintf(tsv, "jazzicon gist\t%d\t%d\t%.2f\t\n", t + 1, ++rank, h.score);
            }
            rows.push_back(row);
        }
        write_sheet(dir + "/grind_jazzicon.png",
                    {"jazzicon style: nearest of " + std::to_string(count) +
                         " candidates per target by pixel gist",
                     "seed = first 32 bits of the address, as usually seeded; gist inside the circle at "
                     "24 x 24; 96 px"},
                    rows);
    }
    std::fclose(tsv);
    std::printf("wrote %s\n", (dir + "/grinding.tsv").c_str());
    return 0;
}

}  // namespace lab
}  // namespace hh
