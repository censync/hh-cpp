// Palette gate and search, following Petroff 2021: minimum pairwise CAM02-UCS distance over normal vision, Machado 2009
// protan and deutan simulation at severities 0.1 .. 1.0 and Brettel 1997 tritan
// simulation, with CIEDE2000 and OKLab reported alongside. The hh gate for four
// colours: minimum CAM02-UCS distance of at least 20 under every condition, and
// WCAG contrast of at least 3:1 against white and against #121212.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "candidate.hpp"
#include "colour.hpp"
#include "lab.hpp"
#include "sheet.hpp"

namespace hh {
namespace lab {

namespace {

using palette4 = std::array<rgb8, 4>;

constexpr double gate_distance = 20.0;
constexpr double gate_contrast = 3.0;
const rgb8 white{255, 255, 255};
const rgb8 dark{0x12, 0x12, 0x12};
const rgb8 frame_grey{0x80, 0x80, 0x80};

struct condition {
    std::string name;
    cvd type;
    int severity;  // tenths; -1 means Brettel at full severity
};

std::vector<condition> conditions() {
    std::vector<condition> out;
    out.push_back({"normal", cvd::none, 0});
    for (int s = 1; s <= 10; ++s) {
        out.push_back({"protan " + std::to_string(s * 10) + "%", cvd::protan, s});
    }
    for (int s = 1; s <= 10; ++s) {
        out.push_back({"deutan " + std::to_string(s * 10) + "%", cvd::deutan, s});
    }
    out.push_back({"tritan (brettel)", cvd::tritan, -1});
    return out;
}

vec3 apply(const condition& c, vec3 lin) {
    if (c.type == cvd::none) {
        return lin;
    }
    if (c.severity < 0) {
        return simulate_brettel(lin, c.type);
    }
    return simulate_machado(lin, c.type, c.severity);
}

double min_contrast(const palette4& p, rgb8 bg) {
    double m = 1e9;
    for (const rgb8& c : p) {
        m = std::min(m, contrast_ratio(relative_luminance(c), relative_luminance(bg)));
    }
    return m;
}

bool allowed(rgb8 c) {
    const double y = relative_luminance(c);
    return contrast_ratio(y, relative_luminance(white)) >= gate_contrast &&
           contrast_ratio(y, relative_luminance(dark)) >= gate_contrast;
}

// Per-colour CAM02-UCS coordinates under every condition, so the search only
// recomputes the colour it changed.
struct colour_views {
    std::vector<vec3> ucs;
};

colour_views views_of(rgb8 c, const std::vector<condition>& conds) {
    colour_views v;
    const vec3 lin = to_linear(c);
    for (const auto& cond : conds) {
        v.ucs.push_back(cam02ucs_from_linear(apply(cond, lin)));
    }
    return v;
}

// The minimum pairwise distance over all conditions.
double objective(const std::array<colour_views, 4>& v, std::size_t n_conds) {
    double worst = 1e9;
    for (std::size_t k = 0; k < n_conds; ++k) {
        for (std::size_t i = 0; i < 4; ++i) {
            for (std::size_t j = i + 1; j < 4; ++j) {
                worst = std::min(worst, distance(v[i].ucs[k], v[j].ucs[k]));
            }
        }
    }
    return worst;
}

// Frame-aware searches also require every colour to be at least the gate
// distance from the frame grey under normal vision, so no colour reads as a
// dull grey next to the neutral frame.
bool chromatic(rgb8 c) {
    static const vec3 grey = cam02ucs_from_linear(to_linear(frame_grey));
    return distance(cam02ucs_from_linear(to_linear(c)), grey) >= gate_distance;
}

struct report {
    std::string name;
    palette4 colours;
    std::vector<double> ucs_by_condition;  // minimum pairwise distance per condition
    double ucs_min = 0;
    std::string ucs_min_where;
    std::string ucs_min_pair;             // the two colours that come closest
    double de2000_min[4] = {0, 0, 0, 0};  // normal, protan, deutan, tritan (full severity)
    double oklab_min[4] = {0, 0, 0, 0};
    double grey_min = 0;   // minimum CAM02-UCS distance after greyscale conversion
    double frame_min = 0;  // minimum CAM02-UCS distance to the frame grey, normal vision
    double contrast_white = 0;
    double contrast_dark = 0;
    bool gate = false;
};

report evaluate(const std::string& name, const palette4& p) {
    const auto conds = conditions();
    report r;
    r.name = name;
    r.colours = p;
    r.ucs_min = 1e9;
    for (const auto& cond : conds) {
        double m = 1e9;
        std::string pair;
        for (std::size_t i = 0; i < 4; ++i) {
            for (std::size_t j = i + 1; j < 4; ++j) {
                const double d = distance(cam02ucs_from_linear(apply(cond, to_linear(p[i]))),
                                          cam02ucs_from_linear(apply(cond, to_linear(p[j]))));
                if (d < m) {
                    m = d;
                    pair = to_hex(p[i]) + "-" + to_hex(p[j]);
                }
            }
        }
        r.ucs_by_condition.push_back(m);
        if (m < r.ucs_min) {
            r.ucs_min = m;
            r.ucs_min_where = cond.name;
            r.ucs_min_pair = pair;
        }
    }
    const vision visions[4] = {vision::normal, vision::protan, vision::deutan, vision::tritan};
    for (int v = 0; v < 4; ++v) {
        double de = 1e9;
        double ok = 1e9;
        for (std::size_t i = 0; i < 4; ++i) {
            for (std::size_t j = i + 1; j < 4; ++j) {
                const vec3 a = simulate(to_linear(p[i]), visions[v]);
                const vec3 b = simulate(to_linear(p[j]), visions[v]);
                de = std::min(de, ciede2000(lab_from_linear(a), lab_from_linear(b)));
                ok = std::min(ok, 100.0 * distance(oklab_from_linear(a), oklab_from_linear(b)));
            }
        }
        r.de2000_min[v] = de;
        r.oklab_min[v] = ok;
    }
    r.grey_min = 1e9;
    for (std::size_t i = 0; i < 4; ++i) {
        for (std::size_t j = i + 1; j < 4; ++j) {
            r.grey_min =
                std::min(r.grey_min, distance(cam02ucs_from_linear(greyscale(to_linear(p[i]))),
                                              cam02ucs_from_linear(greyscale(to_linear(p[j])))));
        }
    }
    r.frame_min = 1e9;
    for (const rgb8& c : p) {
        r.frame_min = std::min(r.frame_min, distance(cam02ucs_from_linear(to_linear(frame_grey)),
                                                     cam02ucs_from_linear(to_linear(c))));
    }
    r.contrast_white = min_contrast(p, white);
    r.contrast_dark = min_contrast(p, dark);
    r.gate = r.ucs_min >= gate_distance && r.contrast_white >= gate_contrast &&
             r.contrast_dark >= gate_contrast;
    return r;
}

std::string hex_list(const palette4& p) {
    std::string s;
    for (std::size_t i = 0; i < 4; ++i) {
        s += (i ? " " : "") + to_hex(p[i]);
    }
    return s;
}

// Sorts the colours by hue in CAM02-UCS so equivalent palettes print alike.
palette4 canonical(palette4 p) {
    std::sort(p.begin(), p.end(), [](rgb8 a, rgb8 b) {
        const vec3 ua = cam02ucs_from_linear(to_linear(a));
        const vec3 ub = cam02ucs_from_linear(to_linear(b));
        return std::atan2(ua.z, ua.y) < std::atan2(ub.z, ub.y);
    });
    return p;
}

struct search_result {
    double score;
    palette4 colours;
};

// Simulated annealing over 8-bit sRGB with the contrast constraint as a hard
// constraint. `start` seeds the search; `max_step` bounds a channel move.
// With `anchor_radius` > 0 every colour must stay within that CAM02-UCS
// distance of its starting value (normal vision), which keeps the hue families.
// With `hue_tolerance` > 0 every colour must keep its CAM02-UCS hue within that many degrees of
// its starting value and at least `min_colourfulness` of its starting colourfulness M', so the
// search may move lightness but not the colour's identity.
search_result anneal(palette4 start, std::uint64_t seed, int iterations, int max_step, double t0,
                     bool with_frame, double anchor_radius = 0.0, double hue_tolerance = 0.0,
                     double min_colourfulness = 0.0) {
    const auto conds = conditions();
    std::array<vec3, 4> anchors{};
    for (std::size_t i = 0; i < 4; ++i) {
        anchors[i] = cam02ucs_from_linear(to_linear(start[i]));
    }
    std::mt19937_64 rng(seed);
    std::array<colour_views, 4> views;
    for (std::size_t i = 0; i < 4; ++i) {
        views[i] = views_of(start[i], conds);
    }
    double current = objective(views, conds.size());
    search_result best{current, start};
    palette4 p = start;
    for (int it = 0; it < iterations; ++it) {
        const double temperature = t0 * (1.0 - static_cast<double>(it) / iterations) + 0.01;
        const int step =
            std::max(1, static_cast<int>(max_step * (1.0 - static_cast<double>(it) / iterations)));
        const std::size_t i = static_cast<std::size_t>(rng() % 4);
        rgb8 c = p[i];
        auto move = [&](std::uint8_t v) {
            const int d = static_cast<int>(rng() % static_cast<unsigned>(2 * step + 1)) - step;
            return static_cast<std::uint8_t>(std::clamp(static_cast<int>(v) + d, 0, 255));
        };
        c = {move(c.r), move(c.g), move(c.b)};
        if (!allowed(c) || (with_frame && !chromatic(c))) {
            continue;
        }
        if (hue_tolerance > 0.0) {
            const vec3 u = cam02ucs_from_linear(to_linear(c));
            const vec3 a = anchors[i];
            double dh = std::atan2(u.z, u.y) - std::atan2(a.z, a.y);
            while (dh > 3.14159265358979323846) {
                dh -= 2 * 3.14159265358979323846;
            }
            while (dh < -3.14159265358979323846) {
                dh += 2 * 3.14159265358979323846;
            }
            if (std::fabs(dh) * 180.0 / 3.14159265358979323846 > hue_tolerance ||
                std::hypot(u.y, u.z) < min_colourfulness * std::hypot(a.y, a.z)) {
                continue;
            }
        }
        if (anchor_radius > 0.0 &&
            distance(cam02ucs_from_linear(to_linear(c)), anchors[i]) > anchor_radius) {
            continue;
        }
        const colour_views old = views[i];
        views[i] = views_of(c, conds);
        const double s = objective(views, conds.size());
        const double u = static_cast<double>(rng() % 1000000) / 1000000.0;
        if (s >= current || u < std::exp((s - current) / temperature)) {
            p[i] = c;
            current = s;
            if (s > best.score) {
                best = {s, p};
            }
        } else {
            views[i] = old;
        }
    }
    best.colours = canonical(best.colours);
    return best;
}

palette4 random_allowed(std::mt19937_64& rng, bool with_frame) {
    palette4 p{};
    for (auto& c : p) {
        do {
            c = {static_cast<std::uint8_t>(rng() % 256), static_cast<std::uint8_t>(rng() % 256),
                 static_cast<std::uint8_t>(rng() % 256)};
        } while (!allowed(c) || (with_frame && !chromatic(c)));
    }
    return p;
}

// Brings a colour into the contrast band by scaling its linear RGB towards
// black (too light) or mixing it with white (too dark), which keeps the hue.
rgb8 into_band(rgb8 c) {
    if (allowed(c)) {
        return c;
    }
    const vec3 lin = to_linear(c);
    const bool too_light =
        contrast_ratio(relative_luminance(c), relative_luminance(white)) < gate_contrast;
    double lo = 0.0;
    double hi = 1.0;
    rgb8 best = c;
    for (int i = 0; i < 40; ++i) {
        const double t = (lo + hi) / 2.0;
        const vec3 v =
            too_light ? vec3{lin.x * t, lin.y * t, lin.z * t}
                      : vec3{lin.x + (1.0 - lin.x) * (1.0 - t), lin.y + (1.0 - lin.y) * (1.0 - t),
                             lin.z + (1.0 - lin.z) * (1.0 - t)};
        const rgb8 q = from_linear(v);
        if (allowed(q)) {
            best = q;
            lo = t;  // allowed: move back towards the original
        } else {
            hi = t;
        }
    }
    return best;
}

void print_report(std::FILE* f, const report& r) {
    std::fprintf(f, "%s\t%s\t%.1f\t%s\t%s\t", r.name.c_str(), hex_list(r.colours).c_str(),
                 r.ucs_min, r.ucs_min_where.c_str(), r.ucs_min_pair.c_str());
    std::fprintf(
        f, "%.1f\t%.1f\t%.1f\t%.1f\t", r.ucs_by_condition.front(),
        *std::min_element(r.ucs_by_condition.begin() + 1, r.ucs_by_condition.begin() + 11),
        *std::min_element(r.ucs_by_condition.begin() + 11, r.ucs_by_condition.begin() + 21),
        r.ucs_by_condition.back());
    std::fprintf(f, "%.1f/%.1f/%.1f/%.1f\t%.1f/%.1f/%.1f/%.1f\t%.1f\t%.1f\t%.2f\t%.2f\t%s\n",
                 r.de2000_min[0], r.de2000_min[1], r.de2000_min[2], r.de2000_min[3], r.oklab_min[0],
                 r.oklab_min[1], r.oklab_min[2], r.oklab_min[3], r.grey_min, r.frame_min,
                 r.contrast_white, r.contrast_dark, r.gate ? "pass" : "fail");
}

// One sample image per palette under each vision condition, on white and on dark.
canvas palette_sheet(const std::vector<report>& reports, const std::string& title) {
    // Every colour on every figure: squares, circles, then the four triangles twice.
    const std::uint8_t fp_bytes[16] = {0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70, 0x78,
                                       0x80, 0xA8, 0xD0, 0xF8, 0x98, 0xB0, 0xC8, 0xE0};
    std::uint8_t fp[32] = {};
    std::copy(fp_bytes, fp_bytes + 16, fp);
    const cells c = features(fp, directions::four);
    std::vector<sheet_row> rows;
    for (const auto& r : reports) {
        design d = initial_design();
        d.palette = r.colours;
        sheet_row row;
        char caption[160];
        std::snprintf(caption, sizeof(caption), "%s\nmin %.1f, frame %.1f\n%s %s\n%s %s",
                      r.name.c_str(), r.ucs_min, r.frame_min, to_hex(r.colours[0]).c_str(),
                      to_hex(r.colours[1]).c_str(), to_hex(r.colours[2]).c_str(),
                      to_hex(r.colours[3]).c_str());
        row.caption = caption;
        for (int bg = 0; bg < 2; ++bg) {
            for (int v = 0; v < 5; ++v) {
                canvas img = render(c, d, 96, bg == 0 ? background::white : background::transparent,
                                    frame_style::none);
                canvas framed(96, 96,
                              bg == 0 ? rgba8{255, 255, 255, 255} : rgba8{0x12, 0x12, 0x12, 255});
                framed.draw(img, 0, 0);
                static const char* names[5] = {"normal", "protan", "deutan", "tritan", "grey"};
                if (v == 4) {
                    framed.transform(greyscale_transform());
                } else {
                    framed.transform(vision_transform(static_cast<vision>(v)));
                }
                row.tiles.push_back({framed, std::string(bg == 0 ? "white " : "dark ") + names[v]});
            }
        }
        rows.push_back(row);
    }
    return row_sheet(
        {title,
         "min = minimum pairwise cam02-ucs distance over all conditions (gate: 20); frame = to "
         "#808080 (normal vision)"},
        rows, 340, 12, light_page());
}

}  // namespace

int run_palette(const options& opt) {
    const std::string dir = opt.out_dir + "/palette";
    make_dirs(dir);

    std::vector<report> reports;
    reports.push_back(evaluate("initial", initial_design().palette));
    // Reference points: Okabe-Ito without black and yellow's light neighbours, and
    // the first four of Petroff's six-colour sequence (both fail the dark-surface
    // contrast; listed only to calibrate the distance scale).
    reports.push_back(evaluate(
        "okabe-ito 4",
        {{{0xE6, 0x9F, 0x00}, {0x56, 0xB4, 0xE9}, {0x00, 0x9E, 0x73}, {0xD5, 0x5E, 0x00}}}));
    reports.push_back(evaluate(
        "petroff6 first 4",
        {{{0x57, 0x90, 0xFC}, {0xF8, 0x9C, 0x20}, {0xE4, 0x25, 0x36}, {0x96, 0x4A, 0x8B}}}));

    const int iterations = opt.quick ? 3000 : 40000;
    const unsigned threads = thread_count(opt);

    // Runs `jobs` independent anneals on the thread pool and returns them best first.
    auto run_jobs = [&](unsigned jobs, const std::function<search_result(unsigned)>& job_fn) {
        std::vector<search_result> results(jobs);
        std::mutex next_mutex;
        unsigned next = 0;
        std::vector<std::thread> pool;
        for (unsigned t = 0; t < threads; ++t) {
            pool.emplace_back([&] {
                for (;;) {
                    unsigned job;
                    {
                        std::lock_guard<std::mutex> lock(next_mutex);
                        if (next >= jobs) {
                            return;
                        }
                        job = next++;
                    }
                    results[job] = job_fn(job);
                }
            });
        }
        for (auto& th : pool) {
            th.join();
        }
        std::sort(results.begin(), results.end(),
                  [](const auto& a, const auto& b) { return a.score > b.score; });
        return results;
    };

    // The best results that are not near-duplicates of each other.
    auto distinct = [](const std::vector<search_result>& results, std::size_t count) {
        std::vector<search_result> out;
        for (const auto& r : results) {
            bool duplicate = false;
            for (const auto& g : out) {
                double d = 0;
                for (std::size_t i = 0; i < 4; ++i) {
                    d += distance(cam02ucs_from_linear(to_linear(r.colours[i])),
                                  cam02ucs_from_linear(to_linear(g.colours[i])));
                }
                duplicate = duplicate || d < 20.0;
            }
            if (!duplicate) {
                out.push_back(r);
            }
            if (out.size() == count) {
                break;
            }
        }
        return out;
    };

    for (int with_frame = 0; with_frame < 2; ++with_frame) {
        const std::string suffix = with_frame ? ", frame-aware" : "";
        // Local refinement of the initial palette, each colour within 12 CAM02-UCS units of
        // its start.
        const auto refined = run_jobs(16, [&](unsigned job) {
            return anneal(initial_design().palette, 1000 + job, iterations, 6, 0.3, with_frame != 0,
                          12.0);
        });
        reports.push_back(evaluate("refined from initial" + suffix, refined.front().colours));

        // Global search from random starts.
        // Fixed job counts and seeds keep the result independent of the machine's thread count.
        const unsigned restarts = opt.quick ? 16 : 64;
        const auto global =
            distinct(run_jobs(restarts,
                              [&](unsigned job) {
                                  std::mt19937_64 rng(7000 + job);
                                  return anneal(random_allowed(rng, with_frame != 0), 9000 + job,
                                                iterations, 48, 2.0, with_frame != 0);
                              }),
                     3);
        for (std::size_t i = 0; i < global.size(); ++i) {
            reports.push_back(
                evaluate("search " + std::to_string(i + 1) + suffix, global[i].colours));
        }
    }

    // The colours picked by eye from the sheets: blue, orange and red of Petroff's six-colour
    // sequence and the violet of "search 3, frame-aware" (#981BCC; #783CD3 of "search 3" as the
    // alternative). The orange is too light for 3:1 on white, so every colour is first brought
    // into the contrast band with its hue kept, then tuned within a small radius.
    std::vector<report> picked;
    const palette4 picks[2] = {
        {{{0x57, 0x90, 0xFC}, {0xF8, 0x9C, 0x20}, {0xE4, 0x25, 0x36}, {0x98, 0x1B, 0xCC}}},
        {{{0x57, 0x90, 0xFC}, {0xF8, 0x9C, 0x20}, {0xE4, 0x25, 0x36}, {0x78, 0x3C, 0xD3}}}};
    for (int k = 0; k < 2; ++k) {
        const std::string tag = k == 0 ? "picked" : "alt violet";
        palette4 banded{};
        for (std::size_t i = 0; i < 4; ++i) {
            banded[i] = into_band(picks[k][i]);
        }
        picked.push_back(evaluate(tag + ", as is", picks[k]));
        picked.push_back(evaluate(tag + ", in the 3:1 band", banded));
        for (double radius : {6.0, 12.0}) {
            const auto tuned = run_jobs(16, [&](unsigned job) {
                return anneal(banded, 3000 + job, iterations, 4, 0.3, true, radius);
            });
            char name[96];
            std::snprintf(name, sizeof(name), "%s, tuned within %.0f", tag.c_str(), radius);
            picked.push_back(evaluate(name, tuned.front().colours));
        }
        // Hue kept within 10 degrees and colourfulness at least 80 %: lightness does the work.
        const auto hue_kept = run_jobs(16, [&](unsigned job) {
            return anneal(banded, 5000 + job, iterations, 8, 0.5, true, 0.0, 10.0, 0.8);
        });
        picked.push_back(evaluate(tag + ", hue kept", hue_kept.front().colours));
    }
    reports.insert(reports.end(), picked.begin(), picked.end());

    const std::string tsv = dir + "/palette_gate.tsv";
    std::FILE* f = std::fopen(tsv.c_str(), "w");
    if (f == nullptr) {
        return 1;
    }
    std::fprintf(f,
                 "palette\tcolours\tucs_min\tucs_min_at\tucs_min_pair\tucs_normal\tucs_protan_"
                 "min\tucs_deutan_min\t"
                 "ucs_tritan\tde2000_min_n/p/d/t\toklab_min_n/p/d/"
                 "t\tucs_grey_min\tucs_to_frame_min\tcontrast_white_min\t"
                 "contrast_121212_min\tgate\n");
    for (const auto& r : reports) {
        print_report(f, r);
        print_report(stdout, r);
    }
    std::fclose(f);
    std::printf("wrote %s\n", tsv.c_str());

    // Per-condition detail for every palette.
    const std::string detail = dir + "/palette_conditions.tsv";
    f = std::fopen(detail.c_str(), "w");
    if (f != nullptr) {
        const auto conds = conditions();
        std::fprintf(f, "condition");
        for (const auto& r : reports) {
            std::fprintf(f, "\t%s", r.name.c_str());
        }
        std::fprintf(f, "\n");
        for (std::size_t k = 0; k < conds.size(); ++k) {
            std::fprintf(f, "%s", conds[k].name.c_str());
            for (const auto& r : reports) {
                std::fprintf(f, "\t%.2f", r.ucs_by_condition[k]);
            }
            std::fprintf(f, "\n");
        }
        std::fclose(f);
        std::printf("wrote %s\n", detail.c_str());
    }

    save_sheet(
        dir + "/palette_candidates.png",
        palette_sheet(
            std::vector<report>(reports.begin(),
                                reports.end() - static_cast<std::ptrdiff_t>(picked.size())),
            "hh palette candidates: one sample image per palette under each vision condition"));
    save_sheet(dir + "/palette_picked.png",
               palette_sheet(picked,
                             "hh palette: the colours picked by eye, brought into the 3:1 band "
                             "and tuned"));
    return 0;
}

}  // namespace lab
}  // namespace hh
