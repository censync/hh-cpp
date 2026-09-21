// hh_cli: an address or hash in, a picture out.
//
//   hh_cli 0x5aAeb6053F3E94C9b9A09f33669435E7Ef1BeAed --out address.png
//   hh_cli --text bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4 --size 256 --out address.png
//   hh_cli <hex> --key <64 hex digits> --shape round --frame double --out private.png
//
// With --batch FILE DIR every line of FILE is one case; the command line tools of the other
// implementations read the same format and must print the same lines and write the same files.
// The format, which every tool follows to the letter:
//
//   - The file is bytes. Lines end with LF; one CR before it is dropped. A line that is then
//     empty or begins with '#' is skipped and not counted. Cases are numbered from 1.
//   - A case is exactly 11 fields separated by runs of ASCII spaces or tabs:
//       hex|text  input  key|-  size  shape  frame  background  frame-alpha  format  quality  matte
//   - size, frame-alpha and quality are 1 to 10 ASCII digits without a sign. A frame alpha
//     above 255 is a bad case; size and quality go to the library as they are, however large.
//   - For "text" the input is the hexadecimal form of the UTF-8 bytes; for "hex" it is passed
//     on as written. background is 8 and matte 6 hexadecimal digits.
//   - A line that breaks these rules, or names an unknown kind, shape or frame, prints
//     "<n>\tbad_case". Everything else is the library's answer: "<n>\t<error name>" and, for ok,
//     the base digest, the fingerprint, the tag and the key check value (or "-"), and the file
//     DIR/case-<n>.<format>.
//
// The numeric options of a single render follow the same rule; a malformed one is a usage error.
//
// This is a demonstration and a test tool. A real host never takes a key from
// the command line, where other processes can read it, and wipes its copies.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <hh/hh.hpp>

#include "../tools/names.hpp"

namespace {

struct request {
    bool text = false;
    std::string input;  // hex digits, or the text itself
    std::string key;    // 64 hex digits or empty
    std::uint32_t size = 128;
    hh::render_options options;
    std::string format = "png";
    int quality = hh::default_jpeg_quality;
    hh::rgb matte = {255, 255, 255};
};

struct result {
    hh::error_code error = hh::error_code::ok;
    hh::base_digest digest;
    hh::fingerprint fingerprint;
    std::string kcv;
    std::vector<std::uint8_t> bytes;
};

result run(const request& rq) {
    result r;
    r.error = rq.text ? hh::make_base_digest_from_text(rq.input, r.digest)
                      : hh::make_base_digest_from_hex(rq.input, r.digest);
    if (r.error != hh::error_code::ok) {
        return r;
    }
    if (rq.key.empty()) {
        r.error = hh::universal_fingerprint(r.digest, r.fingerprint);
    } else {
        std::vector<std::uint8_t> key_bytes;
        hh::secret_key key;
        if (!hh_tools::from_hex(rq.key, key_bytes)) {
            r.error = hh::error_code::invalid_key;
            return r;
        }
        r.error = hh::make_secret_key({key_bytes.data(), key_bytes.size()}, key);
        if (r.error != hh::error_code::ok) {
            return r;
        }
        r.kcv = hh_tools::to_hex(key.kcv());
        r.error = hh::keyed_fingerprint(r.digest, key, r.fingerprint);
    }
    if (r.error != hh::error_code::ok) {
        return r;
    }
    hh::image img;
    r.error = hh::render(r.fingerprint, rq.size, rq.options, img);
    if (r.error != hh::error_code::ok) {
        return r;
    }
    if (rq.format == "png") {
        r.error = hh::encode_png(img, r.bytes);
    } else if (rq.format == "bmp") {
        r.error = hh::encode_bmp(img, rq.matte, r.bytes);
    } else if (rq.format == "jpeg") {
        r.error = hh::encode_jpeg(img, rq.quality, rq.matte, r.bytes);
    } else if (rq.format == "rgba") {
        r.bytes = img.rgba;
    } else {
        r.error = hh::error_code::invalid_argument;
    }
    return r;
}

bool write_file(const std::string& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(f);
}

// 1 to 10 ASCII digits without a sign. Values beyond what the library takes are clamped to a
// value that is just as invalid, so that nothing wraps around.
bool parse_number(const std::string& text, std::uint32_t& out) {
    if (text.empty() || text.size() > 10) {
        return false;
    }
    std::uint64_t v = 0;
    for (char c : text) {
        if (c < '0' || c > '9') {
            return false;
        }
        v = v * 10 + static_cast<std::uint64_t>(c - '0');
    }
    out = v > 0x7FFFFFFFu ? 0x7FFFFFFFu : static_cast<std::uint32_t>(v);
    return true;
}

std::vector<std::string> split_fields(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
            ++i;
        }
        std::size_t j = i;
        while (j < line.size() && line[j] != ' ' && line[j] != '\t') {
            ++j;
        }
        if (j > i) {
            fields.push_back(line.substr(i, j - i));
        }
        i = j;
    }
    return fields;
}

