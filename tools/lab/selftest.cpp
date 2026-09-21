// Self-test of the lab's colour science and prior-art ports against published
// or independently computed reference values. The lab's numbers are only as
// good as these checks.

#include <cmath>
#include <cstdio>
#include <random>
#include <string>

#include <hh/hh.hpp>

#include "candidate.hpp"
#include "colour.hpp"
#include "lab.hpp"
#include "prior_art.hpp"

namespace hh {
namespace lab {

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("[%s] %s\n", ok ? " OK " : "FAIL", what.c_str());
    if (!ok) {
        ++failures;
    }
}

bool near(double a, double b, double tol) {
    return std::fabs(a - b) <= tol;
}

vec3 linear_from_srgb1(vec3 s) {
    return {srgb_to_linear(s.x), srgb_to_linear(s.y), srgb_to_linear(s.z)};
}

vec3 srgb1_from_linear(vec3 l) {
    return {linear_to_srgb(l.x), linear_to_srgb(l.y), linear_to_srgb(l.z)};
}

double ucs_delta(rgb8 a, rgb8 b) {
    return distance(cam02ucs_from_linear(to_linear(a)), cam02ucs_from_linear(to_linear(b)));
}

}  // namespace

int run_selftest(const options&) {
    failures = 0;

    // CIE 159:2004 section 9 worked example (the gold values colorspacious tests against).
    {
        ciecam02_conditions vc{{98.88, 90.0, 32.03}, 18.0, 200.0, 1.0, 0.69, 1.0};
        const auto r = ciecam02_forward({19.31, 23.93, 10.14}, vc);
        check(near(r.j, 48.0314, 1e-3) && near(r.c, 38.7789, 1e-3) && near(r.h, 191.0452, 1e-3),
              "ciecam02 cie 159:2004 example, L_A = 200 (J " + std::to_string(r.j) + ", C " +
                  std::to_string(r.c) + ", h " + std::to_string(r.h) + ")");
        vc.l_a = 20.0;
        const auto r2 = ciecam02_forward({19.31, 23.93, 10.14}, vc);
        check(near(r2.j, 47.6856, 1e-3) && near(r2.h, 185.3445, 1e-3),
              "ciecam02 cie 159:2004 example, L_A = 20 (J " + std::to_string(r2.j) + ", h " +
                  std::to_string(r2.h) + ")");
    }

    // CAM02-UCS: colorspacious reference values.
    {
        const vec3 u = cam02ucs_from_jmh(50.0, 20.0, 10.0);
        check(near(u.x, 62.96296296, 1e-6) && near(u.y, 16.22742674, 1e-6) &&
                  near(u.z, 2.86133316, 1e-6),
              "cam02-ucs from JMh (50, 20, 10)");
        const double d1 = ucs_delta({173, 52, 52}, {69, 120, 51});
        const double d2 = ucs_delta({69, 100, 52}, {69, 120, 51});
        check(near(d1, 44.698469808449964, 1e-6),
              "cam02-ucs delta E (173,52,52)-(69,120,51) = " + std::to_string(d1));
        check(near(d2, 8.503323264883667, 1e-6),
              "cam02-ucs delta E (69,100,52)-(69,120,51) = " + std::to_string(d2));
    }

    // Machado 2009 in linear sRGB: colorspacious reference values.
    {
        const vec3 in = linear_from_srgb1({0.1, 0.2, 0.3});
        const vec3 d50 = srgb1_from_linear(simulate_machado(in, cvd::deutan, 5));
        check(near(d50.x, 0.12440528, 1e-6) && near(d50.y, 0.19103024, 1e-6) &&
                  near(d50.z, 0.29911687, 1e-6),
              "machado deuteranomaly 0.5");
        const vec3 p90 = simulate_machado(in, cvd::protan, 9);
        const vec3 p100 = simulate_machado(in, cvd::protan, 10);
        const vec3 p95 =
            srgb1_from_linear({(p90.x + p100.x) / 2, (p90.y + p100.y) / 2, (p90.z + p100.z) / 2});
        check(near(p95.x, 0.15588987, 1e-6) && near(p95.y, 0.2038791, 1e-6) &&
                  near(p95.z, 0.30416046, 1e-6),
              "machado protanomaly 0.95 (interpolated)");
    }

    // CIEDE2000: Sharma, Wu and Dalal 2005, test pairs 1, 17 and 25.
    {
        const double e1 = ciede2000({50.0, 2.6772, -79.7751}, {50.0, 0.0, -82.7485});
        const double e17 = ciede2000({50.0, 2.5, 0.0}, {73.0, 25.0, -18.0});
        const double e25 = ciede2000({60.2574, -34.0099, 36.2677}, {60.4626, -34.1751, 39.4387});
        check(near(e1, 2.0425, 1e-4) && near(e17, 27.1492, 1e-4) && near(e25, 1.2644, 1e-4),
              "ciede2000 sharma pairs 1, 17, 25 (" + std::to_string(e1) + ", " +
                  std::to_string(e17) + ", " + std::to_string(e25) + ")");
    }

    // OKLab: Ottosson's reference for sRGB red, and white.
    {
        const vec3 red = oklab_from_linear(to_linear({255, 0, 0}));
        const vec3 white = oklab_from_linear(to_linear({255, 255, 255}));
        check(near(red.x, 0.627955, 1e-4) && near(red.y, 0.224863, 1e-4) &&
                  near(red.z, 0.125846, 1e-4),
              "oklab of #FF0000");
        check(near(white.x, 1.0, 1e-4) && near(white.y, 0.0, 1e-4) && near(white.z, 0.0, 1e-4),
              "oklab of #FFFFFF");
    }

    // WCAG contrast.
    {
        const double c21 =
            contrast_ratio(relative_luminance({255, 255, 255}), relative_luminance({0, 0, 0}));
        const double grey = contrast_ratio(relative_luminance({255, 255, 255}),
                                           relative_luminance({128, 128, 128}));
        check(near(c21, 21.0, 1e-9) && near(grey, 3.949, 1e-3),
              "wcag contrast white/black and white/#808080");
    }

    // Blockies: reference output of the ethereum/blockies functions run in node.
    {
        struct ref {
            const char* seed;
            rgb8 colour;
            rgb8 background;
            rgb8 spot;
            const char* data;
        };
        const ref refs[] = {
            {"0xfb6916095ca1df60bb79ce92ce3ea74c37c5d359",
             {250, 173, 21},
             {231, 237, 51},
             {115, 105, 248},
             "1000000100000000110000111211112101011010021221200200002010211201"},
            {"0x0000000000000000000000000000000000000000",
             {218, 69, 84},
             {44, 118, 209},
             {200, 123, 165},
             "0010010010000001011001102111111210122101100110012000000200211200"},
            {"hh",
             {36, 16, 16},
             {35, 41, 13},
             {158, 22, 63},
             "0011110021000012100000011100001100200200100220010011110011211211"},
        };
        for (const auto& r : refs) {
            const blockie b = make_blockie(r.seed);
            std::string data;
            for (std::uint8_t v : b.data) {
                data.push_back(static_cast<char>('0' + v));
            }
            check(b.colour == r.colour && b.background == r.background && b.spot == r.spot &&
                      data == r.data,
                  std::string("blockies port matches the javascript for seed ") + r.seed);
        }
    }

    // Mersenne Twister: the npm package's init_genrand equals std::mt19937 seeding.
    {
        bool same = true;
        for (std::uint32_t seed : {5489u, 1u, 0xDEADBEEFu}) {
            mt19937_js a(seed);
            std::mt19937 b(seed);
            for (int i = 0; i < 2000; ++i) {
                same = same && a.next_u32() == static_cast<std::uint32_t>(b());
            }
        }
        check(same, "mt19937 port matches std::mt19937");
    }

    // The sheets are drawn by the library: the lab's adapter and hh::render agree.
    {
        const auto bytes = sample_fingerprint("hh-lab/selftest", 0);
        hh::fingerprint fp;
        hh::import_fingerprint({bytes.data(), bytes.size()}, hh::mode::keyed, fp);
        bool same = true;
        for (int round = 0; round < 2; ++round) {
            hh::render_options options;
            options.shape = round ? hh::image_shape::round : hh::image_shape::square;
            options.frame = round ? hh::frame_style::ticks : hh::frame_style::rounded;
            options.background_alpha = 0;
            hh::image img;
            same = same && hh::render(fp, 96, options, img) == hh::error_code::ok;
            const canvas c = render(features(bytes.data(), directions::four), proposed_design(), 96,
                                    background::transparent,
                                    round ? frame_style::ring_ticks : frame_style::rounded);
            same = same && c.rgba() == img.rgba;
        }
        check(same, "the lab draws through the library: its sheets equal hh::render");
    }

    std::printf("selftest: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}

}  // namespace lab
}  // namespace hh
