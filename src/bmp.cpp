// BMP encoder (SPEC.md section 12): 24-bit BI_RGB, bottom-up, flattened over a matte.

#include <hh/encode.hpp>

#include "encode_util.hpp"

namespace hh {

namespace {

void put_le16(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
}

void put_le32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    put_le16(out, v & 0xFFFFu);
    put_le16(out, v >> 16);
}

void encode(const detail::pixel_view& img, rgb matte, std::vector<std::uint8_t>& out) {
    const std::uint32_t row = 4 * ((3 * img.width + 3) / 4);
    const std::uint32_t data_size = row * img.height;
    out.reserve(54 + static_cast<std::size_t>(data_size));
    out.push_back('B');
    out.push_back('M');
    put_le32(out, 54 + data_size);
    put_le16(out, 0);
    put_le16(out, 0);
    put_le32(out, 54);
    put_le32(out, 40);
    put_le32(out, img.width);
    put_le32(out, img.height);
    put_le16(out, 1);
    put_le16(out, 24);
    put_le32(out, 0);
    put_le32(out, data_size);
    put_le32(out, 2835);
    put_le32(out, 2835);
    put_le32(out, 0);
    put_le32(out, 0);
    for (std::uint32_t y = img.height; y-- > 0;) {
        const std::uint8_t* p = img.rgba + static_cast<std::size_t>(y) * img.width * 4;
        for (std::uint32_t x = 0; x < img.width; ++x, p += 4) {
            out.push_back(detail::flatten(p[2], p[3], matte.b));
            out.push_back(detail::flatten(p[1], p[3], matte.g));
            out.push_back(detail::flatten(p[0], p[3], matte.r));
        }
        for (std::uint32_t pad = 3 * img.width; pad < row; ++pad) {
            out.push_back(0);
        }
    }
}

}  // namespace

namespace detail {

error_code encode_bmp_view(pixel_view img, rgb matte, std::vector<std::uint8_t>& out) noexcept {
    out.clear();
    try {
        encode(img, matte, out);
    } catch (...) {
        out.clear();
        return error_code::out_of_memory;
    }
    return error_code::ok;
}

}  // namespace detail

error_code encode_bmp(const image& img, rgb matte, std::vector<std::uint8_t>& out) noexcept {
    out.clear();
    if (!detail::valid_image(img)) {
        return error_code::invalid_image;
    }
    return detail::encode_bmp_view(detail::view_of(img), matte, out);
}

}  // namespace hh
