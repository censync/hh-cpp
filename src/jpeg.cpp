// JPEG encoder (SPEC.md section 13): baseline JFIF, 4:4:4, annex K tables,
// the integer forward DCT with 13-bit constants, deterministic rounding.

#include <algorithm>
#include <cstdint>

#include <hh/encode.hpp>

#include "encode_util.hpp"
#include "jpeg_tables.hpp"

namespace hh {

namespace {

using bytes = std::vector<std::uint8_t>;
using i32 = std::int32_t;

// floor((x + 2^(n-1)) / 2^n) without shifting a negative value.
i32 descale(i32 x, int n) noexcept {
    const i32 v = x + (i32(1) << (n - 1));
    if (v >= 0) {
        return v >> n;
    }
    return -((-v + (i32(1) << n) - 1) >> n);
}

constexpr i32 f0298 = 2446;
constexpr i32 f0390 = 3196;
constexpr i32 f0541 = 4433;
constexpr i32 f0765 = 6270;
constexpr i32 f0899 = 7373;
constexpr i32 f1175 = 9633;
constexpr i32 f1501 = 12299;
constexpr i32 f1847 = 15137;
constexpr i32 f1961 = 16069;
constexpr i32 f2053 = 16819;
constexpr i32 f2562 = 20995;
constexpr i32 f3072 = 25172;

// One pass over eight values spaced `step` apart.
void dct_pass(i32* d, int step, bool first) noexcept {
    const i32 t0 = d[0] + d[7 * step];
    const i32 t7 = d[0] - d[7 * step];
    const i32 t1 = d[1 * step] + d[6 * step];
    const i32 t6 = d[1 * step] - d[6 * step];
    const i32 t2 = d[2 * step] + d[5 * step];
    const i32 t5 = d[2 * step] - d[5 * step];
    const i32 t3 = d[3 * step] + d[4 * step];
    const i32 t4 = d[3 * step] - d[4 * step];
    const i32 t10 = t0 + t3;
    const i32 t13 = t0 - t3;
    const i32 t11 = t1 + t2;
    const i32 t12 = t1 - t2;
    const int n = first ? 11 : 15;
    if (first) {
        d[0] = (t10 + t11) * 4;
        d[4 * step] = (t10 - t11) * 4;
    } else {
        d[0] = descale(t10 + t11, 2);
        d[4 * step] = descale(t10 - t11, 2);
    }
    i32 z1 = (t12 + t13) * f0541;
    d[2 * step] = descale(z1 + t13 * f0765, n);
    d[6 * step] = descale(z1 - t12 * f1847, n);

    z1 = t4 + t7;
    i32 z2 = t5 + t6;
    i32 z3 = t4 + t6;
    i32 z4 = t5 + t7;
    const i32 z5 = (z3 + z4) * f1175;
    const i32 m4 = t4 * f0298;
    const i32 m5 = t5 * f2053;
    const i32 m6 = t6 * f3072;
    const i32 m7 = t7 * f1501;
    z1 = -z1 * f0899;
    z2 = -z2 * f2562;
    z3 = -z3 * f1961 + z5;
    z4 = -z4 * f0390 + z5;
    d[7 * step] = descale(m4 + z1 + z3, n);
    d[5 * step] = descale(m5 + z2 + z4, n);
    d[3 * step] = descale(m6 + z2 + z3, n);
    d[1 * step] = descale(m7 + z1 + z4, n);
}

void forward_dct(i32* block) noexcept {
    for (int row = 0; row < 8; ++row) {
        dct_pass(block + 8 * row, 1, true);
    }
    for (int column = 0; column < 8; ++column) {
        dct_pass(block + column, 8, false);
    }
}

struct huffman_encoder {
    std::uint16_t code[256];
    std::uint8_t length[256];
};

// Canonical code assignment of T.81 annex C.
huffman_encoder build(const detail::jpeg_huffman_spec& spec) noexcept {
    huffman_encoder e{};
    std::uint32_t code = 0;
    std::size_t k = 0;
    for (unsigned len = 1; len <= 16; ++len) {
        for (unsigned i = 0; i < spec.counts[len - 1]; ++i) {
            const std::uint8_t symbol = spec.symbols[k++];
            e.code[symbol] = static_cast<std::uint16_t>(code);
            e.length[symbol] = static_cast<std::uint8_t>(len);
            ++code;
        }
        code <<= 1;
    }
    return e;
}

// Entropy-coded data: most significant bit first, FF followed by 00.
class scan_writer {
public:
    explicit scan_writer(bytes& out) noexcept : out_(out), acc_(0), used_(0) {}

