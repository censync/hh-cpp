// Reproduces every record of testdata/vectors.tsv (SPEC.md section 15) through the
// public API. The other implementations run the same file; all must agree on every field.

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "../tools/names.hpp"
#include "sha256.hpp"
#include "test_framework.hpp"
#include "test_util.hpp"

using hh::error_code;
using hh_test::to_hex;
using hh_test::view;

namespace {

using bytes = std::vector<std::uint8_t>;
using record = std::vector<std::string>;

// The directory the build recorded, or HH_TESTDATA_DIR from the environment when the tests
// run somewhere else, for example on a device.
std::string testdata_dir() {
    const char* override_dir = std::getenv("HH_TESTDATA_DIR");
    return override_dir != nullptr ? override_dir : HH_TESTDATA_DIR;
}

std::vector<record> records_of(const std::string& type) {
    std::vector<record> out;
    std::ifstream file(testdata_dir() + "/vectors.tsv", std::ios::binary);
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        record fields;
        std::size_t start = 0;
        for (;;) {
            const std::size_t tab = line.find('\t', start);
            fields.push_back(line.substr(start, tab == std::string::npos ? tab : tab - start));
            if (tab == std::string::npos) {
                break;
            }
            start = tab + 1;
        }
        if (fields[0] == type) {
            out.push_back(fields);
        }
    }
    return out;
}

bytes unhex(const std::string& text) {
    bytes out;
    hh_tools::from_hex(text, out);
    return out;
}

// "hex:<bytes>" or "fill:<byte>:<count>".
bytes input_of(const std::string& field) {
    if (field.rfind("hex:", 0) == 0) {
        return unhex(field.substr(4));
    }
    const std::size_t colon = field.find(':', 5);
    const bytes value = unhex(field.substr(5, colon - 5));
    return bytes(static_cast<std::size_t>(std::strtoull(field.c_str() + colon + 1, nullptr, 10)),
                 value[0]);
}

std::string sha256_hex(const bytes& data) {
    return to_hex(hh::detail::sha256_hash(data.data(), data.size()));
}

std::string cells_text(const hh::fingerprint& fp) {
    std::string out;
    for (const hh::cell& c : hh::describe(fp).cells) {
        out.push_back(static_cast<char>('0' + static_cast<int>(c.figure)));
        out.push_back(static_cast<char>('0' + c.colour));
    }
    return out;
}

// The fields fp, mode, size, shape, frame, background, frame alpha starting at `at`.
struct render_case {
    hh::fingerprint fp;
    std::uint32_t size = 0;
    hh::render_options options;
    bool ok = false;
};

render_case render_case_of(const record& r, std::size_t at) {
    render_case out;
    const hh::mode m = r[at + 1] == "keyed" ? hh::mode::keyed : hh::mode::universal;
    out.ok = hh::import_fingerprint(view(unhex(r[at])), m, out.fp) == error_code::ok;
    out.size = static_cast<std::uint32_t>(std::strtoul(r[at + 2].c_str(), nullptr, 10));
    out.ok = out.ok && hh_tools::parse_shape(r[at + 3], out.options.shape) &&
             hh_tools::parse_frame(r[at + 4], out.options.frame) &&
             hh_tools::parse_rgba(r[at + 5], out.options.background, out.options.background_alpha);
    out.options.frame_alpha =
        static_cast<std::uint8_t>(std::strtoul(r[at + 6].c_str(), nullptr, 10));
    return out;
}

}  // namespace

TEST_CASE("vectors: the file is present and complete") {
    EXPECT_TRUE(records_of("D").size() >= 20);
    EXPECT_TRUE(records_of("H").size() >= 20);
    EXPECT_TRUE(records_of("K").size() >= 8);
    EXPECT_TRUE(records_of("C").size() >= 20);
    EXPECT_TRUE(records_of("R").size() >= 80);
    EXPECT_TRUE(records_of("E").size() >= 25);
    EXPECT_TRUE(records_of("G").size() >= 20);
    EXPECT_TRUE(records_of("W").size() >= 15);
    EXPECT_TRUE(records_of("I").size() >= 14);
    EXPECT_TRUE(records_of("F").size() >= 16);
}