void usage() {
    std::cerr
        << "usage: hh_cli [options] <input>\n"
           "  <input>               hexadecimal bytes (optional 0x), or text with --text\n"
           "  --text                hash the input as UTF-8 text\n"
           "  --key HEX             64 hex digits: render the keyed (private) picture\n"
           "  --size N              16..1024 pixels (default 128)\n"
           "  --shape NAME          square (default) or round\n"
           "  --frame NAME          automatic (default), none, plain, rounded, chamfered, double,\n"
           "                        thick, brackets, ticks, gaps\n"
           "  --background RRGGBBAA background colour and alpha (default ffffffff)\n"
           "  --frame-alpha N       0..255 (default 255)\n"
           "  --format NAME         png (default), bmp, jpeg or rgba\n"
           "  --quality N           JPEG quality 50..100 (default 92)\n"
           "  --matte RRGGBB        what BMP and JPEG flatten transparency over (default ffffff)\n"
           "  --out FILE            write the picture; without it only the values are printed\n"
           "  --batch FILE DIR      run the cases of FILE, write case-<n>.<format> into DIR;\n"
           "                        a case is 11 fields separated by spaces or tabs:\n"
           "                        hex|text <input> <key|-> <size> <shape> <frame> <background>\n"
           "                        <frame alpha> <format> <quality> <matte>\n"
           "                        (the input of a text case is the hex form of its bytes;\n"
           "                        numbers are 1 to 10 digits without a sign)\n";
}