    void put(std::uint32_t bits, unsigned count) {
        acc_ = (acc_ << count) | (bits & ((1u << count) - 1u));
        used_ += count;
        while (used_ >= 8) {
            const std::uint8_t b = static_cast<std::uint8_t>((acc_ >> (used_ - 8)) & 0xFFu);
            out_.push_back(b);
            if (b == 0xFF) {
                out_.push_back(0x00);
            }
            used_ -= 8;
        }
    }

    void finish() {
        if (used_ != 0) {
            put((1u << (8 - used_)) - 1u, 8 - used_);
        }
    }

private:
    bytes& out_;
    std::uint32_t acc_;  // at most 7 + 16 live bits
    unsigned used_;
};

unsigned category(i32 v) noexcept {
    std::uint32_t a = static_cast<std::uint32_t>(v < 0 ? -v : v);
    unsigned n = 0;
    while (a != 0) {
        ++n;
        a >>= 1;
    }
    return n;
}

void put_value(scan_writer& w, i32 v, unsigned bits) {
    if (bits == 0) {
        return;
    }
    const i32 encoded = v >= 0 ? v : v + (i32(1) << bits) - 1;
    w.put(static_cast<std::uint32_t>(encoded), bits);
}

void encode_block(scan_writer& w, const i32* zz, i32& previous_dc, const huffman_encoder& dc,
                  const huffman_encoder& ac) {
    const i32 diff = zz[0] - previous_dc;
    previous_dc = zz[0];
    const unsigned dc_bits = category(diff);
    w.put(dc.code[dc_bits], dc.length[dc_bits]);
    put_value(w, diff, dc_bits);

    unsigned run = 0;
    for (int k = 1; k < 64; ++k) {
        const i32 v = zz[k];
        if (v == 0) {
            ++run;
            continue;
        }
        while (run >= 16) {
            w.put(ac.code[0xF0], ac.length[0xF0]);
            run -= 16;
        }
        const unsigned bits = category(v);
        const unsigned symbol = run * 16 + bits;
        w.put(ac.code[symbol], ac.length[symbol]);
        put_value(w, v, bits);
        run = 0;
    }
    if (run != 0) {
        w.put(ac.code[0x00], ac.length[0x00]);
    }
}

void put_be16(bytes& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v >> 8));
    out.push_back(static_cast<std::uint8_t>(v));
}

void put_quantiser(bytes& out, std::uint8_t id, const std::uint8_t* table) {
    out.push_back(0xFF);
    out.push_back(0xDB);
    put_be16(out, 67);
    out.push_back(id);
    for (int i = 0; i < 64; ++i) {
        out.push_back(table[detail::jpeg_zigzag[i]]);
    }
}

void put_huffman(bytes& out, std::uint8_t id, const detail::jpeg_huffman_spec& spec) {
    out.push_back(0xFF);
    out.push_back(0xC4);
    put_be16(out, 2u + 1u + 16u + spec.symbol_count);
    out.push_back(id);
    out.insert(out.end(), spec.counts, spec.counts + 16);
    out.insert(out.end(), spec.symbols, spec.symbols + spec.symbol_count);
}

void scale_quantiser(const std::uint8_t* base, int quality, std::uint8_t* out) noexcept {
    const int scale = 200 - 2 * quality;
    for (int i = 0; i < 64; ++i) {
        out[i] = static_cast<std::uint8_t>(std::clamp((base[i] * scale + 50) / 100, 1, 255));
    }
}