TEST_CASE("vectors: derivation records") {
    for (const record& r : records_of("D")) {
        EXPECT_EQ(r.size(), std::size_t{16});
        const bytes data = input_of(r[3]);
        hh::base_digest digest;
        const error_code ec =
            r[2] == "text"
                ? hh::make_base_digest_from_text(
                      std::string_view(reinterpret_cast<const char*>(data.data()), data.size()),
                      digest)
                : hh::make_base_digest(view(data), digest);
        EXPECT_EQ(ec, error_code::ok);
        EXPECT_EQ(to_hex(digest.bytes()), r[7]);
        hh::fingerprint universal;
        EXPECT_EQ(hh::universal_fingerprint(digest, universal), error_code::ok);
        EXPECT_EQ(to_hex(universal.bytes()), r[9]);
        EXPECT_EQ(cells_text(universal), r[12]);
        EXPECT_EQ(universal.tag(), r[14]);
        if (r[5] != "-") {  // M1 and d0 = SHA-256(M1), checked without the library's derivation
            EXPECT_EQ(sha256_hex(unhex(r[5])), r[6]);
        }
        if (r[4] == "-") {
            EXPECT_EQ(r[10], std::string{"-"});
            continue;
        }
        hh::secret_key key;
        EXPECT_EQ(hh::make_secret_key(view(unhex(r[4])), key), error_code::ok);
        EXPECT_EQ(to_hex(key.kcv()), r[11]);
        hh::fingerprint keyed;
        EXPECT_EQ(hh::keyed_fingerprint(digest, key, keyed), error_code::ok);
        EXPECT_EQ(to_hex(keyed.bytes()), r[10]);
        EXPECT_EQ(cells_text(keyed), r[13]);
        EXPECT_EQ(keyed.tag(), r[15]);
    }
}

TEST_CASE("vectors: hexadecimal input records") {
    for (const record& r : records_of("H")) {
        const bytes text = unhex(r[2]);
        const std::string_view string(reinterpret_cast<const char*>(text.data()), text.size());
        hh::base_digest digest;
        const error_code ec = hh::make_base_digest_from_hex(string, digest);
        bytes decoded;
        if (hh_tools::from_hex(r[3], decoded) && !r[3].empty()) {
            EXPECT_EQ(ec, error_code::ok);
            hh::base_digest expected;
            EXPECT_EQ(hh::make_base_digest(view(decoded), expected), error_code::ok);
            EXPECT_TRUE(digest == expected);
        } else {
            EXPECT_EQ(std::string{hh::error_name(ec)}, r[3]);
        }
    }
}

TEST_CASE("vectors: key records") {
    for (const record& r : records_of("K")) {
        hh::secret_key key;
        const bytes raw = r[2] == "-" ? bytes{} : unhex(r[2]);
        const error_code ec = hh::make_secret_key(view(raw), key);
        EXPECT_EQ(ec == error_code::ok ? to_hex(key.kcv()) : std::string{hh::error_name(ec)}, r[3]);
    }
}

TEST_CASE("vectors: contrast records") {
    for (const record& r : records_of("C")) {
        hh::render_options options;
        hh::rgb page{};
        EXPECT_TRUE(hh_tools::parse_rgba(r[2], options.background, options.background_alpha));
        options.frame_alpha = static_cast<std::uint8_t>(std::strtoul(r[3].c_str(), nullptr, 10));
        EXPECT_TRUE(hh_tools::parse_rgb(r[4], page));
        const hh::contrast_report report = hh::measure_contrast(options, page);
        EXPECT_EQ(std::to_string(report.figures_x100), r[5]);
        EXPECT_EQ(std::to_string(report.frame_x100), r[6]);
    }
}

TEST_CASE("vectors: render records") {
    for (const record& r : records_of("R")) {
        EXPECT_EQ(r.size(), std::size_t{15});
        const render_case c = render_case_of(r, 2);
        EXPECT_TRUE(c.ok);
        hh::rgb matte{};
        EXPECT_TRUE(hh_tools::parse_rgb(r[10], matte));
        const int quality = std::atoi(r[9].c_str());
        hh::image img;
        bytes png, bmp, jpeg;
        EXPECT_EQ(hh::render(c.fp, c.size, c.options, img), error_code::ok);
        EXPECT_EQ(hh::encode_png(img, png), error_code::ok);
        EXPECT_EQ(hh::encode_bmp(img, matte, bmp), error_code::ok);
        EXPECT_EQ(hh::encode_jpeg(img, quality, matte, jpeg), error_code::ok);
        EXPECT_EQ(sha256_hex(img.rgba) + " " + r[1], r[11] + " " + r[1]);
        EXPECT_EQ(sha256_hex(png) + " " + r[1], r[12] + " " + r[1]);
        EXPECT_EQ(sha256_hex(bmp) + " " + r[1], r[13] + " " + r[1]);
        EXPECT_EQ(sha256_hex(jpeg) + " " + r[1], r[14] + " " + r[1]);
    }
}

TEST_CASE("vectors: render error records") {
    for (const record& r : records_of("E")) {
        const render_case c = render_case_of(r, 2);
        EXPECT_TRUE(c.ok);
        hh::image img;
        EXPECT_EQ(
            std::string{hh::error_name(hh::render(c.fp, c.size, c.options, img))} + " " + r[1],
            r[9] + " " + r[1]);
    }
}

