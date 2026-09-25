// gen_vectors: writes the golden vectors of SPEC.md section 15 from the reference
// implementation.
//
//   gen_vectors <dir>            writes <dir>/vectors.tsv and <dir>/golden/*.png
//   gen_vectors --check <dir>    regenerates in memory and compares, byte for byte
//
// The vectors are frozen with the algorithm: once released they are only ever
// checked, never regenerated into something different.

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <hh/hh.hpp>

#include "derive.hpp"
#include "features.hpp"
#include "names.hpp"
#include "sha256.hpp"

namespace {

using bytes = std::vector<std::uint8_t>;
using hh_tools::to_hex;

std::ostringstream tsv;
std::map<std::string, bytes> golden;
int failures = 0;

void fail(const std::string& what) {
    std::cerr << "gen_vectors: " << what << "\n";
    ++failures;
}

bytes hex(const std::string& text) {
    bytes out;
    if (!hh_tools::from_hex(text, out)) {
        fail("bad hex literal " + text);
    }
    return out;
}

bytes ascii(const std::string& text) {
    return bytes(text.begin(), text.end());
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

bytes key_1() {
    bytes k(32);
    for (std::size_t i = 0; i < k.size(); ++i) {
        k[i] = static_cast<std::uint8_t>(i);
    }
    return k;
}

bytes key_2() {
    return bytes(32, 0xFF);
}

// ---- D: derivation ---------------------------------------------------------

struct derived {
    hh::fingerprint universal;
    hh::fingerprint keyed;
};

derived derive_record(const std::string& id, bool text, const std::string& input_field,
                      const bytes& data, const bytes& key) {
    derived out;
    const auto kind = text ? hh::detail::input_kind::text : hh::detail::input_kind::binary;
    hh::base_digest digest;
    const hh::error_code ec =
        text
            ? hh::make_base_digest_from_text(
                  std::string_view(reinterpret_cast<const char*>(data.data()), data.size()), digest)
            : hh::make_base_digest({data.data(), data.size()}, digest);
    if (ec != hh::error_code::ok) {
        fail(id + ": " + hh::error_name(ec));
        return out;
    }
    const auto header = hh::detail::m1_header(kind, static_cast<std::uint32_t>(data.size()));
    std::string m1 = "-";
    if (header.size() + data.size() <= 256) {
        m1 = to_hex(header) + to_hex(data);
    }
    const auto d0 = hh::detail::derive_d0(kind, data.data(), data.size());
    const auto m2 = hh::detail::m2_message(digest.bytes().data());
    hh::universal_fingerprint(digest, out.universal);

    std::string key_field = "-", keyed_fp = "-", kcv = "-", keyed_cells = "-", keyed_tag = "-";
    if (!key.empty()) {
        hh::secret_key k;
        if (hh::make_secret_key({key.data(), key.size()}, k) != hh::error_code::ok ||
            hh::keyed_fingerprint(digest, k, out.keyed) != hh::error_code::ok) {
            fail(id + ": key");
            return out;
        }
        key_field = to_hex(key);
        keyed_fp = to_hex(out.keyed.bytes());
        kcv = to_hex(k.kcv());
        keyed_cells = cells_text(out.keyed);
        keyed_tag = out.keyed.tag();
    }
    tsv << "D\t" << id << "\t" << (text ? "text" : "binary") << "\t" << input_field << "\t"
        << key_field << "\t" << m1 << "\t" << to_hex(d0) << "\t" << to_hex(digest.bytes()) << "\t"
        << to_hex(m2) << "\t" << to_hex(out.universal.bytes()) << "\t" << keyed_fp << "\t" << kcv
        << "\t" << cells_text(out.universal) << "\t" << keyed_cells << "\t" << out.universal.tag()
        << "\t" << keyed_tag << "\n";
    return out;
}

derived derive_bytes(const std::string& id, bool text, const bytes& data, const bytes& key) {
    return derive_record(id, text, "hex:" + to_hex(data), data, key);
}

derived derive_fill(const std::string& id, std::uint8_t value, std::size_t count,
                    const bytes& key) {
    const std::uint8_t one[1] = {value};
    return derive_record(id, false, "fill:" + to_hex(one, 1) + ":" + std::to_string(count),
                         bytes(count, value), key);
}

// ---- R, E, G: renders ------------------------------------------------------

struct look {
    hh::image_shape shape = hh::image_shape::square;
    hh::frame_style frame = hh::frame_style::automatic;
    std::string background = "ffffffff";
    unsigned frame_alpha = 255;
    int quality = hh::default_jpeg_quality;
    std::string matte = "ffffff";
};

hh::render_options options_of(const look& l) {
    hh::render_options o;
    o.shape = l.shape;
    o.frame = l.frame;
    hh_tools::parse_rgba(l.background, o.background, o.background_alpha);
    o.frame_alpha = static_cast<std::uint8_t>(l.frame_alpha);
    return o;
}

std::string look_fields(const hh::fingerprint& fp, std::uint32_t size, const look& l) {
    std::ostringstream out;
    out << to_hex(fp.bytes()) << "\t" << hh_tools::mode_name(fp.mode()) << "\t" << size << "\t"
        << hh_tools::shape_name(l.shape) << "\t" << hh_tools::frame_name(l.frame) << "\t"
        << l.background << "\t" << l.frame_alpha;
    return out.str();
}

int render_counter = 0;

void render_record(const hh::fingerprint& fp, std::uint32_t size, const look& l) {
    const std::string id = "r" + std::to_string(++render_counter);
    hh::image img;
    bytes png, bmp, jpeg;
    hh::rgb matte{};
    hh_tools::parse_rgb(l.matte, matte);
    hh::error_code ec = hh::render(fp, size, options_of(l), img);
    if (ec == hh::error_code::ok) {
        ec = hh::encode_png(img, png);
    }
    if (ec == hh::error_code::ok) {
        ec = hh::encode_bmp(img, matte, bmp);
    }
    if (ec == hh::error_code::ok) {
        ec = hh::encode_jpeg(img, l.quality, matte, jpeg);
    }
    if (ec != hh::error_code::ok) {
        fail(id + ": " + hh::error_name(ec));
        return;
    }
    tsv << "R\t" << id << "\t" << look_fields(fp, size, l) << "\t" << l.quality << "\t" << l.matte
        << "\t" << sha256_hex(img.rgba) << "\t" << sha256_hex(png) << "\t" << sha256_hex(bmp)
        << "\t" << sha256_hex(jpeg) << "\n";
}

int error_counter = 0;

void error_record(const hh::fingerprint& fp, std::uint32_t size, const look& l,
                  hh::error_code expected) {
    const std::string id = "e" + std::to_string(++error_counter);
    hh::image img;
    const hh::error_code ec = hh::render(fp, size, options_of(l), img);
    if (ec != expected) {
        fail(id + ": expected " + hh::error_name(expected) + ", got " + hh::error_name(ec));
    }
    tsv << "E\t" << id << "\t" << look_fields(fp, size, l) << "\t" << hh::error_name(ec) << "\n";
}

void golden_record(const std::string& name, const hh::fingerprint& fp, std::uint32_t size,
                   const look& l) {
    hh::image img;
    bytes png;
    if (hh::render(fp, size, options_of(l), img) != hh::error_code::ok ||
        hh::encode_png(img, png) != hh::error_code::ok) {
        fail("golden " + name);
        return;
    }
    golden[name] = png;
    tsv << "G\t" << name.substr(0, name.size() - 4) << "\t" << name << "\t"
        << look_fields(fp, size, l) << "\n";
}

// ---- W: every size of a range ------------------------------------------------

int sweep_counter = 0;

void sweep_record(const hh::fingerprint& fp, const look& l, std::uint32_t first,
                  std::uint32_t last) {
    const std::string id = "w" + std::to_string(++sweep_counter);
    hh::detail::sha256 hasher;
    hh::image img;
    for (std::uint32_t size = first; size <= last; ++size) {
        const hh::error_code ec = hh::render(fp, size, options_of(l), img);
        if (ec != hh::error_code::ok) {
            fail(id + " at " + std::to_string(size) + ": " + hh::error_name(ec));
            return;
        }
        hasher.update(img.rgba.data(), img.rgba.size());
    }
    std::ostringstream out;
    out << to_hex(fp.bytes()) << "\t" << hh_tools::mode_name(fp.mode()) << "\t"
        << hh_tools::shape_name(l.shape) << "\t" << hh_tools::frame_name(l.frame) << "\t"
        << l.background << "\t" << l.frame_alpha;
    tsv << "W\t" << id << "\t" << out.str() << "\t" << first << "\t" << last << "\t"
        << to_hex(hasher.finish()) << "\n";
}

// ---- I: the encoders on images that are not renders --------------------------

int image_counter = 0;

void image_record(std::uint32_t width, std::uint32_t height, const std::string& pattern,
                  int quality, const std::string& matte_text) {
    const std::string id = "i" + std::to_string(++image_counter);
    hh::image img;
    hh::rgb matte{};
    bytes png, bmp, jpeg;
    if (!hh_tools::pattern_image(pattern, width, height, img) ||
        !hh_tools::parse_rgb(matte_text, matte) || hh::encode_png(img, png) != hh::error_code::ok ||
        hh::encode_bmp(img, matte, bmp) != hh::error_code::ok ||
        hh::encode_jpeg(img, quality, matte, jpeg) != hh::error_code::ok) {
        fail(id + ": " + pattern);
        return;
    }
    tsv << "I\t" << id << "\t" << width << "\t" << height << "\t" << pattern << "\t" << quality
        << "\t" << matte_text << "\t" << sha256_hex(img.rgba) << "\t" << sha256_hex(png) << "\t"
        << sha256_hex(bmp) << "\t" << sha256_hex(jpeg) << "\n";
    if (pattern == "opaque:2" && width == 8 && height == 8 && quality == 100) {
        // This record exists for the stuffed final byte of the entropy-coded data.
        const std::size_t n = jpeg.size();
        if (n < 4 || jpeg[n - 4] != 0xFF || jpeg[n - 3] != 0x00) {
            fail(id + ": the JPEG does not end in FF 00 FF D9");
        }
    }
}

// ---- F: failures outside render() --------------------------------------------

int failure_counter = 0;

void failure_record(const std::string& operation, const std::string& arguments, hh::error_code got,
                    hh::error_code expected) {
    const std::string id = "f" + std::to_string(++failure_counter);
    if (got != expected) {
        fail(id + ": expected " + hh::error_name(expected) + ", got " + hh::error_name(got));
    }
    tsv << "F\t" << id << "\t" << operation << "\t" << arguments << "\t" << hh::error_name(got)
        << "\n";
}

hh::fingerprint fingerprint_of(const bytes& b, hh::mode m) {
    hh::fingerprint fp;
    hh::import_fingerprint({b.data(), b.size()}, m, fp);
    return fp;
}

look with(hh::image_shape shape, hh::frame_style frame, const std::string& background = "ffffffff",
          unsigned frame_alpha = 255) {
    look l;
    l.shape = shape;
    l.frame = frame;
    l.background = background;
    l.frame_alpha = frame_alpha;
    return l;
}

void generate() {
    using hh::frame_style;
    using hh::image_shape;
    tsv << "# Humanized Hash golden vectors. Generated by tools/gen_vectors of hh-cpp; do not "
           "edit.\n"
           "# The format is defined in docs/SPEC.md, section 15.\n";

    // ---- D ----
    tsv << "# D\tid\tkind\tinput\tkey\tM1\td0\ts\tM2\tfp universal\tfp keyed\tKCV\tcells universal"
           "\tcells keyed\ttag universal\ttag keyed\n";
    const bytes k1 = key_1();
    const bytes k2 = key_2();
    // The two addresses are test vectors of EIP-55.
    const derived evm1 =
        derive_bytes("evm-1", false, hex("5aAeb6053F3E94C9b9A09f33669435E7Ef1BeAed"), k1);
    const derived evm2 =
        derive_bytes("evm-2", false, hex("fB6916095ca1df60bB79Ce92cE3Ea74c37c5d359"), k1);
    derive_bytes("evm-1-other-key", false, hex("5aAeb6053F3E94C9b9A09f33669435E7Ef1BeAed"), k2);
    derive_bytes("evm-zero", false, bytes(20, 0), k1);
    // A lookalike pair in the manner of address poisoning: the first and the last four bytes agree.
    const derived poison_a =
        derive_bytes("poison-a", false, hex("1234567890abcdef00112233445566778899aabb"), k1);
    const derived poison_b =
        derive_bytes("poison-b", false, hex("12345678f1e2d3c4b5a69788796a5b4c8899aabb"), k1);
    bytes sui(32);
    for (std::size_t i = 0; i < sui.size(); ++i) {
        sui[i] = static_cast<std::uint8_t>(7 * i + 1);
    }
    derive_bytes("sui-32", false, sui, k1);
    derive_bytes("solana-system-program", false, bytes(32, 0), k1);
    derive_bytes("tron-21", false, hex("41a614f803b6fd780986a42c78ec9c7f77e6ded13c"), k1);
    bytes ton = hex("00000000");
    ton.insert(ton.end(), sui.rbegin(), sui.rend());
    derive_bytes("ton-workchain-0", false, ton, k1);
    // The genesis block of Bitcoin: its hash, and the address of its coinbase as text.
    derive_bytes("bitcoin-block-hash", false,
                 hex("000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f"), {});
    derive_bytes("bitcoin-base58", true, ascii("1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa"), k1);
    // The bech32 example of BIP-173, as text and, to show the separation, as binary.
    derive_bytes("bitcoin-bech32", true, ascii("bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4"), k1);
    derive_bytes("bitcoin-bech32-as-binary", false,
                 ascii("bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4"), {});
    // Two, three and four byte UTF-8 sequences: "caf" U+00E9 " " U+20AC " " U+10348.
    derive_bytes("text-utf8", true, hex("636166c3a920e282ac20f0908d88"), k1);
    derive_bytes("one-byte-00", false, hex("00"), k1);
    derive_bytes("one-byte-ff", false, hex("ff"), {});
    // M1 of 55, 56, 63, 64 and 65 bytes: around the SHA-256 padding and block boundaries.
    for (std::size_t length : {35u, 36u, 43u, 44u, 45u}) {
        derive_fill("m1-" + std::to_string(length + 20) + "-bytes", 0x5A, length, {});
    }
    derive_fill("one-mebibyte", 0xAB, 1048576, k1);

    // ---- H ----
    tsv << "# H\tid\tstring (hex of its UTF-8 bytes)\tdecoded bytes or error\n";
    int h = 0;
    auto hex_record = [&](const std::string& text) {
        hh::base_digest from_text;
        const hh::error_code ec = hh::make_base_digest_from_hex(text, from_text);
        std::string result = hh::error_name(ec);
        if (ec == hh::error_code::ok) {
            std::string digits = text;
            if (digits.size() >= 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) {
                digits.erase(0, 2);
            }
            const bytes decoded = hex(digits);
            hh::base_digest from_bytes;
            hh::make_base_digest({decoded.data(), decoded.size()}, from_bytes);
            if (from_bytes != from_text) {
                fail("hex record " + text);
            }
            result = to_hex(decoded);
        }
        tsv << "H\th" << ++h << "\t" << to_hex(ascii(text)) << "\t" << result << "\n";
    };
    for (const char* text : {"0x5aAeb6053F3E94C9b9A09f33669435E7Ef1BeAed",
                             "0X5AAEB6053F3E94C9B9A09F33669435E7EF1BEAED",
                             "5aaeb6053f3e94c9b9a09f33669435e7ef1beaed",
                             "00",
                             "0x00ff",
                             "",
                             "0x",
                             "0",
                             "abc",
                             "0xabc",
                             "0xzz",
                             "0x0g",
                             " ab",
                             "ab ",
                             "a b0",
                             "0x 12",
                             "x012",
                             "00x1",
                             "+1ab",
                             "ab:cd",
                             "0x0x12"}) {
        hex_record(text);
    }
    hex_record(std::string("ab\0cd", 5));
    hex_record("\xC3\xA9\xC3\xA9");

    // ---- K ----
    tsv << "# K\tid\tkey\tKCV or error\n";
    int k = 0;
    auto key_record = [&](const bytes& key) {
        hh::secret_key sk;
        const hh::error_code ec = hh::make_secret_key({key.data(), key.size()}, sk);
        tsv << "K\tk" << ++k << "\t" << (key.empty() ? "-" : to_hex(key)) << "\t"
            << (ec == hh::error_code::ok ? to_hex(sk.kcv()) : hh::error_name(ec)) << "\n";
    };
    key_record(k1);
    key_record(k2);
    bytes low(32, 0);
    low[31] = 1;
    key_record(low);
    key_record(bytes(32, 0));
    key_record(bytes(31, 0x11));
    key_record(bytes(33, 0x11));
    key_record(bytes(1, 0x11));
    key_record(bytes(64, 0x11));

    // ---- C ----
    tsv << "# C\tid\tbackground\tframe alpha\tpage\tfigures_x100\tframe_x100\n";
    int c = 0;
    auto contrast_record = [&](const std::string& background, unsigned frame_alpha,
                               const std::string& page) {
        hh::render_options o;
        hh_tools::parse_rgba(background, o.background, o.background_alpha);
        o.frame_alpha = static_cast<std::uint8_t>(frame_alpha);
        hh::rgb p{};
        hh_tools::parse_rgb(page, p);
        const hh::contrast_report r = hh::measure_contrast(o, p);
        tsv << "C\tc" << ++c << "\t" << background << "\t" << frame_alpha << "\t" << page << "\t"
            << r.figures_x100 << "\t" << r.frame_x100 << "\n";
    };
    for (const char* background :
         {"ffffffff", "121212ff", "000000ff", "f2f2f2ff", "f4ecd8ff", "e3f0fbff", "9e9e9eff",
          "7a96c5ff", "890af0ff", "c10445ff", "d48200ff", "0f1b2dff", "808080ff"}) {
        contrast_record(background, 255, "ffffff");
    }
    contrast_record("ffffffff", 128, "ffffff");
    contrast_record("ffffffff", 0, "ffffff");
    contrast_record("00000000", 255, "121212");
    contrast_record("00000000", 128, "121212");
    contrast_record("00000000", 255, "9e9e9e");
    contrast_record("ffffff80", 255, "121212");
    contrast_record("12345640", 77, "fedcba");
    contrast_record("123456c0", 200, "000000");

    // ---- R ----
    tsv << "# R\tid\tfp\tmode\tsize\tshape\tframe\tbackground\tframe alpha\tquality\tmatte\tsha256 "
           "rgba"
           "\tsha256 png\tsha256 bmp\tsha256 jpeg\n";
    // Every figure in every colour, and two empty cells.
    bytes sampler_bytes(32, 0);
    const std::uint8_t sampler_cells[16] = {0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70, 0x78,
                                            0x80, 0xA8, 0xD0, 0xF8, 0x98, 0xB0, 0x00, 0x20};
    std::copy(sampler_cells, sampler_cells + 16, sampler_bytes.begin());
    const hh::fingerprint sampler_u = fingerprint_of(sampler_bytes, hh::mode::universal);
    const hh::fingerprint sampler_k = fingerprint_of(sampler_bytes, hh::mode::keyed);
    const hh::fingerprint blank_u = fingerprint_of(bytes(32, 0), hh::mode::universal);
    const hh::fingerprint blank_k = fingerprint_of(bytes(32, 0), hh::mode::keyed);
    const hh::fingerprint squares_u = fingerprint_of(bytes(32, 0x40), hh::mode::universal);

    const std::uint32_t sizes[] = {16, 32, 48, 64, 128, 256, 1024};
    for (std::uint32_t size : sizes) {
        render_record(evm1.universal, size, look{});
        render_record(evm1.keyed, size, look{});
    }
    for (std::uint32_t size : {32u, 64u, 128u, 1024u}) {
        render_record(evm1.universal, size,
                      with(image_shape::square, frame_style::automatic, "00000000"));
        render_record(evm1.keyed, size,
                      with(image_shape::round, frame_style::automatic, "00000000"));
    }
    for (frame_style style :
         {frame_style::none, frame_style::plain, frame_style::rounded, frame_style::chamfered,
          frame_style::double_line, frame_style::thick, frame_style::brackets}) {
        for (std::uint32_t size : {31u, 64u, 128u}) {
            render_record(evm2.keyed, size, with(image_shape::square, style));
        }
    }
    for (frame_style style : {frame_style::none, frame_style::plain, frame_style::double_line,
                              frame_style::thick, frame_style::ticks, frame_style::gaps}) {
        for (std::uint32_t size : {33u, 64u, 128u}) {
            render_record(evm2.keyed, size, with(image_shape::round, style));
        }
    }
    render_record(evm2.universal, 48, with(image_shape::square, frame_style::plain));
    render_record(evm2.universal, 128, with(image_shape::square, frame_style::plain));
    // The mode does not restrict the look: universal fingerprints take every style of the shape.
    for (frame_style style :
         {frame_style::rounded, frame_style::chamfered, frame_style::double_line,
          frame_style::thick, frame_style::brackets}) {
        render_record(evm2.universal, 64, with(image_shape::square, style));
    }
    for (frame_style style :
         {frame_style::double_line, frame_style::thick, frame_style::ticks, frame_style::gaps}) {
        render_record(evm2.universal, 64, with(image_shape::round, style));
    }
    render_record(evm2.universal, 64, with(image_shape::round, frame_style::none));
    render_record(evm2.universal, 128, with(image_shape::round, frame_style::plain));
    render_record(evm2.keyed, 1024, with(image_shape::round, frame_style::ticks, "00000000"));
    render_record(evm2.keyed, 17,
                  with(image_shape::square, frame_style::rounded));  // the radius is capped
    render_record(evm2.keyed, 18, with(image_shape::square, frame_style::rounded));
    render_record(evm2.keyed, 18, with(image_shape::round, frame_style::thick));
    render_record(evm2.keyed, 16, with(image_shape::round, frame_style::gaps));
    // Backgrounds and frame transparency.
    render_record(evm1.keyed, 128, with(image_shape::square, frame_style::automatic, "121212ff"));
    render_record(evm1.keyed, 128, with(image_shape::square, frame_style::automatic, "000000ff"));
    render_record(evm1.keyed, 128, with(image_shape::square, frame_style::automatic, "f2f2f2ff"));
    render_record(evm1.keyed, 96,
                  with(image_shape::square, frame_style::double_line, "12121280", 128));
    render_record(evm1.keyed, 96,
                  with(image_shape::round, frame_style::double_line, "fffffe01", 1));
    render_record(evm1.keyed, 96, with(image_shape::square, frame_style::thick, "ffffffff", 0));
    render_record(evm1.keyed, 96, with(image_shape::round, frame_style::ticks, "0f1b2dc8", 254));
    // Particular fingerprints.
    render_record(sampler_u, 128, look{});
    render_record(sampler_u, 128, with(image_shape::square, frame_style::automatic, "00000000"));
    render_record(sampler_k, 200, with(image_shape::round, frame_style::gaps, "121212ff"));
    render_record(blank_u, 64, look{});
    render_record(blank_k, 64, look{});
    render_record(squares_u, 64, look{});
    render_record(poison_a.universal, 128, look{});
    render_record(poison_b.universal, 128, look{});
    // JPEG quality and the matte of the formats without alpha.
    for (int quality : {50, 75, 100}) {
        look l;
        l.quality = quality;
        render_record(evm1.universal, 64, l);
    }
    look dark_matte = with(image_shape::square, frame_style::automatic, "00000000");
    dark_matte.matte = "121212";
    render_record(evm1.keyed, 64, dark_matte);
    dark_matte.shape = image_shape::round;
    dark_matte.frame = frame_style::thick;
    dark_matte.matte = "0048ff";
    dark_matte.quality = 60;
    render_record(evm1.keyed, 50, dark_matte);

    // ---- E ----
    tsv << "# E\tid\tfp\tmode\tsize\tshape\tframe\tbackground\tframe alpha\terror\n";
    using hh::error_code;
    for (std::uint32_t size : {0u, 15u, 1025u, 4096u}) {
        error_record(evm1.universal, size, look{}, error_code::invalid_size);
    }
    // A style that does not fit the shape, in either mode.
    for (const hh::fingerprint* fp : {&evm1.universal, &evm1.keyed}) {
        for (frame_style style : {frame_style::ticks, frame_style::gaps}) {
            error_record(*fp, 64, with(image_shape::square, style), error_code::invalid_frame);
        }
        for (frame_style style :
             {frame_style::rounded, frame_style::chamfered, frame_style::brackets}) {
            error_record(*fp, 64, with(image_shape::round, style), error_code::invalid_frame);
        }
    }
    for (const char* background :
         {"9e9e9eff", "7a96c5ff", "890af0ff", "c10445ff", "d48200ff", "808080ff"}) {
        error_record(evm1.universal, 64,
                     with(image_shape::square, frame_style::automatic, background),
                     error_code::low_contrast);
    }
    error_record(evm1.keyed, 16, with(image_shape::round, frame_style::thick),
                 error_code::invalid_size);
    error_record(evm1.keyed, 17, with(image_shape::round, frame_style::double_line),
                 error_code::invalid_size);
    // The order of the checks: size, then frame, then contrast, then the room for the cells.
    error_record(evm1.universal, 15, with(image_shape::square, frame_style::thick, "9e9e9eff"),
                 error_code::invalid_size);
    error_record(evm1.universal, 64, with(image_shape::square, frame_style::ticks, "9e9e9eff"),
                 error_code::invalid_frame);
    error_record(evm1.universal, 64, with(image_shape::square, frame_style::thick, "9e9e9eff"),
                 error_code::low_contrast);
    error_record(evm1.keyed, 16, with(image_shape::round, frame_style::thick, "9e9e9eff"),
                 error_code::low_contrast);

    // ---- W ----
    tsv << "# W\tid\tfp\tmode\tshape\tframe\tbackground\tframe alpha\tfirst size\tlast size"
           "\tsha256 of the concatenated rgba buffers\n";
    const std::string veil = "12345680";  // translucent, so that every style renders
    sweep_record(evm1.universal, look{}, 16, 160);
    sweep_record(evm1.keyed, look{}, 16, 160);
    for (frame_style style :
         {frame_style::none, frame_style::plain, frame_style::rounded, frame_style::chamfered,
          frame_style::double_line, frame_style::thick, frame_style::brackets}) {
        sweep_record(evm2.keyed, with(image_shape::square, style, veil, 200), 16, 160);
    }
    for (frame_style style :
         {frame_style::none, frame_style::plain, frame_style::ticks, frame_style::gaps}) {
        sweep_record(evm2.keyed, with(image_shape::round, style, veil, 200), 16, 160);
    }
    for (frame_style style : {frame_style::double_line, frame_style::thick}) {
        sweep_record(evm2.keyed, with(image_shape::round, style, veil, 200), 18, 160);
    }
    sweep_record(evm2.universal, with(image_shape::square, frame_style::rounded, veil, 200), 16,
                 160);
    sweep_record(evm2.universal, with(image_shape::round, frame_style::ticks, veil, 200), 16, 160);

    // ---- I ----
    tsv << "# I\tid\twidth\theight\tpattern\tquality\tmatte\tsha256 rgba\tsha256 png\tsha256 bmp"
           "\tsha256 jpeg\n";
    image_record(1, 1, "noise:1", 92, "ffffff");
    image_record(1, 1, "opaque:1", 92, "ffffff");
    image_record(1, 1, "flat:00000000", 92, "0048ff");
    image_record(3, 5, "noise:3", 92, "ffffff");
    image_record(5, 3, "opaque:4", 75, "ffffff");
    image_record(8, 8, "opaque:2", 100, "ffffff");  // the entropy-coded data ends in a stuffed FF
    image_record(17, 9, "noise:5", 50, "0048ff");
    image_record(9, 64, "noise:6", 77, "121212");
    image_record(64, 7, "opaque:7", 92, "ffffff");
    image_record(300, 2, "flat:7a96c5ff", 92, "ffffff");  // runs longer than the longest match
    image_record(33, 33, "flat:ffffff80", 100, "000000");
    image_record(16, 16, "flat:000000ff", 50, "ffffff");   // the lowest DC value
    image_record(16, 16, "flat:ffffffff", 100, "ffffff");  // the highest DC value
    image_record(257, 31, "opaque:8", 60, "ffffff");

    // ---- F ----
    tsv << "# F\tid\toperation\targuments\terror\n";
    {
        using hh::error_code;
        hh::base_digest digest;
        failure_record("digest", "binary\thex:", hh::make_base_digest({nullptr, 0}, digest),
                       error_code::empty_input);
        failure_record("digest", "text\thex:", hh::make_base_digest_from_text("", digest),
                       error_code::empty_input);
        const bytes too_long(1048577, 0x61);
        failure_record("digest", "binary\tfill:61:1048577",
                       hh::make_base_digest({too_long.data(), too_long.size()}, digest),
                       error_code::input_too_large);
        failure_record(
            "digest", "text\tfill:61:1048577",
            hh::make_base_digest_from_text(
                std::string_view(reinterpret_cast<const char*>(too_long.data()), too_long.size()),
                digest),
            error_code::input_too_large);
        hh::image img;
        hh_tools::pattern_image("flat:ffffffff", 8, 8, img);
        bytes out;
        for (int quality : {-1, 0, 49, 101, 1000}) {
            failure_record("jpeg", std::to_string(quality),
                           hh::encode_jpeg(img, quality, {255, 255, 255}, out),
                           error_code::invalid_quality);
        }
        struct shape {
            std::uint32_t width, height;
            std::size_t length;
        };
        for (const shape& s :
             {shape{0, 1, 0}, shape{1, 0, 0}, shape{4097, 1, 16388}, shape{1, 4097, 16388},
              shape{2, 2, 15}, shape{2, 2, 17}, shape{2, 2, 0}}) {
            hh::image bad;
            bad.width = s.width;
            bad.height = s.height;
            bad.rgba.assign(s.length, 0x7F);
            const std::string arguments = std::to_string(s.width) + "\t" +
                                          std::to_string(s.height) + "\t" +
                                          std::to_string(s.length);
            failure_record("image", arguments, hh::encode_png(bad, out), error_code::invalid_image);
            if (hh::encode_bmp(bad, {0, 0, 0}, out) != error_code::invalid_image ||
                hh::encode_jpeg(bad, 92, {0, 0, 0}, out) != error_code::invalid_image) {
                fail("image failure " + arguments);
            }
        }
    }

    // ---- G ----
    tsv << "# G\tid\tfile\tfp\tmode\tsize\tshape\tframe\tbackground\tframe alpha\n";
    golden_record("evm-1-universal-128.png", evm1.universal, 128, look{});
    golden_record("evm-1-keyed-128.png", evm1.keyed, 128, look{});
    golden_record("evm-1-universal-32.png", evm1.universal, 32, look{});
    golden_record("evm-1-keyed-48.png", evm1.keyed, 48, look{});
    golden_record("evm-1-universal-256-transparent.png", evm1.universal, 256,
                  with(image_shape::square, frame_style::automatic, "00000000"));
    golden_record("evm-1-keyed-128-dark.png", evm1.keyed, 128,
                  with(image_shape::square, frame_style::automatic, "121212ff"));
    golden_record("evm-2-universal-128-plain.png", evm2.universal, 128,
                  with(image_shape::square, frame_style::plain));
    golden_record("evm-2-keyed-128-chamfered.png", evm2.keyed, 128,
                  with(image_shape::square, frame_style::chamfered));
    golden_record("evm-2-keyed-128-double.png", evm2.keyed, 128,
                  with(image_shape::square, frame_style::double_line));
    golden_record("evm-2-keyed-128-thick.png", evm2.keyed, 128,
                  with(image_shape::square, frame_style::thick));
    golden_record("evm-2-keyed-128-brackets.png", evm2.keyed, 128,
                  with(image_shape::square, frame_style::brackets));
    golden_record("evm-2-universal-128-round.png", evm2.universal, 128,
                  with(image_shape::round, frame_style::none));
    golden_record("evm-2-universal-128-round-plain.png", evm2.universal, 128,
                  with(image_shape::round, frame_style::plain));
    golden_record("evm-2-keyed-128-round-double.png", evm2.keyed, 128,
                  with(image_shape::round, frame_style::double_line));
    golden_record("evm-2-keyed-128-round-thick.png", evm2.keyed, 128,
                  with(image_shape::round, frame_style::thick));
    golden_record("evm-2-keyed-128-round-ticks.png", evm2.keyed, 128,
                  with(image_shape::round, frame_style::ticks));
    golden_record("evm-2-keyed-128-round-gaps.png", evm2.keyed, 128,
                  with(image_shape::round, frame_style::gaps));
    golden_record("sampler-universal-128.png", sampler_u, 128, look{});
    golden_record("poison-a-universal-128.png", poison_a.universal, 128, look{});
    golden_record("poison-b-universal-128.png", poison_b.universal, 128, look{});
}

bool read_file(const std::string& path, bytes& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

bool write_file(const std::string& path, const bytes& data) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(f);
}

}  // namespace

int main(int argc, char** argv) {
    const bool check = argc == 3 && std::string(argv[1]) == "--check";
    if (argc != 2 && !check) {
        std::cerr << "usage: gen_vectors <dir> | gen_vectors --check <dir>\n";
        return 2;
    }
    const std::string dir = argv[argc - 1];
    generate();
    if (failures != 0) {
        return 1;
    }
    const std::string text = tsv.str();
    const bytes table(text.begin(), text.end());
    if (check) {
        bytes existing;
        if (!read_file(dir + "/vectors.tsv", existing) || existing != table) {
            std::cerr << "gen_vectors: " << dir
                      << "/vectors.tsv differs from the reference implementation\n";
            return 1;
        }
        for (const auto& [name, png] : golden) {
            if (!read_file(dir + "/golden/" + name, existing) || existing != png) {
                std::cerr << "gen_vectors: golden/" << name << " differs\n";
                return 1;
            }
        }
        std::cout << "vectors.tsv and " << golden.size() << " golden files match\n";
        return 0;
    }
    if (!write_file(dir + "/vectors.tsv", table)) {
        std::cerr << "gen_vectors: cannot write into " << dir << "\n";
        return 1;
    }
    for (const auto& [name, png] : golden) {
        if (!write_file(dir + "/golden/" + name, png)) {
            std::cerr << "gen_vectors: cannot write " << dir << "/golden/" << name << "\n";
            return 1;
        }
    }
    std::cout << "wrote " << dir << "/vectors.tsv and " << golden.size() << " golden files\n";
    return 0;
}
