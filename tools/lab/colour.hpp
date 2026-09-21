#pragma once

// Colour science for the design lab: sRGB, CIE XYZ, CIECAM02 and CAM02-UCS,
// CIELAB and CIEDE2000, OKLab, WCAG contrast, and colour vision deficiency
// simulation (Machado 2009, Brettel 1997). Floating point is allowed here and
// only here; nothing in this file is used by the library.

#include <array>
#include <cstdint>
#include <string>

namespace hh {
namespace lab {

struct rgb8 {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
};

inline bool operator==(rgb8 a, rgb8 b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

struct vec3 {
    double x;
    double y;
    double z;
};

// "#RRGGBB" in upper case.
std::string to_hex(rgb8 c);

// sRGB transfer function (IEC 61966-2-1) on the 0..1 scale.
double srgb_to_linear(double v);
double linear_to_srgb(double v);

vec3 to_linear(rgb8 c);
// Clamps to 0..1 and rounds to the nearest 8-bit code.
rgb8 from_linear(vec3 lin);

// CIE XYZ on the 0..100 scale (D65) from linear sRGB, using the inverse of the
// IEC 61966-2-1 XYZ-to-sRGB matrix, as colorspacious does.
vec3 xyz100_from_linear(vec3 lin);

// WCAG 2.x relative luminance and contrast ratio.
double relative_luminance(rgb8 c);
double contrast_ratio(double y1, double y2);

// CIECAM02 (CIE 159:2004) forward model for given viewing conditions.
struct ciecam02_conditions {
    vec3 white_xyz100;
    double y_b;
    double l_a;
    double f;
    double c;
    double n_c;
};

// The sRGB viewing conditions used by Petroff 2021 through colorspacious:
// D65 white, Y_b = 20, L_A = 64 / pi / 5, average surround.
ciecam02_conditions srgb_conditions();

struct ciecam02_result {
    double j;
    double c;
    double h;
    double m;
};

ciecam02_result ciecam02_forward(vec3 xyz100, const ciecam02_conditions& vc);

// CAM02-UCS (Luo, Cui and Li 2006) J', a', b' from J, M, h.
vec3 cam02ucs_from_jmh(double j, double m, double h_degrees);

// Convenience: CAM02-UCS coordinates of a linear sRGB colour under srgb_conditions().
vec3 cam02ucs_from_linear(vec3 lin);

// CIELAB (D65) and CIEDE2000 (Sharma, Wu and Dalal 2005).
vec3 lab_from_linear(vec3 lin);
double ciede2000(vec3 lab1, vec3 lab2);

// OKLab (Ottosson 2020).
vec3 oklab_from_linear(vec3 lin);

double distance(vec3 a, vec3 b);

// Colour vision deficiency simulation in linear sRGB. Results are clamped to 0..1.
enum class cvd {
    none,
    protan,
    deutan,
    tritan
};

// Machado, Oliveira and Fernandes 2009; severity in tenths (0..10), matrices
// from the authors' supplementary page (the same table colorspacious ships).
vec3 simulate_machado(vec3 lin, cvd type, int severity_tenths);

// Brettel, Vienot and Mollon 1997 at full severity, with the plane parameters
// precomputed by DaltonLens (Smith-Pokorny LMS, sRGB white as the neutral axis).
vec3 simulate_brettel(vec3 lin, cvd type);

// The vision conditions the lab reports: normal vision and the dichromacies
// at full severity (Machado for protan and deutan, Brettel for tritan).
enum class vision {
    normal,
    protan,
    deutan,
    tritan
};
const char* vision_name(vision v);
vec3 simulate(vec3 lin, vision v);

// Luminance-preserving greyscale (CIE Y), as a linear grey.
vec3 greyscale(vec3 lin);

}  // namespace lab
}  // namespace hh
