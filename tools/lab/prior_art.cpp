#include "prior_art.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace hh {
namespace lab {

namespace {

constexpr double pi = 3.14159265358979323846;

// JavaScript int32 arithmetic on uint32 storage.
std::uint32_t shl(std::uint32_t v, unsigned n) {
    return v << n;
}

std::uint32_t asr(std::uint32_t v, unsigned n) {
    const std::uint32_t shifted = v >> n;
    return (v & 0x80000000u) ? shifted | ~(0xFFFFFFFFu >> n) : shifted;
}

class xorshift_js {
public:
    explicit xorshift_js(const std::string& seed) {
        for (std::size_t i = 0; i < seed.size(); ++i) {
            // ((s << 5) - s) + charCode, reduced modulo 2^32 like every later use.
            std::uint32_t& s = state_[i % 4];
            s = shl(s, 5) - s + static_cast<std::uint32_t>(static_cast<unsigned char>(seed[i]));
        }
    }

    double rand() {
        const std::uint32_t t = state_[0] ^ shl(state_[0], 11);
        state_[0] = state_[1];
        state_[1] = state_[2];
        state_[2] = state_[3];
        state_[3] = state_[3] ^ asr(state_[3], 19) ^ t ^ asr(t, 8);
        return static_cast<double>(state_[3]) / 2147483648.0;
    }

private:
    std::array<std::uint32_t, 4> state_{};
};

// CSS hsl() to sRGB, as browsers convert fill styles (saturation and lightness
// are clamped to 0..100 %, hue wraps).
rgb8 css_hsl(double hue_degrees, double sat_percent, double light_percent) {
    const double s = std::clamp(sat_percent, 0.0, 100.0) / 100.0;
    const double l = std::clamp(light_percent, 0.0, 100.0) / 100.0;
    double h = std::fmod(hue_degrees, 360.0);
    if (h < 0) {
        h += 360.0;
    }
    h /= 60.0;
    const double t2 = l <= 0.5 ? l * (s + 1.0) : l + s - l * s;
    const double t1 = 2.0 * l - t2;
    auto channel = [&](double hh) {
        if (hh < 0) {
            hh += 6.0;
        }
        if (hh >= 6.0) {
            hh -= 6.0;
        }
        double v;
        if (hh < 1.0) {
            v = (t2 - t1) * hh + t1;
        } else if (hh < 3.0) {
            v = t2;
        } else if (hh < 4.0) {
            v = (t2 - t1) * (4.0 - hh) + t1;
        } else {
            v = t1;
        }
        return static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0, 1.0) * 255.0));
    };
    return {channel(h + 2.0), channel(h), channel(h - 2.0)};
}

rgb8 blockie_colour(xorshift_js& r) {
    const double h = std::floor(r.rand() * 360.0);
    const double s = r.rand() * 60.0 + 40.0;
    const double l = (r.rand() + r.rand() + r.rand() + r.rand()) * 25.0;
    return css_hsl(h, s, l);
}

// RGB <-> HSL as the `color` 0.11 package does for rotate().
void rgb_to_hsl(rgb8 c, double& h, double& s, double& l) {
    const double r = c.r / 255.0;
    const double g = c.g / 255.0;
    const double b = c.b / 255.0;
    const double mx = std::max({r, g, b});
    const double mn = std::min({r, g, b});
    const double delta = mx - mn;
    l = (mx + mn) / 2.0;
    if (delta == 0) {
        h = 0;
        s = 0;
        return;
    }
    s = l < 0.5 ? delta / (mx + mn) : delta / (2.0 - mx - mn);
    if (mx == r) {
        h = (g - b) / delta;
    } else if (mx == g) {
        h = 2.0 + (b - r) / delta;
    } else {
        h = 4.0 + (r - g) / delta;
    }
    h = std::min(h * 60.0, 360.0);
    if (h < 0) {
        h += 360.0;
    }
}

rgb8 rotate_hue(rgb8 c, double degrees) {
    double h = 0;
    double s = 0;
    double l = 0;
    rgb_to_hsl(c, h, s, l);
    h = std::fmod(h + degrees, 360.0);
    if (h < 0) {
        h += 360.0;
    }
    return css_hsl(h, s * 100.0, l * 100.0);
}

rgb8 pick_colour(std::vector<rgb8>& remaining, mt19937_js& g) {
    (void)g.random();  // jazzicon draws and discards one value here
    const auto idx =
        static_cast<std::size_t>(std::floor(static_cast<double>(remaining.size()) * g.random()));
    const rgb8 c = remaining[idx];
    remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(idx));
    return c;
}

}  // namespace

blockie make_blockie(const std::string& seed) {
    xorshift_js r(seed);
    blockie b{};
    b.colour = blockie_colour(r);
    b.background = blockie_colour(r);
    b.spot = blockie_colour(r);
    for (int y = 0; y < 8; ++y) {
        std::uint8_t row[4];
        for (auto& v : row) {
            v = static_cast<std::uint8_t>(std::floor(r.rand() * 2.3));
        }
        for (int x = 0; x < 4; ++x) {
            b.data[static_cast<std::size_t>(y * 8 + x)] = row[x];
            b.data[static_cast<std::size_t>(y * 8 + 7 - x)] = row[x];
        }
    }
    return b;
}

