#include <cstdint>

#include "contrast.hpp"
#include "test_framework.hpp"
#include "test_util.hpp"

using hh::rgb;
using hh::detail::contrast_x100;

// Expected values are the WCAG ratio computed in floating point with Python,
// times 100 and rounded down.
TEST_CASE("contrast: known ratios") {
    EXPECT_EQ(contrast_x100({255, 255, 255}, {0, 0, 0}), 2100u);
    EXPECT_EQ(contrast_x100({0, 0, 0}, {255, 255, 255}), 2100u);
    EXPECT_EQ(contrast_x100({255, 255, 255}, {128, 128, 128}), 394u);
    EXPECT_EQ(contrast_x100({0x7A, 0x96, 0xC5}, {255, 255, 255}), 300u);
    EXPECT_EQ(contrast_x100({0x89, 0x0A, 0xF0}, {0x12, 0x12, 0x12}), 300u);
    EXPECT_EQ(contrast_x100({0xD4, 0x82, 0x00}, {0x9E, 0x9E, 0x9E}), 112u);
    EXPECT_EQ(contrast_x100({255, 0, 0}, {0, 0, 255}), 214u);
    EXPECT_EQ(contrast_x100({17, 34, 51}, {17, 34, 51}), 100u);
}

TEST_CASE("contrast: the palette keeps 3:1 on white and on #121212") {
    EXPECT_TRUE(hh::detail::figures_contrast_x100({255, 255, 255}) >= 300u);
    EXPECT_TRUE(hh::detail::figures_contrast_x100({0x12, 0x12, 0x12}) >= 300u);
    EXPECT_TRUE(hh::detail::figures_contrast_x100({0, 0, 0}) >= 300u);
}

TEST_CASE("contrast: compositing") {
    const rgb over_full = hh::detail::over({10, 20, 30}, 255, {200, 200, 200});
    EXPECT_TRUE(over_full == (rgb{10, 20, 30}));
    const rgb over_none = hh::detail::over({10, 20, 30}, 0, {200, 100, 50});
    EXPECT_TRUE(over_none == (rgb{200, 100, 50}));
    // (128 * 255 + 127 * 18 + 127) / 255 = 137
    const rgb half = hh::detail::over({255, 255, 255}, 128, {0x12, 0x12, 0x12});
    EXPECT_TRUE(half == (rgb{137, 137, 137}));
}

TEST_CASE("contrast: measure_contrast follows the options") {
    hh::render_options options;
    hh::contrast_report r = hh::measure_contrast(options, {0, 0, 0});
    EXPECT_EQ(r.figures_x100, 300u);
    EXPECT_EQ(r.frame_x100, 394u);

    options.background_alpha = 0;  // transparent: the page decides
    r = hh::measure_contrast(options, {0x12, 0x12, 0x12});
    EXPECT_EQ(r.figures_x100, 300u);
    r = hh::measure_contrast(options, {0x9E, 0x9E, 0x9E});
    EXPECT_EQ(r.figures_x100, 112u);

    options.background_alpha = 255;
    options.frame_alpha = 0;  // an invisible frame has no contrast
    r = hh::measure_contrast(options, {0, 0, 0});
    EXPECT_EQ(r.frame_x100, 100u);
}
