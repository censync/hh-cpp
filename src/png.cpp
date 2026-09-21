// PNG encoder (SPEC.md section 11): filter 0, one fixed-Huffman deflate block,
// greedy matches against the previous pixel and the pixel above.

#include <algorithm>
#include <new>

#include <hh/encode.hpp>

#include "checksum.hpp"
#include "encode_util.hpp"

namespace hh {

namespace {

using bytes = std::vector<std::uint8_t>;

// Deflate packs bits from the least significant bit of each byte.
class bit_writer {
public:
    explicit bit_writer(bytes& out) noexcept : out_(out), acc_(0), used_(0) {}

    void put(std::uint32_t bits, unsigned count) {
        acc_ |= bits << used_;
        used_ += count;
        while (used_ >= 8) {
            out_.push_back(static_cast<std::uint8_t>(acc_ & 0xFFu));
            acc_ >>= 8;
            used_ -= 8;
        }
    }

    // Huffman codes are sent most significant bit first.
    void put_code(std::uint32_t code, unsigned count) {
        std::uint32_t reversed = 0;
        for (unsigned i = 0; i < count; ++i) {
            reversed = (reversed << 1) | ((code >> i) & 1u);
        }
        put(reversed, count);
    }

    void flush() {
        if (used_ != 0) {
            out_.push_back(static_cast<std::uint8_t>(acc_ & 0xFFu));
            acc_ = 0;
            used_ = 0;
        }
    }

private:
    bytes& out_;
    std::uint32_t acc_;  // holds at most 7 + 13 bits
    unsigned used_;
};

// The fixed literal/length code of RFC 1951 section 3.2.6.
void put_symbol(bit_writer& w, unsigned symbol) {
    if (symbol < 144) {
        w.put_code(0x30u + symbol, 8);
    } else if (symbol < 256) {
        w.put_code(0x190u + (symbol - 144u), 9);
    } else if (symbol < 280) {
        w.put_code(symbol - 256u, 7);
    } else {
        w.put_code(0xC0u + (symbol - 280u), 8);
    }
}

constexpr unsigned length_base[29] = {3,  4,  5,  6,   7,   8,   9,   10,  11, 13,
                                      15, 17, 19, 23,  27,  31,  35,  43,  51, 59,
                                      67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr unsigned length_extra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                       2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr unsigned dist_base[30] = {1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
                                    33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
                                    1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr unsigned dist_extra[30] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                     6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

void put_match(bit_writer& w, unsigned length, unsigned dist) {
    unsigned li = 28;
    while (length_base[li] > length) {
        --li;
    }
    put_symbol(w, 257u + li);
    w.put(length - length_base[li], length_extra[li]);
    unsigned di = 29;
    while (dist_base[di] > dist) {
        --di;
    }
    w.put_code(di, 5);
    w.put(dist - dist_base[di], dist_extra[di]);
}

void deflate_fixed(const bytes& raw, std::size_t bpp, std::size_t stride, bytes& out) {
    bit_writer w(out);
    w.put(1, 1);  // BFINAL
    w.put(1, 2);  // BTYPE = 01
    const std::size_t n = raw.size();
    const std::size_t distances[2] = {bpp, stride};
    std::size_t i = 0;
    while (i < n) {
        std::size_t best = 0;
        std::size_t dist = 0;
        const std::size_t limit = std::min<std::size_t>(258, n - i);
        for (const std::size_t d : distances) {
            if (i < d) {
                continue;
            }
            std::size_t len = 0;
            while (len < limit && raw[i + len] == raw[i - d + len]) {
                ++len;
            }
            if (len > best) {
                best = len;
                dist = d;
            }
        }
        if (best >= 3) {
            put_match(w, static_cast<unsigned>(best), static_cast<unsigned>(dist));
            i += best;
        } else {
            put_symbol(w, raw[i]);
            ++i;
        }
    }
    put_symbol(w, 256);
    w.flush();
}

void put_be32(bytes& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v >> 24));
    out.push_back(static_cast<std::uint8_t>(v >> 16));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
    out.push_back(static_cast<std::uint8_t>(v));
}

void put_chunk(bytes& out, const char* type, const bytes& data) {
    put_be32(out, static_cast<std::uint32_t>(data.size()));
    const std::size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    put_be32(out, detail::crc32(out.data() + start, out.size() - start));
}

void encode(const detail::pixel_view& img, bytes& out) {
    const std::size_t pixels = static_cast<std::size_t>(img.width) * img.height;
    bool opaque = true;
    for (std::size_t p = 0; p < pixels && opaque; ++p) {
        opaque = img.rgba[4 * p + 3] == 255;
    }
    const std::size_t bpp = opaque ? 3 : 4;
    const std::size_t stride = 1 + static_cast<std::size_t>(img.width) * bpp;

    bytes raw;
    raw.reserve(stride * img.height);
    for (std::uint32_t y = 0; y < img.height; ++y) {
        raw.push_back(0);  // filter type 0
        const std::uint8_t* row = img.rgba + static_cast<std::size_t>(y) * img.width * 4;
        for (std::uint32_t x = 0; x < img.width; ++x) {
            raw.insert(raw.end(), row + 4 * x, row + 4 * x + bpp);
        }
    }

    bytes zlib = {0x78, 0x01};
    deflate_fixed(raw, bpp, stride, zlib);
    put_be32(zlib, detail::adler32(raw.data(), raw.size()));

    const std::uint8_t signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    out.assign(signature, signature + 8);
    bytes ihdr;
    put_be32(ihdr, img.width);
    put_be32(ihdr, img.height);
    ihdr.push_back(8);
    ihdr.push_back(opaque ? 2 : 6);
    ihdr.push_back(0);
    ihdr.push_back(0);
    ihdr.push_back(0);
    put_chunk(out, "IHDR", ihdr);
    put_chunk(out, "sRGB", bytes{0});
    put_chunk(out, "IDAT", zlib);
    put_chunk(out, "IEND", bytes{});
}

}  // namespace

namespace detail {

error_code encode_png_view(pixel_view img, std::vector<std::uint8_t>& out) noexcept {
    out.clear();
    try {
        encode(img, out);
    } catch (...) {
        out.clear();
        return error_code::out_of_memory;
    }
    return error_code::ok;
}

}  // namespace detail

error_code encode_png(const image& img, std::vector<std::uint8_t>& out) noexcept {
    out.clear();
    if (!detail::valid_image(img)) {
        return error_code::invalid_image;
    }
    return detail::encode_png_view(detail::view_of(img), out);
}

}  // namespace hh
