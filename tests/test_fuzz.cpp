// Deterministic pseudo-random robustness loops over every entry point. They assert
// totality: a defined error code for any input, no crash, nothing the sanitizers
// object to. The seeds are fixed, so a failure reproduces.

#include <cstdint>
#include <string>
#include <vector>

#include <hh/hh.h>

#include "derive.hpp"
#include "test_framework.hpp"
#include "test_util.hpp"

using hh::error_code;
using hh_test::prng;

TEST_CASE("fuzz: hexadecimal parsing agrees with a simple oracle") {
    prng random(0xA11CE);
    const char alphabet[] = "0123456789abcdefABCDEFxXgG -:\n";
    int accepted = 0;
    for (int round = 0; round < 4000; ++round) {
        std::string text;
        const std::uint32_t length = random.below(12);
        if (random.below(4) == 0) {
            text = random.below(2) == 0 ? "0x" : "0X";
        }
        for (std::uint32_t i = 0; i < length; ++i) {
            // Mostly hex digits, so that valid strings are common.
            const std::uint32_t pick = random.below(10) < 9 ? random.below(22) : random.below(30);
            text.push_back(alphabet[pick]);
        }
        std::string digits = text;
        if (digits.size() >= 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) {
            digits.erase(0, 2);
        }
        bool valid = !digits.empty() && digits.size() % 2 == 0;
        for (char c : digits) {
            valid = valid &&
                    ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
        }
        // The parser runs before the slow stretch, so only the verdict is compared here;
        // the digest itself is computed for a sample of the valid strings.
        if (!valid || accepted < 8) {
            hh::base_digest digest;
            const error_code ec = hh::make_base_digest_from_hex(text, digest);
            EXPECT_EQ(ec, valid ? error_code::ok : error_code::invalid_hex);
            EXPECT_EQ(digest.empty(), !valid);
            accepted += valid ? 1 : 0;
        }
    }
    EXPECT_TRUE(accepted >= 8);
}

TEST_CASE("fuzz: random fingerprints render with random options") {
    prng random(0xF00D);
    hh::image img;
    int rendered = 0;
    for (int round = 0; round < 1500; ++round) {
        const auto fp_bytes = random.bytes(32);
        const hh::mode m = random.below(2) == 0 ? hh::mode::universal : hh::mode::keyed;
        const hh::fingerprint fp = hh_test::fingerprint_of(fp_bytes, m);
        hh::render_options options;
        options.shape = static_cast<hh::image_shape>(random.below(3));   // 2 is out of range
        options.frame = static_cast<hh::frame_style>(random.below(11));  // 10 is out of range
        const auto colour = random.bytes(5);
        options.background = {colour[0], colour[1], colour[2]};
        options.background_alpha = random.below(3) == 0 ? std::uint8_t{255} : colour[3];
        options.frame_alpha = colour[4];
        const std::uint32_t size =
            random.below(100) == 0 ? random.below(2000) : 8 + random.below(90);
        const error_code ec = hh::render(fp, size, options, img);
        if (ec == error_code::ok) {
            ++rendered;
            EXPECT_EQ(img.width, size);
            EXPECT_EQ(img.rgba.size(), static_cast<std::size_t>(size) * size * 4);
            bool canonical = true;  // a transparent pixel is 00 00 00 00
            for (std::size_t p = 0; p < img.rgba.size() && canonical; p += 4) {
                canonical = img.rgba[p + 3] != 0 ||
                            (img.rgba[p] == 0 && img.rgba[p + 1] == 0 && img.rgba[p + 2] == 0);
            }
            EXPECT_TRUE(canonical);
        } else {
            EXPECT_TRUE(ec == error_code::invalid_size || ec == error_code::invalid_frame ||
                        ec == error_code::low_contrast || ec == error_code::invalid_argument);
            EXPECT_TRUE(img.rgba.empty());
        }
    }
    EXPECT_TRUE(rendered > 200);
}

TEST_CASE("fuzz: encoders take any image") {
    prng random(0xBEEF);
    std::vector<std::uint8_t> out;
    for (int round = 0; round < 300; ++round) {
        hh::image img;
        img.width = 1 + random.below(40);
        img.height = 1 + random.below(40);
        img.rgba = random.bytes(static_cast<std::size_t>(img.width) * img.height * 4);
        if (random.below(3) == 0) {  // flat areas, as real pictures have
            const std::uint8_t v = static_cast<std::uint8_t>(random.below(256));
            for (std::size_t i = 0; i < img.rgba.size() / 2; ++i) {
                img.rgba[i] = v;
            }
        }
        if (random.below(10) == 0) {  // a buffer of the wrong length
            img.rgba.resize(img.rgba.size() - 1 - random.below(3));
        }
        const bool valid = img.rgba.size() == static_cast<std::size_t>(img.width) * img.height * 4;
        const hh::rgb matte = {static_cast<std::uint8_t>(random.below(256)), 0, 255};
        const int quality = static_cast<int>(random.below(130)) - 10;
        EXPECT_EQ(hh::encode_png(img, out), valid ? error_code::ok : error_code::invalid_image);
        EXPECT_EQ(hh::encode_bmp(img, matte, out),
                  valid ? error_code::ok : error_code::invalid_image);
        const error_code jpeg = hh::encode_jpeg(img, quality, matte, out);
        const error_code expected = !valid                            ? error_code::invalid_image
                                    : (quality < 50 || quality > 100) ? error_code::invalid_quality
                                                                      : error_code::ok;
        EXPECT_EQ(jpeg, expected);
        EXPECT_EQ(out.empty(), jpeg != error_code::ok);
    }
}