void encode(const detail::pixel_view& img, int quality, rgb matte, bytes& out) {
    std::uint8_t quantiser[2][64];
    scale_quantiser(detail::jpeg_luminance_quantiser, quality, quantiser[0]);
    scale_quantiser(detail::jpeg_chrominance_quantiser, quality, quantiser[1]);

    const std::uint8_t header[] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 'J',  'F',  'I',  'F',
                                   0x00, 0x01, 0x02, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00};
    out.assign(header, header + sizeof(header));
    put_quantiser(out, 0, quantiser[0]);
    put_quantiser(out, 1, quantiser[1]);
    out.push_back(0xFF);
    out.push_back(0xC0);
    put_be16(out, 17);
    out.push_back(8);
    put_be16(out, img.height);
    put_be16(out, img.width);
    const std::uint8_t components[] = {3, 1, 0x11, 0, 2, 0x11, 1, 3, 0x11, 1};
    out.insert(out.end(), components, components + sizeof(components));
    put_huffman(out, 0x00, detail::jpeg_dc_luminance);
    put_huffman(out, 0x10, detail::jpeg_ac_luminance);
    put_huffman(out, 0x01, detail::jpeg_dc_chrominance);
    put_huffman(out, 0x11, detail::jpeg_ac_chrominance);
    const std::uint8_t scan[] = {0xFF, 0xDA, 0x00, 0x0C, 3, 1, 0x00, 2, 0x11, 3, 0x11, 0, 63, 0};
    out.insert(out.end(), scan, scan + sizeof(scan));

    const huffman_encoder dc[2] = {build(detail::jpeg_dc_luminance),
                                   build(detail::jpeg_dc_chrominance)};
    const huffman_encoder ac[2] = {build(detail::jpeg_ac_luminance),
                                   build(detail::jpeg_ac_chrominance)};
    scan_writer writer(out);
    i32 previous_dc[3] = {0, 0, 0};
    i32 block[3][64];
    i32 zz[64];
    for (std::uint32_t by = 0; by < img.height; by += 8) {
        for (std::uint32_t bx = 0; bx < img.width; bx += 8) {
            for (std::uint32_t j = 0; j < 8; ++j) {
                const std::uint32_t y = std::min(by + j, img.height - 1);
                for (std::uint32_t i = 0; i < 8; ++i) {
                    const std::uint32_t x = std::min(bx + i, img.width - 1);
                    const std::uint8_t* p =
                        img.rgba + (static_cast<std::size_t>(y) * img.width + x) * 4;
                    const i32 r = detail::flatten(p[0], p[3], matte.r);
                    const i32 g = detail::flatten(p[1], p[3], matte.g);
                    const i32 b = detail::flatten(p[2], p[3], matte.b);
                    const std::size_t k = 8 * j + i;
                    block[0][k] = (19595 * r + 38470 * g + 7471 * b + 32768) / 65536 - 128;
                    block[1][k] = (-11059 * r - 21709 * g + 32768 * b + 8421375) / 65536 - 128;
                    block[2][k] = (32768 * r - 27439 * g - 5329 * b + 8421375) / 65536 - 128;
                }
            }
            for (int c = 0; c < 3; ++c) {
                forward_dct(block[c]);
                const std::uint8_t* q = quantiser[c == 0 ? 0 : 1];
                for (int i = 0; i < 64; ++i) {
                    const int k = detail::jpeg_zigzag[i];
                    const i32 coefficient = block[c][k];
                    const i32 divisor = 8 * q[k];
                    i32 value = coefficient >= 0 ? (coefficient + divisor / 2) / divisor
                                                 : -((-coefficient + divisor / 2) / divisor);
                    // The baseline AC tables stop at 10 bits. DC values stay within
                    // -1024..1016 by construction, so their differences fit 11 bits.
                    if (i != 0) {
                        value = std::clamp(value, -1023, 1023);
                    }
                    zz[i] = value;
                }
                encode_block(writer, zz, previous_dc[c], dc[c == 0 ? 0 : 1], ac[c == 0 ? 0 : 1]);
            }
        }
    }
    writer.finish();
    out.push_back(0xFF);
    out.push_back(0xD9);
}

}  // namespace

namespace detail {

error_code encode_jpeg_view(pixel_view img, int quality, rgb matte,
                            std::vector<std::uint8_t>& out) noexcept {
    out.clear();
    if (quality < min_jpeg_quality || quality > max_jpeg_quality) {
        return error_code::invalid_quality;
    }
    try {
        encode(img, quality, matte, out);
    } catch (...) {
        out.clear();
        return error_code::out_of_memory;
    }
    return error_code::ok;
}

}  // namespace detail

error_code encode_jpeg(const image& img, int quality, rgb matte,
                       std::vector<std::uint8_t>& out) noexcept {
    out.clear();
    if (!detail::valid_image(img)) {
        return error_code::invalid_image;
    }
    return detail::encode_jpeg_view(detail::view_of(img), quality, matte, out);
}

}  // namespace hh
