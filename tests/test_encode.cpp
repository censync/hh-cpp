#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "checksum.hpp"
#include "jpeg_tables.hpp"
#include "test_framework.hpp"
#include "test_util.hpp"

using hh::error_code;
using hh_test::bytes_of;
using hh_test::fingerprint_of;

namespace {

using bytes = std::vector<std::uint8_t>;

std::uint32_t be32(const bytes& b, std::size_t pos) {
    return (std::uint32_t(b[pos]) << 24) | (std::uint32_t(b[pos + 1]) << 16) |
           (std::uint32_t(b[pos + 2]) << 8) | std::uint32_t(b[pos + 3]);
}

std::uint32_t le32(const bytes& b, std::size_t pos) {
    return std::uint32_t(b[pos]) | (std::uint32_t(b[pos + 1]) << 8) |
           (std::uint32_t(b[pos + 2]) << 16) | (std::uint32_t(b[pos + 3]) << 24);
}

struct chunk {
    std::string type;
    bytes data;
};

// Splits a PNG into chunks and checks the signature and every CRC.
bool read_png(const bytes& png, std::vector<chunk>& chunks) {
    const std::uint8_t signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (png.size() < 8 || !std::equal(signature, signature + 8, png.begin())) {
        return false;
    }
    std::size_t pos = 8;
    while (pos + 12 <= png.size()) {
        const std::uint32_t length = be32(png, pos);
        if (pos + 12 + length > png.size()) {
            return false;
        }
        chunk c;
        c.type.assign(png.begin() + static_cast<std::ptrdiff_t>(pos + 4),
                      png.begin() + static_cast<std::ptrdiff_t>(pos + 8));
        c.data.assign(png.begin() + static_cast<std::ptrdiff_t>(pos + 8),
                      png.begin() + static_cast<std::ptrdiff_t>(pos + 8 + length));
        if (hh::detail::crc32(png.data() + pos + 4, 4 + length) != be32(png, pos + 8 + length)) {
            return false;
        }
        chunks.push_back(c);
        pos += 12 + length;
    }
    return pos == png.size();
}

// An independent inflater for one fixed-Huffman block, enough to check the encoder.
class inflater {
public:
    explicit inflater(const bytes& in, std::size_t pos) : in_(in), pos_(pos), bit_(0) {}

