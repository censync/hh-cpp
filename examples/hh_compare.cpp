// hh_compare: two addresses side by side in one picture, as a host shows them when
// a decision depends on the comparison (a pasted address against a saved one).
//
//   hh_compare 0x1234567890abcdef00112233445566778899aabb
//              0x12345678f1e2d3c4b5a69788796a5b4c8899aabb pair.png
//
// The two addresses above agree in their first and last four bytes, as the
// lookalikes of address poisoning do; their pictures are unrelated.

#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <hh/hh.hpp>

namespace {

bool picture_of(const std::string& hex, std::uint32_t size, hh::image& img, std::string& tag) {
    hh::base_digest digest;
    hh::fingerprint fp;
    hh::error_code ec = hh::make_base_digest_from_hex(hex, digest);
    if (ec == hh::error_code::ok) {
        ec = hh::universal_fingerprint(digest, fp);
    }
    if (ec == hh::error_code::ok) {
        ec = hh::render(fp, size, hh::render_options{}, img);
    }
    if (ec != hh::error_code::ok) {
        std::cerr << hex << ": " << hh::error_message(ec) << "\n";
        return false;
    }
    tag = fp.tag();
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "usage: hh_compare <hex address> <hex address> <out.png>\n";
        return 2;
    }
    constexpr std::uint32_t size = 128;
    constexpr std::uint32_t gap = 32;
    hh::image left;
    hh::image right;
    std::string left_tag;
    std::string right_tag;
    if (!picture_of(argv[1], size, left, left_tag) ||
        !picture_of(argv[2], size, right, right_tag)) {
        return 1;
    }

    hh::image pair;
    pair.width = 2 * size + gap;
    pair.height = size;
    pair.rgba.assign(static_cast<std::size_t>(pair.width) * pair.height * 4, 255);
    for (std::uint32_t y = 0; y < size; ++y) {
        const std::size_t row = std::size_t{y} * size * 4;
        const std::size_t out = std::size_t{y} * pair.width * 4;
        std::memcpy(pair.rgba.data() + out, left.rgba.data() + row, std::size_t{size} * 4);
        std::memcpy(pair.rgba.data() + out + std::size_t{size + gap} * 4, right.rgba.data() + row,
                    std::size_t{size} * 4);
    }
    std::vector<std::uint8_t> png;
    if (hh::encode_png(pair, png) != hh::error_code::ok) {
        return 1;
    }
    std::ofstream file(argv[3], std::ios::binary);
    file.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
    if (!file) {
        std::cerr << "cannot write " << argv[3] << "\n";
        return 1;
    }
    std::cout << left_tag.substr(0, 3) << "-" << left_tag.substr(3) << "   "
              << right_tag.substr(0, 3) << "-" << right_tag.substr(3) << "   "
              << (left.rgba == right.rgba ? "the pictures are equal" : "the pictures differ")
              << "\n";
    return 0;
}
