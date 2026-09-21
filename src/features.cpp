#include "features.hpp"

#include "palette.hpp"

namespace hh {
namespace detail {

namespace {

constexpr figure figure_table[8] = {
    figure::none,        figure::none,           figure::square,        figure::circle,
    figure::triangle_up, figure::triangle_right, figure::triangle_down, figure::triangle_left,
};

constexpr char crockford[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

std::uint32_t rgb_word(rgb c) noexcept {
    return (std::uint32_t(c.r) << 16) | (std::uint32_t(c.g) << 8) | std::uint32_t(c.b);
}

}  // namespace

std::array<cell, 16> cells_of(const std::uint8_t* fp) noexcept {
    std::array<cell, 16> out{};
    for (std::size_t i = 0; i < out.size(); ++i) {
        const std::uint8_t b = fp[i];
        const figure f = figure_table[b >> 5];
        out[i].figure = f;
        out[i].colour =
            f == figure::none ? std::uint8_t{0} : static_cast<std::uint8_t>((b >> 3) & 3u);
    }
    return out;
}

std::array<char, 7> tag_of(const std::uint8_t* fp) noexcept {
    const std::uint32_t word = (std::uint32_t(fp[16]) << 24) | (std::uint32_t(fp[17]) << 16) |
                               (std::uint32_t(fp[18]) << 8) | std::uint32_t(fp[19]);
    const std::uint32_t v = word >> 2;
    std::array<char, 7> out{};
    for (unsigned i = 0; i < 6; ++i) {
        out[i] = crockford[(v >> (25u - 5u * i)) & 31u];
    }
    out[6] = '\0';
    return out;
}

}  // namespace detail

std::array<char, 7> fingerprint::tag_chars() const noexcept {
    return detail::tag_of(bytes_.data());
}

std::string fingerprint::tag() const {
    return std::string(tag_chars().data());
}

layout describe(const fingerprint& fp) noexcept {
    layout out{};
    out.mode = fp.mode();
    out.cells = detail::cells_of(fp.bytes().data());
    if (fp.empty()) {
        out.cells = {};
    }
    for (std::size_t i = 0; i < out.palette_rgb.size(); ++i) {
        out.palette_rgb[i] = detail::rgb_word(detail::palette[i]);
    }
    out.frame_rgb = detail::rgb_word(detail::frame_colour);
    return out;
}

}  // namespace hh