canvas render_blockie(const blockie& b, int size) {
    canvas out(size, size, {255, 255, 255, 255});
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const std::uint8_t v =
                b.data[static_cast<std::size_t>((y * 8 / size) * 8 + x * 8 / size)];
            const rgb8 c = v == 0 ? b.background : (v == 1 ? b.colour : b.spot);
            out.set(x, y, {c.r, c.g, c.b, 255});
        }
    }
    return out;
}

mt19937_js::mt19937_js(std::uint32_t seed) : mt_{}, index_(624) {
    mt_[0] = seed;
    for (std::size_t i = 1; i < 624; ++i) {
        mt_[i] = 1812433253u * (mt_[i - 1] ^ (mt_[i - 1] >> 30)) + static_cast<std::uint32_t>(i);
    }
}

std::uint32_t mt19937_js::next_u32() {
    if (index_ >= 624) {
        for (std::size_t k = 0; k < 624; ++k) {
            const std::uint32_t y = (mt_[k] & 0x80000000u) | (mt_[(k + 1) % 624] & 0x7FFFFFFFu);
            mt_[k] = mt_[(k + 397) % 624] ^ (y >> 1) ^ ((y & 1u) ? 0x9908B0DFu : 0u);
        }
        index_ = 0;
    }
    std::uint32_t y = mt_[index_++];
    y ^= y >> 11;
    y ^= (y << 7) & 0x9D2C5680u;
    y ^= (y << 15) & 0xEFC60000u;
    y ^= y >> 18;
    return y;
}

double mt19937_js::random() {
    return next_u32() * (1.0 / 4294967296.0);
}

jazzicon make_jazzicon(std::uint32_t seed) {
    static const rgb8 palette[10] = {{0x01, 0x88, 0x8C}, {0xFC, 0x75, 0x00}, {0x03, 0x4F, 0x5D},
                                     {0xF7, 0x3F, 0x01}, {0xFC, 0x19, 0x60}, {0xC7, 0x14, 0x4C},
                                     {0xF3, 0xC1, 0x00}, {0x15, 0x98, 0xF2}, {0x24, 0x65, 0xE1},
                                     {0xF1, 0x9E, 0x02}};
    mt19937_js g(seed);
    const double amount = g.random() * 30.0 - 15.0;
    std::vector<rgb8> remaining;
    for (const rgb8& c : palette) {
        remaining.push_back(rotate_hue(c, amount));
    }
    jazzicon j{};
    j.background = pick_colour(remaining, g);
    constexpr double total = 3.0;
    for (int i = 0; i < 3; ++i) {
        const double first_rot = g.random();
        const double angle = 2.0 * pi * first_rot;
        const double velocity = 1.0 / total * g.random() + i * 1.0 / total;
        jazzicon_shape& s = j.shapes[static_cast<std::size_t>(i)];
        s.tx = std::cos(angle) * velocity;
        s.ty = std::sin(angle) * velocity;
        const double second_rot = g.random();
        s.rotation_degrees = first_rot * 360.0 + second_rot * 180.0;
        s.colour = pick_colour(remaining, g);
    }
    return j;
}

rgb8 jazzicon_colour_at(const jazzicon& j, double x, double y) {
    const double dx = x - 0.5;
    const double dy = y - 0.5;
    if (dx * dx + dy * dy > 0.25) {
        return {255, 255, 255};
    }
    rgb8 c = j.background;
    for (const jazzicon_shape& s : j.shapes) {
        // transform = translate(tx ty) rotate(rot 0.5 0.5): invert it for the point.
        const double qx = x - s.tx - 0.5;
        const double qy = y - s.ty - 0.5;
        const double a = s.rotation_degrees * pi / 180.0;
        const double px = std::cos(a) * qx + std::sin(a) * qy + 0.5;
        const double py = -std::sin(a) * qx + std::cos(a) * qy + 0.5;
        if (px >= 0.0 && px <= 1.0 && py >= 0.0 && py <= 1.0) {
            c = s.colour;
        }
    }
    return c;
}

canvas render_jazzicon(const jazzicon& j, int size) {
    canvas out(size, size, {255, 255, 255, 255});
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            vec3 acc{0, 0, 0};
            for (int sy = 0; sy < 4; ++sy) {
                for (int sx = 0; sx < 4; ++sx) {
                    const rgb8 c = jazzicon_colour_at(j, (x + (sx + 0.5) / 4.0) / size,
                                                      (y + (sy + 0.5) / 4.0) / size);
                    const vec3 lin = to_linear(c);
                    acc = {acc.x + lin.x, acc.y + lin.y, acc.z + lin.z};
                }
            }
            const rgb8 c = from_linear({acc.x / 16.0, acc.y / 16.0, acc.z / 16.0});
            out.set(x, y, {c.r, c.g, c.b, 255});
        }
    }
    return out;
}

}  // namespace lab
}  // namespace hh