TEST_CASE("vectors: size sweep records") {
    for (const record& r : records_of("W")) {
        EXPECT_EQ(r.size(), std::size_t{11});
        // fp, mode, shape, frame, background, frame alpha, first, last: reuse the render parser
        // with the size column filled in.
        record as_render = {r[2], r[3], r[8], r[4], r[5], r[6], r[7]};
        const render_case c = render_case_of(as_render, 0);
        EXPECT_TRUE(c.ok);
        const auto last = static_cast<std::uint32_t>(std::strtoul(r[9].c_str(), nullptr, 10));
        hh::detail::sha256 hasher;
        hh::image img;
        for (std::uint32_t size = c.size; size <= last; ++size) {
            EXPECT_EQ(hh::render(c.fp, size, c.options, img), error_code::ok);
            hasher.update(img.rgba.data(), img.rgba.size());
        }
        EXPECT_EQ(to_hex(hasher.finish()) + " " + r[1], r[10] + " " + r[1]);
    }
}

TEST_CASE("vectors: image records") {
    for (const record& r : records_of("I")) {
        EXPECT_EQ(r.size(), std::size_t{11});
        hh::image img;
        hh::rgb matte{};
        EXPECT_TRUE(
            hh_tools::pattern_image(r[4], static_cast<std::uint32_t>(std::atoi(r[2].c_str())),
                                    static_cast<std::uint32_t>(std::atoi(r[3].c_str())), img));
        EXPECT_TRUE(hh_tools::parse_rgb(r[6], matte));
        bytes png, bmp, jpeg;
        EXPECT_EQ(hh::encode_png(img, png), error_code::ok);
        EXPECT_EQ(hh::encode_bmp(img, matte, bmp), error_code::ok);
        EXPECT_EQ(hh::encode_jpeg(img, std::atoi(r[5].c_str()), matte, jpeg), error_code::ok);
        EXPECT_EQ(sha256_hex(img.rgba) + " " + r[1], r[7] + " " + r[1]);
        EXPECT_EQ(sha256_hex(png) + " " + r[1], r[8] + " " + r[1]);
        EXPECT_EQ(sha256_hex(bmp) + " " + r[1], r[9] + " " + r[1]);
        EXPECT_EQ(sha256_hex(jpeg) + " " + r[1], r[10] + " " + r[1]);
    }
}

TEST_CASE("vectors: failure records") {
    for (const record& r : records_of("F")) {
        std::string got;
        if (r[2] == "digest") {
            const bytes data = input_of(r[4]);
            hh::base_digest digest;
            const error_code ec =
                r[3] == "text"
                    ? hh::make_base_digest_from_text(
                          std::string_view(reinterpret_cast<const char*>(data.data()), data.size()),
                          digest)
                    : hh::make_base_digest(view(data), digest);
            got = hh::error_name(ec);
        } else if (r[2] == "jpeg") {
            hh::image img;
            hh_tools::pattern_image("flat:ffffffff", 8, 8, img);
            bytes out;
            got =
                hh::error_name(hh::encode_jpeg(img, std::atoi(r[3].c_str()), {255, 255, 255}, out));
        } else if (r[2] == "image") {
            hh::image img;
            img.width = static_cast<std::uint32_t>(std::atoi(r[3].c_str()));
            img.height = static_cast<std::uint32_t>(std::atoi(r[4].c_str()));
            img.rgba.assign(static_cast<std::size_t>(std::atoi(r[5].c_str())), 0x7F);
            bytes out;
            got = hh::error_name(hh::encode_png(img, out));
            EXPECT_EQ(got, std::string{hh::error_name(hh::encode_bmp(img, {0, 0, 0}, out))});
            EXPECT_EQ(got, std::string{hh::error_name(hh::encode_jpeg(img, 92, {0, 0, 0}, out))});
        }
        EXPECT_EQ(got + " " + r[1], r.back() + " " + r[1]);
    }
}

TEST_CASE("vectors: golden files") {
    for (const record& r : records_of("G")) {
        const render_case c = render_case_of(r, 3);
        EXPECT_TRUE(c.ok);
        hh::image img;
        bytes png;
        EXPECT_EQ(hh::render(c.fp, c.size, c.options, img), error_code::ok);
        EXPECT_EQ(hh::encode_png(img, png), error_code::ok);
        std::ifstream file(testdata_dir() + "/golden/" + r[2], std::ios::binary);
        const bytes expected((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
        EXPECT_FALSE(expected.empty());
        EXPECT_TRUE(png == expected);
    }
}
