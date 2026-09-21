#include "colour.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace hh {
namespace lab {

namespace {

constexpr double pi = 3.14159265358979323846;

using mat3 = std::array<std::array<double, 3>, 3>;

vec3 mul(const mat3& m, vec3 v) {
    return {m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z};
}

mat3 inverse(const mat3& m) {
    const double a = m[0][0], b = m[0][1], c = m[0][2];
    const double d = m[1][0], e = m[1][1], f = m[1][2];
    const double g = m[2][0], h = m[2][1], i = m[2][2];
    const double det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    return {{{(e * i - f * h) / det, (c * h - b * i) / det, (b * f - c * e) / det},
             {(f * g - d * i) / det, (a * i - c * g) / det, (c * d - a * f) / det},
             {(d * h - e * g) / det, (b * g - a * h) / det, (a * e - b * d) / det}}};
}

mat3 product(const mat3& x, const mat3& y) {
    mat3 out{};
    for (std::size_t r = 0; r < 3; ++r) {
        for (std::size_t c = 0; c < 3; ++c) {
            out[r][c] = x[r][0] * y[0][c] + x[r][1] * y[1][c] + x[r][2] * y[2][c];
        }
    }
    return out;
}

vec3 clamp01(vec3 v) {
    return {std::clamp(v.x, 0.0, 1.0), std::clamp(v.y, 0.0, 1.0), std::clamp(v.z, 0.0, 1.0)};
}

// IEC 61966-2-1:1999, XYZ (0..1) to linear sRGB.
constexpr mat3 xyz_to_srgb = {
    {{3.2406, -1.5372, -0.4986}, {-0.9689, 1.8758, 0.0415}, {0.0557, -0.2040, 1.0570}}};

const mat3& srgb_to_xyz() {
    static const mat3 m = inverse(xyz_to_srgb);
    return m;
}

constexpr mat3 m_cat02 = {
    {{0.7328, 0.4296, -0.1624}, {-0.7036, 1.6975, 0.0061}, {0.0030, 0.0136, 0.9834}}};

constexpr mat3 m_hpe = {
    {{0.38971, 0.68898, -0.07868}, {-0.22981, 1.18340, 0.04641}, {0.00000, 0.00000, 1.00000}}};

const mat3& hpe_cat02_inv() {
    static const mat3 m = product(m_hpe, inverse(m_cat02));
    return m;
}

// Machado et al. 2009 simulation matrices for severities 0.0, 0.1, ..., 1.0.
using machado_table = std::array<mat3, 11>;

constexpr machado_table machado_protan = {{
    {{{1.000000, 0.000000, -0.000000},
      {0.000000, 1.000000, 0.000000},
      {-0.000000, -0.000000, 1.000000}}},
    {{{0.856167, 0.182038, -0.038205},
      {0.029342, 0.955115, 0.015544},
      {-0.002880, -0.001563, 1.004443}}},
    {{{0.734766, 0.334872, -0.069637},
      {0.051840, 0.919198, 0.028963},
      {-0.004928, -0.004209, 1.009137}}},
    {{{0.630323, 0.465641, -0.095964},
      {0.069181, 0.890046, 0.040773},
      {-0.006308, -0.007724, 1.014032}}},
    {{{0.539009, 0.579343, -0.118352},
      {0.082546, 0.866121, 0.051332},
      {-0.007136, -0.011959, 1.019095}}},
    {{{0.458064, 0.679578, -0.137642},
      {0.092785, 0.846313, 0.060902},
      {-0.007494, -0.016807, 1.024301}}},
    {{{0.385450, 0.769005, -0.154455},
      {0.100526, 0.829802, 0.069673},
      {-0.007442, -0.022190, 1.029632}}},
    {{{0.319627, 0.849633, -0.169261},
      {0.106241, 0.815969, 0.077790},
      {-0.007025, -0.028051, 1.035076}}},
    {{{0.259411, 0.923008, -0.182420},
      {0.110296, 0.804340, 0.085364},
      {-0.006276, -0.034346, 1.040622}}},
    {{{0.203876, 0.990338, -0.194214},
      {0.112975, 0.794542, 0.092483},
      {-0.005222, -0.041043, 1.046265}}},
    {{{0.152286, 1.052583, -0.204868},
      {0.114503, 0.786281, 0.099216},
      {-0.003882, -0.048116, 1.051998}}},
}};

constexpr machado_table machado_deutan = {{
    {{{1.000000, 0.000000, -0.000000},
      {0.000000, 1.000000, 0.000000},
      {-0.000000, -0.000000, 1.000000}}},
    {{{0.866435, 0.177704, -0.044139},
      {0.049567, 0.939063, 0.011370},
      {-0.003453, 0.007233, 0.996220}}},
    {{{0.760729, 0.319078, -0.079807},
      {0.090568, 0.889315, 0.020117},
      {-0.006027, 0.013325, 0.992702}}},
    {{{0.675425, 0.433850, -0.109275},
      {0.125303, 0.847755, 0.026942},
      {-0.007950, 0.018572, 0.989378}}},
    {{{0.605511, 0.528560, -0.134071},
      {0.155318, 0.812366, 0.032316},
      {-0.009376, 0.023176, 0.986200}}},
    {{{0.547494, 0.607765, -0.155259},
      {0.181692, 0.781742, 0.036566},
      {-0.010410, 0.027275, 0.983136}}},
    {{{0.498864, 0.674741, -0.173604},
      {0.205199, 0.754872, 0.039929},
      {-0.011131, 0.030969, 0.980162}}},
    {{{0.457771, 0.731899, -0.189670},
      {0.226409, 0.731012, 0.042579},
      {-0.011595, 0.034333, 0.977261}}},
    {{{0.422823, 0.781057, -0.203881},
      {0.245752, 0.709602, 0.044646},
      {-0.011843, 0.037423, 0.974421}}},
    {{{0.392952, 0.823610, -0.216562},
      {0.263559, 0.690210, 0.046232},
      {-0.011910, 0.040281, 0.971630}}},
    {{{0.367322, 0.860646, -0.227968},
      {0.280085, 0.672501, 0.047413},
      {-0.011820, 0.042940, 0.968881}}},
}};

constexpr machado_table machado_tritan = {{
    {{{1.000000, 0.000000, -0.000000},
      {0.000000, 1.000000, 0.000000},
      {-0.000000, -0.000000, 1.000000}}},
    {{{0.926670, 0.092514, -0.019184},
      {0.021191, 0.964503, 0.014306},
      {0.008437, 0.054813, 0.936750}}},
    {{{0.895720, 0.133330, -0.029050},
      {0.029997, 0.945400, 0.024603},
      {0.013027, 0.104707, 0.882266}}},
    {{{0.905871, 0.127791, -0.033662},
      {0.026856, 0.941251, 0.031893},
      {0.013410, 0.148296, 0.838294}}},
    {{{0.948035, 0.089490, -0.037526},
      {0.014364, 0.946792, 0.038844},
      {0.010853, 0.193991, 0.795156}}},
    {{{1.017277, 0.027029, -0.044306},
      {-0.006113, 0.958479, 0.047634},
      {0.006379, 0.248708, 0.744913}}},
    {{{1.104996, -0.046633, -0.058363},
      {-0.032137, 0.971635, 0.060503},
      {0.001336, 0.317922, 0.680742}}},
    {{{1.193214, -0.109812, -0.083402},
      {-0.058496, 0.979410, 0.079086},
      {-0.002346, 0.403492, 0.598854}}},
    {{{1.257728, -0.139648, -0.118081},
      {-0.078003, 0.975409, 0.102594},
      {-0.003316, 0.501214, 0.502102}}},
    {{{1.278864, -0.125333, -0.153531},
      {-0.084748, 0.957674, 0.127074},
      {-0.000989, 0.601151, 0.399838}}},
    {{{1.255528, -0.076749, -0.178779},
      {-0.078411, 0.930809, 0.147602},
      {0.004733, 0.691367, 0.303900}}},
}};

// Brettel et al. 1997: two projection matrices in linear RGB and the normal of
// the separating plane, as precomputed by libDaltonLens.
struct brettel_params {
    mat3 plane1;
    mat3 plane2;
    vec3 separation_normal;
};

constexpr brettel_params brettel_protan = {
    {{{0.14980, 1.19548, -0.34528}, {0.10764, 0.84864, 0.04372}, {0.00384, -0.00540, 1.00156}}},
    {{{0.14570, 1.16172, -0.30742}, {0.10816, 0.85291, 0.03892}, {0.00386, -0.00524, 1.00139}}},
    {0.00048, 0.00393, -0.00441}};

constexpr brettel_params brettel_deutan = {
    {{{0.36477, 0.86381, -0.22858}, {0.26294, 0.64245, 0.09462}, {-0.02006, 0.02728, 0.99278}}},
    {{{0.37298, 0.88166, -0.25464}, {0.25954, 0.63506, 0.10540}, {-0.01980, 0.02784, 0.99196}}},
    {-0.00281, -0.00611, 0.00892}};

constexpr brettel_params brettel_tritan = {
    {{{1.01277, 0.13548, -0.14826}, {-0.01243, 0.86812, 0.14431}, {0.07589, 0.80500, 0.11911}}},
    {{{0.93678, 0.18979, -0.12657}, {0.06154, 0.81526, 0.12320}, {-0.37562, 1.12767, 0.24796}}},
    {0.03901, -0.02788, -0.01113}};

double lab_f(double t) {
    constexpr double delta = 6.0 / 29.0;
    if (t > delta * delta * delta) {
        return std::cbrt(t);
    }
    return t / (3.0 * delta * delta) + 4.0 / 29.0;
}

double degrees(double radians) {
    return radians * 180.0 / pi;
}

double radians(double deg) {
    return deg * pi / 180.0;
}

}  // namespace

std::string to_hex(rgb8 c) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", c.r, c.g, c.b);
    return std::string(buf);
}

