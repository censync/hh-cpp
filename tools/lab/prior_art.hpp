#pragma once

// Ports of two widely used address identicons, for the grinding
// comparison. They follow the published JavaScript
// (ethereum/blockies, danfinlay/jazzicon 2.0) closely enough to reproduce the
// look and the parameter space; they are not byte-exact renderers.

#include <array>
#include <cstdint>
#include <string>

#include "canvas.hpp"
#include "colour.hpp"

namespace hh {
namespace lab {

// Blockies: xorshift128 seeded from the address string, three HSL colours and
// an 8x8 grid mirrored from 4x8 random cells (0 background, 1 colour, 2 spot).
struct blockie {
    rgb8 colour;
    rgb8 background;
    rgb8 spot;
    std::array<std::uint8_t, 64> data;
};

blockie make_blockie(const std::string& seed);
canvas render_blockie(const blockie& b, int size);

// Jazzicon: Mersenne Twister seeded with the first 32 bits of the address, a
// hue-shifted ten-colour palette, a background and three rotated squares,
// clipped to a circle.
struct jazzicon_shape {
    double tx;
    double ty;
    double rotation_degrees;
    rgb8 colour;
};

struct jazzicon {
    rgb8 background;
    std::array<jazzicon_shape, 3> shapes;
};

jazzicon make_jazzicon(std::uint32_t seed);

// Samples the icon at a point of the unit-diameter design (0..1 on both axes);
// points outside the circle are white.
rgb8 jazzicon_colour_at(const jazzicon& j, double x, double y);

canvas render_jazzicon(const jazzicon& j, int size);

// The raw Mersenne Twister stream, exposed for the self-test.
class mt19937_js {
public:
    explicit mt19937_js(std::uint32_t seed);
    std::uint32_t next_u32();
    double random();  // next_u32() / 2^32, as the npm mersenne-twister package

private:
    std::array<std::uint32_t, 624> mt_;
    std::size_t index_;
};

}  // namespace lab
}  // namespace hh