int batch(const std::string& file, const std::string& dir) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        std::cerr << "cannot read " << file << "\n";
        return 1;
    }
    std::error_code ignored;
    std::filesystem::create_directories(dir, ignored);
    std::string line;
    int n = 0;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }
        ++n;
        const std::vector<std::string> f = split_fields(line);
        request rq;
        std::uint32_t size = 0, frame_alpha = 0, quality = 0;
        bool ok = f.size() == 11 && (f[0] == "hex" || f[0] == "text") && parse_number(f[3], size) &&
                  parse_number(f[7], frame_alpha) && frame_alpha <= 255 &&
                  parse_number(f[9], quality) && hh_tools::parse_shape(f[4], rq.options.shape) &&
                  hh_tools::parse_frame(f[5], rq.options.frame) &&
                  hh_tools::parse_rgba(f[6], rq.options.background, rq.options.background_alpha) &&
                  hh_tools::parse_rgb(f[10], rq.matte);
        if (ok) {
            rq.text = f[0] == "text";
            if (rq.text) {
                std::vector<std::uint8_t> bytes;
                ok = hh_tools::from_hex(f[1], bytes);
                rq.input.assign(bytes.begin(), bytes.end());
            } else {
                rq.input = f[1];
            }
            rq.key = f[2] == "-" ? "" : f[2];
            rq.size = size;
            rq.options.frame_alpha = static_cast<std::uint8_t>(frame_alpha);
            rq.format = f[8];
            rq.quality = static_cast<int>(quality);
        }
        if (!ok) {
            std::cout << n << "\tbad_case\n";
            continue;
        }
        const result r = run(rq);
        std::cout << n << "\t" << hh::error_name(r.error);
        if (r.error == hh::error_code::ok) {
            std::cout << "\t" << hh_tools::to_hex(r.digest.bytes()) << "\t"
                      << hh_tools::to_hex(r.fingerprint.bytes()) << "\t" << r.fingerprint.tag()
                      << "\t" << (r.kcv.empty() ? "-" : r.kcv);
            if (!write_file(dir + "/case-" + std::to_string(n) + "." + rq.format, r.bytes)) {
                std::cerr << "cannot write into " << dir << "\n";
                return 1;
            }
        }
        std::cout << "\n";
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    request rq;
    std::string out_path;
    bool have_input = false;
    bool format_given = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto value = [&]() -> std::string {
            if (i + 1 >= argc) {
                usage();
                std::exit(2);
            }
            return argv[++i];
        };
        bool ok = true;
        if (arg == "--batch") {
            const std::string file = value();
            return batch(file, value());
        } else if (arg == "--text") {
            rq.text = true;
        } else if (arg == "--key") {
            rq.key = value();
        } else if (arg == "--size") {
            ok = parse_number(value(), rq.size);
        } else if (arg == "--shape") {
            ok = hh_tools::parse_shape(value(), rq.options.shape);
        } else if (arg == "--frame") {
            ok = hh_tools::parse_frame(value(), rq.options.frame);
        } else if (arg == "--background") {
            ok = hh_tools::parse_rgba(value(), rq.options.background, rq.options.background_alpha);
        } else if (arg == "--frame-alpha") {
            std::uint32_t v = 0;
            ok = parse_number(value(), v) && v <= 255;
            rq.options.frame_alpha = static_cast<std::uint8_t>(v);
        } else if (arg == "--format") {
            rq.format = value();
            format_given = true;
        } else if (arg == "--quality") {
            std::uint32_t v = 0;
            ok = parse_number(value(), v);
            rq.quality = static_cast<int>(v);
        } else if (arg == "--matte") {
            ok = hh_tools::parse_rgb(value(), rq.matte);
        } else if (arg == "--out") {
            out_path = value();
        } else if (arg == "--help" || arg == "-h") {
            usage();
            return 0;
        } else if (!have_input && arg.rfind("--", 0) != 0) {
            rq.input = arg;
            have_input = true;
        } else {
            ok = false;
        }
        if (!ok) {
            usage();
            return 2;
        }
    }
    if (!have_input) {
        usage();
        return 2;
    }
    if (!format_given && out_path.size() > 4) {
        const std::string ext = out_path.substr(out_path.rfind('.') + 1);
        if (ext == "bmp" || ext == "rgba") {
            rq.format = ext;
        } else if (ext == "jpg" || ext == "jpeg") {
            rq.format = "jpeg";
        }
    }

    const result r = run(rq);
    if (r.error != hh::error_code::ok) {
        std::cerr << "error: " << hh::error_name(r.error) << ": " << hh::error_message(r.error)
                  << "\n";
        return 1;
    }
    const hh::layout l = hh::describe(r.fingerprint);
    std::cout << "mode         " << hh_tools::mode_name(l.mode) << "\n"
              << "base digest  " << hh_tools::to_hex(r.digest.bytes()) << "\n"
              << "fingerprint  " << hh_tools::to_hex(r.fingerprint.bytes()) << "\n";
    const std::string tag = r.fingerprint.tag();
    std::cout << "tag          " << tag.substr(0, 3) << "-" << tag.substr(3) << "\n";
    if (!r.kcv.empty()) {
        std::cout << "key check    " << r.kcv << "\n";
    }
    static const char* figures[] = {".", "S", "O", "^", ">", "v", "<"};
    for (int row = 0; row < 4; ++row) {
        std::cout << (row == 0 ? "cells        " : "             ");
        for (int col = 0; col < 4; ++col) {
            const hh::cell& c = l.cells[static_cast<std::size_t>(4 * row + col)];
            std::cout << figures[static_cast<unsigned>(c.figure)]
                      << (c.figure == hh::figure::none ? ' ' : static_cast<char>('0' + c.colour))
                      << " ";
        }
        std::cout << "\n";
    }
    if (!out_path.empty()) {
        if (!write_file(out_path, r.bytes)) {
            std::cerr << "cannot write " << out_path << "\n";
            return 1;
        }
        std::cout << "wrote        " << out_path << " (" << r.bytes.size() << " bytes)\n";
    }
    return 0;
}
