#pragma once

// Helpers shared by the encoders. Internal.

#include <cstddef>
#include <cstdint>

#include <hh/encode.hpp>
#include <hh/image.hpp>

namespace hh {
namespace detail {

// RGBA pixels that somebody else owns.
struct pixel_view {
    const std::uint8_t* rgba;
    std::uint32_t width;
    std::uint32_t height;
};

inline bool valid_dimensions(std::uint32_t width, std::uint32_t height) noexcept {
    return width != 0 && height != 0 && width <= max_encoded_dimension &&
           height <= max_encoded_dimension;
}

// Whether the image is 1..4096 by 1..4096 with a buffer of width * height * 4 bytes.
inline bool valid_image(const image& img) noexcept {
    return valid_dimensions(img.width, img.height) &&
           img.rgba.size() == static_cast<std::size_t>(img.width) * img.height * 4;
}

inline pixel_view view_of(const image& img) noexcept {
    return {img.rgba.data(), img.width, img.height};
}

// The encoders proper. The view must be valid; they report ok, invalid_quality
// or out_of_memory and leave `out` empty on failure.
error_code encode_png_view(pixel_view img, std::vector<std::uint8_t>& out) noexcept;
error_code encode_bmp_view(pixel_view img, rgb matte, std::vector<std::uint8_t>& out) noexcept;
error_code encode_jpeg_view(pixel_view img, int quality, rgb matte,
                            std::vector<std::uint8_t>& out) noexcept;

// One channel of a pixel with alpha `a` flattened over the matte (SPEC.md section 10).
inline std::uint8_t flatten(std::uint8_t value, std::uint8_t a, std::uint8_t matte) noexcept {
    return static_cast<std::uint8_t>((std::uint32_t(a) * value + (255u - a) * matte + 127u) / 255u);
}

}  // namespace detail
}  // namespace hh
