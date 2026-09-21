#pragma once

#include <cstdint>
#include <vector>

#include "hh/error.hpp"
#include "hh/export.hpp"
#include "hh/image.hpp"
#include "hh/types.hpp"

namespace hh {

// The encoders are deterministic: the same image gives the same bytes in every
// implementation of hh. They accept any RGBA image of 1..4096 by 1..4096 pixels.
// On failure `out` is left empty.

constexpr std::uint32_t max_encoded_dimension = 4096;
constexpr int default_jpeg_quality = 92;
constexpr int min_jpeg_quality = 50;
constexpr int max_jpeg_quality = 100;

// 8-bit truecolour PNG; with alpha only if some pixel is not opaque.
HH_API error_code encode_png(const image& img, std::vector<std::uint8_t>& out) noexcept;

// 24-bit BMP. Pixels are flattened over `matte` (BMP has no alpha here).
HH_API error_code encode_bmp(const image& img, rgb matte, std::vector<std::uint8_t>& out) noexcept;

// Baseline JFIF, 4:4:4, quality 50..100. Pixels are flattened over `matte`.
// Offered for compatibility: JPEG rings on flat colour edges, prefer PNG.
HH_API error_code encode_jpeg(const image& img, int quality, rgb matte,
                              std::vector<std::uint8_t>& out) noexcept;

}  // namespace hh