    bool run(bytes& out) {
        if (bits(1) != 1 || bits(2) != 1) {
            return false;
        }
        static const unsigned length_base[29] = {3,  4,  5,  6,   7,   8,   9,   10,  11, 13,
                                                 15, 17, 19, 23,  27,  31,  35,  43,  51, 59,
                                                 67, 83, 99, 115, 131, 163, 195, 227, 258};
        static const unsigned length_extra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                                  2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
        static const unsigned dist_base[30] = {
            1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
            193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
        static const unsigned dist_extra[30] = {0, 0, 0,  0,  1,  1,  2,  2,  3,  3,
                                                4, 4, 5,  5,  6,  6,  7,  7,  8,  8,
                                                9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
        for (;;) {
            unsigned code = 0;
            for (int i = 0; i < 7; ++i) {
                code = (code << 1) | bits(1);
            }
            unsigned symbol;
            if (code <= 0x17) {
                symbol = 256 + code;
            } else {
                code = (code << 1) | bits(1);
                if (code >= 0x30 && code <= 0xBF) {
                    symbol = code - 0x30;
                } else if (code >= 0xC0 && code <= 0xC7) {
                    symbol = 280 + code - 0xC0;
                } else {
                    code = (code << 1) | bits(1);
                    symbol = 144 + code - 0x190;
                }
            }
            if (symbol < 256) {
                out.push_back(static_cast<std::uint8_t>(symbol));
            } else if (symbol == 256) {
                return true;
            } else {
                const unsigned li = symbol - 257;
                if (li >= 29) {
                    return false;
                }
                const unsigned length = length_base[li] + bits(length_extra[li]);
                unsigned dcode = 0;
                for (int i = 0; i < 5; ++i) {
                    dcode = (dcode << 1) | bits(1);
                }
                if (dcode >= 30) {
                    return false;
                }
                const unsigned dist = dist_base[dcode] + bits(dist_extra[dcode]);
                if (dist > out.size()) {
                    return false;
                }
                for (unsigned k = 0; k < length; ++k) {
                    out.push_back(out[out.size() - dist]);
                }
            }
        }
    }

    std::size_t next_byte() const { return bit_ == 0 ? pos_ : pos_ + 1; }

private:
    unsigned bits(unsigned count) {
        unsigned v = 0;
        for (unsigned i = 0; i < count; ++i) {
            const unsigned b = pos_ < in_.size() ? (unsigned{in_[pos_]} >> bit_) & 1u : 0u;
            v |= b << i;
            if (++bit_ == 8) {
                bit_ = 0;
                ++pos_;
            }
        }
        return v;
    }

    const bytes& in_;
    std::size_t pos_;
    unsigned bit_;
};

hh::image sample(std::uint32_t size, hh::mode m, hh::render_options options = {}) {
    std::vector<std::uint8_t> fp(32);
    for (std::size_t i = 0; i < fp.size(); ++i) {
        fp[i] = static_cast<std::uint8_t>(i * 37 + 11);
    }
    hh::image img;
    hh::render(fingerprint_of(fp, m), size, options, img);
    return img;
}

// Checks that a PNG decodes back to the pixels it was made from.
bool png_round_trip(const hh::image& img) {
    bytes png;
    if (hh::encode_png(img, png) != error_code::ok) {
        return false;
    }
    std::vector<chunk> chunks;
    if (!read_png(png, chunks) || chunks.size() != 4 || chunks[0].type != "IHDR" ||
        chunks[1].type != "sRGB" || chunks[2].type != "IDAT" || chunks[3].type != "IEND") {
        return false;
    }
    const bytes& ihdr = chunks[0].data;
    if (ihdr.size() != 13 || be32(ihdr, 0) != img.width || be32(ihdr, 4) != img.height ||
        ihdr[8] != 8 || ihdr[10] != 0 || ihdr[11] != 0 || ihdr[12] != 0) {
        return false;
    }
    bool opaque = true;
    for (std::size_t p = 3; p < img.rgba.size(); p += 4) {
        opaque = opaque && img.rgba[p] == 255;
    }
    if (ihdr[9] != (opaque ? 2 : 6) || chunks[1].data != bytes{0} || !chunks[3].data.empty()) {
        return false;
    }
    const bytes& z = chunks[2].data;
    if (z.size() < 6 || z[0] != 0x78 || z[1] != 0x01) {
        return false;
    }
    inflater inf(z, 2);
    bytes raw;
    if (!inf.run(raw) || inf.next_byte() + 4 != z.size() ||
        be32(z, z.size() - 4) != hh::detail::adler32(raw.data(), raw.size())) {
        return false;
    }
    const std::size_t bpp = opaque ? 3 : 4;
    if (raw.size() != (1 + img.width * bpp) * img.height) {
        return false;
    }
    for (std::uint32_t y = 0; y < img.height; ++y) {
        const std::uint8_t* row = raw.data() + y * (1 + img.width * bpp);
        if (row[0] != 0) {
            return false;
        }
        for (std::uint32_t x = 0; x < img.width; ++x) {
            if (!std::equal(row + 1 + x * bpp, row + 1 + (x + 1) * bpp,
                            img.rgba.data() + (static_cast<std::size_t>(y) * img.width + x) * 4)) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace

TEST_CASE("checksum: crc-32 and adler-32 check values") {
    const auto check = bytes_of("123456789");
    EXPECT_EQ(hh::detail::crc32(check.data(), check.size()), 0xCBF43926u);
    EXPECT_EQ(hh::detail::crc32(nullptr, 0), 0u);
    const auto iend = bytes_of("IEND");
    EXPECT_EQ(hh::detail::crc32(iend.data(), iend.size()), 0xAE426082u);
    // Continuing a CRC equals hashing the concatenation.
    EXPECT_EQ(hh::detail::crc32(check.data() + 4, 5, hh::detail::crc32(check.data(), 4)),
              0xCBF43926u);

    const auto wikipedia = bytes_of("Wikipedia");
    EXPECT_EQ(hh::detail::adler32(wikipedia.data(), wikipedia.size()), 0x11E60398u);
    EXPECT_EQ(hh::detail::adler32(nullptr, 0), 1u);
    // More than one 5552-byte block; the value is what zlib gives for 20000 bytes of FF.
    const bytes ones(20000, 0xFF);
    EXPECT_EQ(hh::detail::adler32(ones.data(), ones.size()), 0x9F51D664u);
}

TEST_CASE("png: round trip through an independent inflater") {
    EXPECT_TRUE(png_round_trip(sample(16, hh::mode::universal)));
    EXPECT_TRUE(png_round_trip(sample(33, hh::mode::universal)));
    EXPECT_TRUE(png_round_trip(sample(128, hh::mode::keyed)));  // rounded corners: alpha
    hh::render_options options;
    options.shape = hh::image_shape::round;
    options.frame = hh::frame_style::gaps;
    options.background = {0x12, 0x12, 0x12};
    options.background_alpha = 200;
    options.frame_alpha = 100;
    EXPECT_TRUE(png_round_trip(sample(77, hh::mode::keyed, options)));
}

TEST_CASE("png: arbitrary images, including ones without any repetition") {
    hh_test::prng random(2026);
    for (std::uint32_t width : {1u, 2u, 3u, 7u, 64u}) {
        for (std::uint32_t height : {1u, 5u, 9u}) {
            hh::image img;
            img.width = width;
            img.height = height;
            img.rgba = random.bytes(static_cast<std::size_t>(width) * height * 4);
            EXPECT_TRUE(png_round_trip(img));
            for (std::size_t p = 3; p < img.rgba.size(); p += 4) {
                img.rgba[p] = 255;
            }
            EXPECT_TRUE(png_round_trip(img));
        }
    }
    hh::image flat;  // one long run: matches of the maximum length 258
    flat.width = 300;
    flat.height = 3;
    flat.rgba.assign(300 * 3 * 4, 0x55);
    EXPECT_TRUE(png_round_trip(flat));
}

TEST_CASE("png: flat figures compress well") {
    const hh::image img = sample(128, hh::mode::universal);
    bytes png;
    EXPECT_EQ(hh::encode_png(img, png), error_code::ok);
    EXPECT_TRUE(png.size() * 8 < img.rgba.size());
}

TEST_CASE("bmp: header and pixels") {
    const hh::image img = sample(33, hh::mode::keyed);  // 33 * 3 = 99: one padding byte per row
    bytes bmp;
    EXPECT_EQ(hh::encode_bmp(img, {255, 255, 255}, bmp), error_code::ok);
    const std::uint32_t row = 100;
    EXPECT_EQ(bmp.size(), std::size_t{54 + row * 33});
    EXPECT_TRUE(bmp[0] == 'B' && bmp[1] == 'M');
    EXPECT_EQ(le32(bmp, 2), 54u + row * 33u);
    EXPECT_EQ(le32(bmp, 10), 54u);
    EXPECT_EQ(le32(bmp, 14), 40u);
    EXPECT_EQ(le32(bmp, 18), 33u);
    EXPECT_EQ(le32(bmp, 22), 33u);
    EXPECT_EQ(le32(bmp, 26), 1u | (24u << 16));
    EXPECT_EQ(le32(bmp, 30), 0u);
    EXPECT_EQ(le32(bmp, 34), row * 33u);
    EXPECT_EQ(le32(bmp, 38), 2835u);
    bool pixels_match = true;
    for (std::uint32_t y = 0; y < 33; ++y) {
        const std::uint8_t* line = bmp.data() + 54 + (32 - y) * row;  // bottom-up
        for (std::uint32_t x = 0; x < 33; ++x) {
            const std::uint8_t* p = img.rgba.data() + (static_cast<std::size_t>(y) * 33 + x) * 4;
            for (std::uint32_t c = 0; c < 3; ++c) {
                const unsigned flat = (unsigned{p[3]} * p[c] + (255u - p[3]) * 255u + 127u) / 255u;
                pixels_match = pixels_match && line[3 * x + (2 - c)] == flat;
            }
        }
        pixels_match = pixels_match && line[99] == 0;
    }
    EXPECT_TRUE(pixels_match);
}

TEST_CASE("bmp: the matte shows through transparent pixels") {
    hh::image img;
    img.width = 1;
    img.height = 1;
    img.rgba = {10, 20, 30, 0};
    bytes bmp;
    EXPECT_EQ(hh::encode_bmp(img, {1, 2, 3}, bmp), error_code::ok);
    EXPECT_TRUE(bmp[54] == 3 && bmp[55] == 2 && bmp[56] == 1 && bmp[57] == 0);
    img.rgba[3] = 255;
    EXPECT_EQ(hh::encode_bmp(img, {1, 2, 3}, bmp), error_code::ok);
    EXPECT_TRUE(bmp[54] == 30 && bmp[55] == 20 && bmp[56] == 10);
}

TEST_CASE("jpeg: marker structure") {
    const hh::image img = sample(50, hh::mode::universal);  // not a multiple of 8
    bytes jpeg;
    EXPECT_EQ(hh::encode_jpeg(img, 92, {255, 255, 255}, jpeg), error_code::ok);
    EXPECT_TRUE(jpeg.size() > 700);
    EXPECT_TRUE(jpeg[0] == 0xFF && jpeg[1] == 0xD8);
    EXPECT_TRUE(jpeg[jpeg.size() - 2] == 0xFF && jpeg[jpeg.size() - 1] == 0xD9);
    const std::uint8_t expected[] = {0xE0, 0xDB, 0xDB, 0xC0, 0xC4, 0xC4, 0xC4, 0xC4, 0xDA};
    std::size_t pos = 2;
    for (std::uint8_t marker : expected) {
        EXPECT_TRUE(pos + 4 <= jpeg.size() && jpeg[pos] == 0xFF && jpeg[pos + 1] == marker);
        const std::size_t length = (std::size_t{jpeg[pos + 2]} << 8) | jpeg[pos + 3];
        if (marker == 0xC0) {
            EXPECT_TRUE(jpeg[pos + 4] == 8 && jpeg[pos + 5] == 0 && jpeg[pos + 6] == 50 &&
                        jpeg[pos + 7] == 0 && jpeg[pos + 8] == 50 && jpeg[pos + 9] == 3);
        }
        pos += 2 + length;
    }
    // In the entropy-coded data every FF is followed by 00 (there are no restart markers).
    bool stuffed = true;
    for (std::size_t i = pos; i + 2 < jpeg.size(); ++i) {
        stuffed = stuffed && (jpeg[i] != 0xFF || jpeg[i + 1] == 0x00);
    }
    EXPECT_TRUE(stuffed);
}

TEST_CASE("jpeg: quality scales the quantiser tables") {
    const hh::image img = sample(16, hh::mode::universal);
    bytes best;
    bytes worst;
    EXPECT_EQ(hh::encode_jpeg(img, 100, {255, 255, 255}, best), error_code::ok);
    EXPECT_EQ(hh::encode_jpeg(img, 50, {255, 255, 255}, worst), error_code::ok);
    // The first DQT starts at offset 20: FF DB 00 43 00, then 64 values in zigzag order.
    bool all_ones = true;
    bool base_table = true;
    for (std::size_t i = 0; i < 64; ++i) {
        all_ones = all_ones && best[25 + i] == 1;
        base_table =
            base_table &&
            worst[25 + i] == hh::detail::jpeg_luminance_quantiser[hh::detail::jpeg_zigzag[i]];
    }
    EXPECT_TRUE(all_ones);
    EXPECT_TRUE(base_table);
    EXPECT_TRUE(best.size() > worst.size());
}

TEST_CASE("jpeg: the tables are complete") {
    const hh::detail::jpeg_huffman_spec* specs[] = {
        &hh::detail::jpeg_dc_luminance, &hh::detail::jpeg_dc_chrominance,
        &hh::detail::jpeg_ac_luminance, &hh::detail::jpeg_ac_chrominance};
    for (const auto* spec : specs) {
        unsigned total = 0;
        for (std::uint8_t count : spec->counts) {
            total += count;
        }
        EXPECT_EQ(total, static_cast<unsigned>(spec->symbol_count));
        std::vector<bool> seen(256, false);
        bool unique = true;
        for (std::size_t i = 0; i < spec->symbol_count; ++i) {
            unique = unique && !seen[spec->symbols[i]];
            seen[spec->symbols[i]] = true;
        }
        EXPECT_TRUE(unique);
    }
    // The AC tables hold every run/size pair with size 1..10, plus end-of-block and ZRL.
    for (const auto* spec : {&hh::detail::jpeg_ac_luminance, &hh::detail::jpeg_ac_chrominance}) {
        std::vector<bool> seen(256, false);
        for (std::size_t i = 0; i < spec->symbol_count; ++i) {
            seen[spec->symbols[i]] = true;
        }
        bool complete = seen[0x00] && seen[0xF0];
        for (unsigned run = 0; run < 16; ++run) {
            for (unsigned size = 1; size <= 10; ++size) {
                complete = complete && seen[run * 16 + size];
            }
        }
        EXPECT_TRUE(complete);
    }
    std::vector<bool> seen(64, false);
    for (std::uint8_t k : hh::detail::jpeg_zigzag) {
        seen[k] = true;
    }
    EXPECT_TRUE(std::find(seen.begin(), seen.end(), false) == seen.end());
}

TEST_CASE("jpeg: extreme images stay encodable") {
    hh_test::prng random(7);
    hh::image noise;
    noise.width = 24;
    noise.height = 17;
    noise.rgba = random.bytes(24 * 17 * 4);
    hh::image checker = noise;  // the largest AC coefficients: alternate black and white
    for (std::uint32_t y = 0; y < checker.height; ++y) {
        for (std::uint32_t x = 0; x < checker.width; ++x) {
            const std::uint8_t v = ((x + y) % 2 == 0) ? 0 : 255;
            std::uint8_t* p =
                checker.rgba.data() + (static_cast<std::size_t>(y) * checker.width + x) * 4;
            p[0] = p[1] = p[2] = v;
            p[3] = 255;
        }
    }
    bytes out;
    for (int quality : {50, 75, 92, 100}) {
        EXPECT_EQ(hh::encode_jpeg(noise, quality, {0, 0, 0}, out), error_code::ok);
        EXPECT_EQ(hh::encode_jpeg(checker, quality, {0, 0, 0}, out), error_code::ok);
    }
}

TEST_CASE("encoders: invalid arguments") {
    bytes out = {1, 2, 3};
    hh::image img;
    EXPECT_EQ(hh::encode_png(img, out), error_code::invalid_image);
    EXPECT_TRUE(out.empty());
    img.width = 2;
    img.height = 2;
    img.rgba.assign(15, 0);  // one byte short
    EXPECT_EQ(hh::encode_png(img, out), error_code::invalid_image);
    EXPECT_EQ(hh::encode_bmp(img, {0, 0, 0}, out), error_code::invalid_image);
    EXPECT_EQ(hh::encode_jpeg(img, 92, {0, 0, 0}, out), error_code::invalid_image);
    img.rgba.assign(16, 0);
    EXPECT_EQ(hh::encode_jpeg(img, 49, {0, 0, 0}, out), error_code::invalid_quality);
    EXPECT_EQ(hh::encode_jpeg(img, 101, {0, 0, 0}, out), error_code::invalid_quality);
    EXPECT_EQ(hh::encode_jpeg(img, 50, {0, 0, 0}, out), error_code::ok);
    img.width = hh::max_encoded_dimension + 1;
    EXPECT_EQ(hh::encode_png(img, out), error_code::invalid_image);
}

TEST_CASE("encoders: the same image gives the same bytes") {
    const hh::image img = sample(64, hh::mode::keyed);
    bytes a;
    bytes b;
    EXPECT_EQ(hh::encode_png(img, a), error_code::ok);
    EXPECT_EQ(hh::encode_png(img, b), error_code::ok);
    EXPECT_TRUE(a == b);
    EXPECT_EQ(hh::encode_jpeg(img, 92, {255, 255, 255}, a), error_code::ok);
    EXPECT_EQ(hh::encode_jpeg(img, 92, {255, 255, 255}, b), error_code::ok);
    EXPECT_TRUE(a == b);
}