TEST_CASE("fuzz: the c abi rejects bad arguments and never writes past a buffer") {
    prng random(0xCAFE);
    const auto fp = random.bytes(32);
    for (int round = 0; round < 300; ++round) {
        const std::uint32_t size = 8 + random.below(60);
        const std::size_t needed = hh_render_bytes(size);
        const std::size_t capacity =
            random.below(3) == 0 ? needed : random.below(static_cast<std::uint32_t>(needed + 64));
        std::vector<std::uint8_t> buffer(capacity + 8, 0xC3);
        hh_render_options options;
        hh_render_options_init(&options);
        options.frame = static_cast<std::uint8_t>(random.below(12));
        options.shape = static_cast<std::uint8_t>(random.below(3));
        const int mode = static_cast<int>(random.below(4));
        const int ec = hh_render(fp.data(), mode, size, &options, buffer.data(), capacity);
        EXPECT_TRUE(ec >= HH_OK && ec <= HH_INVALID_ARGUMENT);
        bool guard = true;
        for (std::size_t i = capacity; i < buffer.size(); ++i) {
            guard = guard && buffer[i] == 0xC3;
        }
        EXPECT_TRUE(guard);
        if (ec == HH_OK) {
            EXPECT_TRUE(capacity >= needed && needed != 0);
        }
    }
}

TEST_CASE("fuzz: digests, keys, imports, layout and contrast take any bytes") {
    prng random(0xD16E57);
    // The text and binary digests cost a full stretch each, so their loop is short; they must
    // accept any bytes, including invalid UTF-8 and embedded zeros, and stay separated.
    for (int round = 0; round < 12; ++round) {
        const auto data = random.bytes(1 + random.below(80));
        hh::base_digest binary;
        hh::base_digest text;
        EXPECT_EQ(hh::make_base_digest(hh_test::view(data), binary), error_code::ok);
        EXPECT_EQ(
            hh::make_base_digest_from_text(
                std::string_view(reinterpret_cast<const char*>(data.data()), data.size()), text),
            error_code::ok);
        EXPECT_TRUE(binary != text);
    }
    for (int round = 0; round < 3000; ++round) {
        const auto data = random.bytes(random.below(70));
        hh::base_digest digest;
        hh::secret_key key;
        hh::fingerprint fp;
        const bool is32 = data.size() == 32;
        bool zero = true;
        for (std::uint8_t b : data) {
            zero = zero && b == 0;
        }
        EXPECT_EQ(hh::import_base_digest(hh_test::view(data), digest),
                  is32 ? error_code::ok : error_code::invalid_digest);
        EXPECT_EQ(hh::make_secret_key(hh_test::view(data), key),
                  is32 && !zero ? error_code::ok : error_code::invalid_key);
        const hh::mode m = static_cast<hh::mode>(random.below(4));
        const bool mode_ok = m == hh::mode::universal || m == hh::mode::keyed;
        EXPECT_EQ(hh::import_fingerprint(hh_test::view(data), m, fp),
                  is32 && mode_ok ? error_code::ok : error_code::invalid_fingerprint);
        const hh::layout l = hh::describe(fp);
        for (const hh::cell& c : l.cells) {
            EXPECT_TRUE(static_cast<unsigned>(c.figure) <= 6 && c.colour <= 3);
            EXPECT_TRUE(c.figure != hh::figure::none || c.colour == 0);
        }
        EXPECT_EQ(fp.tag().size(), std::size_t{6});

        hh::render_options options;
        const auto colour = random.bytes(8);
        options.background = {colour[0], colour[1], colour[2]};
        options.background_alpha = colour[3];
        options.frame_alpha = colour[4];
        const hh::contrast_report report =
            hh::measure_contrast(options, {colour[5], colour[6], colour[7]});
        EXPECT_TRUE(report.figures_x100 >= 100 && report.figures_x100 <= 2100);
        EXPECT_TRUE(report.frame_x100 >= 100 && report.frame_x100 <= 2100);
    }
}