double srgb_to_linear(double v) {
    return v < 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
}

double linear_to_srgb(double v) {
    return v <= 0.0031308 ? v * 12.92 : 1.055 * std::pow(v, 1.0 / 2.4) - 0.055;
}

vec3 to_linear(rgb8 c) {
    return {srgb_to_linear(c.r / 255.0), srgb_to_linear(c.g / 255.0), srgb_to_linear(c.b / 255.0)};
}

rgb8 from_linear(vec3 lin) {
    auto code = [](double v) {
        const double s = linear_to_srgb(std::clamp(v, 0.0, 1.0));
        return static_cast<std::uint8_t>(std::lround(std::clamp(s, 0.0, 1.0) * 255.0));
    };
    return {code(lin.x), code(lin.y), code(lin.z)};
}

vec3 xyz100_from_linear(vec3 lin) {
    const vec3 xyz = mul(srgb_to_xyz(), lin);
    return {xyz.x * 100.0, xyz.y * 100.0, xyz.z * 100.0};
}

double relative_luminance(rgb8 c) {
    const vec3 lin = to_linear(c);
    return 0.2126 * lin.x + 0.7152 * lin.y + 0.0722 * lin.z;
}

double contrast_ratio(double y1, double y2) {
    const double hi = std::max(y1, y2);
    const double lo = std::min(y1, y2);
    return (hi + 0.05) / (lo + 0.05);
}

