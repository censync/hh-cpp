#pragma once

// An RGBA8 canvas with the few drawing operations the sheets need: filling,
// straight-alpha compositing, colour transforms and a 5x7 bitmap font.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "colour.hpp"

namespace hh {
namespace lab {

struct rgba8 {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
    std::uint8_t a;
};

class canvas {
public:
    canvas() = default;
    canvas(int width, int height, rgba8 fill);

    int width() const { return width_; }
    int height() const { return height_; }
    const std::vector<std::uint8_t>& rgba() const { return rgba_; }

    rgba8 get(int x, int y) const;
    void set(int x, int y, rgba8 c);

    void fill_rect(int x, int y, int w, int h, rgba8 c);

    // Composites `src` (straight alpha) over this canvas at (x, y).
    void draw(const canvas& src, int x, int y);

    // Nearest-neighbour magnification by an integer factor.
    canvas magnified(int factor) const;

    // Applies `f` to the colour of every pixel (alpha untouched).
    void transform(const std::function<rgb8(rgb8)>& f);

    // Draws text with the built-in 5x7 font (upper case only; lower case is
    // mapped to upper case). Each font pixel is `scale` x `scale` pixels.
    void text(int x, int y, const std::string& s, int scale, rgba8 colour);

    static int text_width(const std::string& s, int scale);
    static int text_height(int scale);

private:
    std::size_t index(int x, int y) const;

    int width_ = 0;
    int height_ = 0;
    std::vector<std::uint8_t> rgba_;
};

// Builds a colour transform for a vision condition or greyscale, with a cache
// keyed on the input colour (sheets have few distinct colours).
std::function<rgb8(rgb8)> vision_transform(vision v);
std::function<rgb8(rgb8)> greyscale_transform();

// Writes the canvas as an 8-bit RGB (or RGBA) PNG.
bool write_png(const std::string& path, const canvas& c, bool with_alpha);

}  // namespace lab
}  // namespace hh