ciecam02_conditions srgb_conditions() {
    return {{95.047, 100.0, 108.883}, 20.0, (64.0 / pi) / 5.0, 1.0, 0.69, 1.0};
}

ciecam02_result ciecam02_forward(vec3 xyz100, const ciecam02_conditions& vc) {
    const vec3 rgb_w = mul(m_cat02, vc.white_xyz100);
    const double d =
        std::clamp(vc.f * (1.0 - (1.0 / 3.6) * std::exp((-vc.l_a - 42.0) / 92.0)), 0.0, 1.0);
    const vec3 d_rgb = {d * vc.white_xyz100.y / rgb_w.x + 1.0 - d,
                        d * vc.white_xyz100.y / rgb_w.y + 1.0 - d,
                        d * vc.white_xyz100.y / rgb_w.z + 1.0 - d};
    const double k = 1.0 / (5.0 * vc.l_a + 1.0);
    const double k4 = k * k * k * k;
    const double f_l =
        0.2 * k4 * (5.0 * vc.l_a) + 0.1 * (1.0 - k4) * (1.0 - k4) * std::cbrt(5.0 * vc.l_a);
    const double n = vc.y_b / vc.white_xyz100.y;
    const double z = 1.48 + std::sqrt(n);
    const double n_bb = 0.725 * std::pow(1.0 / n, 0.2);
    const double n_cb = n_bb;

    auto adapt = [&](vec3 rgb) {
        const vec3 rgb_c = {d_rgb.x * rgb.x, d_rgb.y * rgb.y, d_rgb.z * rgb.z};
        const vec3 p = mul(hpe_cat02_inv(), rgb_c);
        auto compress = [&](double v) {
            const double sign = v < 0 ? -1.0 : 1.0;
            const double t = std::pow(f_l * std::fabs(v) / 100.0, 0.42);
            return sign * 400.0 * (t / (t + 27.13)) + 0.1;
        };
        return vec3{compress(p.x), compress(p.y), compress(p.z)};
    };

    const vec3 aw = adapt(rgb_w);
    const double a_w = (2.0 * aw.x + aw.y + aw.z / 20.0 - 0.305) * n_bb;

    const vec3 pa = adapt(mul(m_cat02, xyz100));
    const double a = pa.x - 12.0 * pa.y / 11.0 + pa.z / 11.0;
    const double b = (pa.x + pa.y - 2.0 * pa.z) / 9.0;
    const double h_rad = std::atan2(b, a);
    double h = std::fmod(degrees(h_rad), 360.0);
    if (h < 0) {
        h += 360.0;
    }
    const double achromatic = (2.0 * pa.x + pa.y + pa.z / 20.0 - 0.305) * n_bb;
    const double j = 100.0 * std::pow(std::max(achromatic, 0.0) / a_w, vc.c * z);
    const double e_t = (12500.0 / 13.0) * vc.n_c * n_cb * (std::cos(h_rad + 2.0) + 3.8);
    const double t = e_t * std::sqrt(a * a + b * b) / (pa.x + pa.y + 21.0 * pa.z / 20.0);
    const double c =
        std::pow(t, 0.9) * std::sqrt(j / 100.0) * std::pow(1.64 - std::pow(0.29, n), 0.73);
    const double m = c * std::pow(f_l, 0.25);
    return {j, c, h, m};
}

vec3 cam02ucs_from_jmh(double j, double m, double h_degrees) {
    constexpr double c1 = 0.007;
    constexpr double c2 = 0.0228;
    const double jp = (1.0 + 100.0 * c1) * j / (1.0 + c1 * j);
    const double mp = std::log(1.0 + c2 * m) / c2;
    return {jp, mp * std::cos(radians(h_degrees)), mp * std::sin(radians(h_degrees))};
}

vec3 cam02ucs_from_linear(vec3 lin) {
    static const ciecam02_conditions vc = srgb_conditions();
    const ciecam02_result r = ciecam02_forward(xyz100_from_linear(lin), vc);
    return cam02ucs_from_jmh(r.j, r.m, r.h);
}

vec3 lab_from_linear(vec3 lin) {
    const vec3 xyz = xyz100_from_linear(lin);
    const double fx = lab_f(xyz.x / 95.047);
    const double fy = lab_f(xyz.y / 100.0);
    const double fz = lab_f(xyz.z / 108.883);
    return {116.0 * fy - 16.0, 500.0 * (fx - fy), 200.0 * (fy - fz)};
}

double ciede2000(vec3 lab1, vec3 lab2) {
    const double l1 = lab1.x, a1 = lab1.y, b1 = lab1.z;
    const double l2 = lab2.x, a2 = lab2.y, b2 = lab2.z;
    const double c1 = std::hypot(a1, b1);
    const double c2 = std::hypot(a2, b2);
    const double c_bar = (c1 + c2) / 2.0;
    const double c_bar7 = std::pow(c_bar, 7.0);
    const double g = 0.5 * (1.0 - std::sqrt(c_bar7 / (c_bar7 + std::pow(25.0, 7.0))));
    const double a1p = (1.0 + g) * a1;
    const double a2p = (1.0 + g) * a2;
    const double c1p = std::hypot(a1p, b1);
    const double c2p = std::hypot(a2p, b2);
    auto hue = [](double b, double ap) {
        if (b == 0.0 && ap == 0.0) {
            return 0.0;
        }
        double h = degrees(std::atan2(b, ap));
        return h < 0 ? h + 360.0 : h;
    };
    const double h1p = hue(b1, a1p);
    const double h2p = hue(b2, a2p);
    const double dlp = l2 - l1;
    const double dcp = c2p - c1p;
    double dhp = 0.0;
    if (c1p * c2p != 0.0) {
        dhp = h2p - h1p;
        if (dhp > 180.0) {
            dhp -= 360.0;
        } else if (dhp < -180.0) {
            dhp += 360.0;
        }
    }
    const double d_hp = 2.0 * std::sqrt(c1p * c2p) * std::sin(radians(dhp / 2.0));
    const double lp_bar = (l1 + l2) / 2.0;
    const double cp_bar = (c1p + c2p) / 2.0;
    double hp_bar = h1p + h2p;
    if (c1p * c2p != 0.0) {
        if (std::fabs(h1p - h2p) <= 180.0) {
            hp_bar /= 2.0;
        } else if (h1p + h2p < 360.0) {
            hp_bar = (h1p + h2p + 360.0) / 2.0;
        } else {
            hp_bar = (h1p + h2p - 360.0) / 2.0;
        }
    }
    const double t = 1.0 - 0.17 * std::cos(radians(hp_bar - 30.0)) +
                     0.24 * std::cos(radians(2.0 * hp_bar)) +
                     0.32 * std::cos(radians(3.0 * hp_bar + 6.0)) -
                     0.20 * std::cos(radians(4.0 * hp_bar - 63.0));
    const double d_theta = 30.0 * std::exp(-((hp_bar - 275.0) / 25.0) * ((hp_bar - 275.0) / 25.0));
    const double cp_bar7 = std::pow(cp_bar, 7.0);
    const double r_c = 2.0 * std::sqrt(cp_bar7 / (cp_bar7 + std::pow(25.0, 7.0)));
    const double s_l = 1.0 + (0.015 * (lp_bar - 50.0) * (lp_bar - 50.0)) /
                                 std::sqrt(20.0 + (lp_bar - 50.0) * (lp_bar - 50.0));
    const double s_c = 1.0 + 0.045 * cp_bar;
    const double s_h = 1.0 + 0.015 * cp_bar * t;
    const double r_t = -std::sin(radians(2.0 * d_theta)) * r_c;
    const double tl = dlp / s_l;
    const double tc = dcp / s_c;
    const double th = d_hp / s_h;
    return std::sqrt(tl * tl + tc * tc + th * th + r_t * tc * th);
}

vec3 oklab_from_linear(vec3 lin) {
    const double l = 0.4122214708 * lin.x + 0.5363325363 * lin.y + 0.0514459929 * lin.z;
    const double m = 0.2119034982 * lin.x + 0.6806995451 * lin.y + 0.1073969566 * lin.z;
    const double s = 0.0883024619 * lin.x + 0.2817188376 * lin.y + 0.6299787005 * lin.z;
    const double l3 = std::cbrt(l);
    const double m3 = std::cbrt(m);
    const double s3 = std::cbrt(s);
    return {0.2104542553 * l3 + 0.7936177850 * m3 - 0.0040720468 * s3,
            1.9779984951 * l3 - 2.4285922050 * m3 + 0.4505937099 * s3,
            0.0259040371 * l3 + 0.7827717662 * m3 - 0.8086757660 * s3};
}

double distance(vec3 a, vec3 b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    const double dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

vec3 simulate_machado(vec3 lin, cvd type, int severity_tenths) {
    const int s = std::clamp(severity_tenths, 0, 10);
    switch (type) {
        case cvd::none:
            return lin;
        case cvd::protan:
            return clamp01(mul(machado_protan[static_cast<std::size_t>(s)], lin));
        case cvd::deutan:
            return clamp01(mul(machado_deutan[static_cast<std::size_t>(s)], lin));
        case cvd::tritan:
            return clamp01(mul(machado_tritan[static_cast<std::size_t>(s)], lin));
    }
    return lin;
}

vec3 simulate_brettel(vec3 lin, cvd type) {
    const brettel_params* p = nullptr;
    switch (type) {
        case cvd::none:
            return lin;
        case cvd::protan:
            p = &brettel_protan;
            break;
        case cvd::deutan:
            p = &brettel_deutan;
            break;
        case cvd::tritan:
            p = &brettel_tritan;
            break;
    }
    const vec3 n = p->separation_normal;
    const double side = lin.x * n.x + lin.y * n.y + lin.z * n.z;
    return clamp01(mul(side >= 0 ? p->plane1 : p->plane2, lin));
}

const char* vision_name(vision v) {
    switch (v) {
        case vision::normal:
            return "normal";
        case vision::protan:
            return "protan";
        case vision::deutan:
            return "deutan";
        case vision::tritan:
            return "tritan";
    }
    return "?";
}

vec3 simulate(vec3 lin, vision v) {
    switch (v) {
        case vision::normal:
            return lin;
        case vision::protan:
            return simulate_machado(lin, cvd::protan, 10);
        case vision::deutan:
            return simulate_machado(lin, cvd::deutan, 10);
        case vision::tritan:
            return simulate_brettel(lin, cvd::tritan);
    }
    return lin;
}

vec3 greyscale(vec3 lin) {
    const double y = 0.2126 * lin.x + 0.7152 * lin.y + 0.0722 * lin.z;
    return {y, y, y};
}

}  // namespace lab
}  // namespace hh
