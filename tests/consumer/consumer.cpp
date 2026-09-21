// What a consumer of the installed package compiles: the public headers only.
#include <cstdio>
#include <cstring>

#include <hh/hh.h>
#include <hh/hh.hpp>

int main() {
    const unsigned char address[20] = {0x5a, 0xAe, 0xb6, 0x05, 0x3F, 0x3E, 0x94, 0xC9, 0xb9, 0xA0,
                                       0x9f, 0x33, 0x66, 0x94, 0x35, 0xE7, 0xEf, 0x1B, 0xeA, 0xed};
    hh::base_digest digest;
    hh::fingerprint fp;
    hh::image img;
    std::vector<std::uint8_t> png;
    if (hh::make_base_digest({address, sizeof(address)}, digest) != hh::error_code::ok ||
        hh::universal_fingerprint(digest, fp) != hh::error_code::ok ||
        hh::render(fp, 64, hh::render_options{}, img) != hh::error_code::ok ||
        hh::encode_png(img, png) != hh::error_code::ok) {
        return 1;
    }
    char tag[HH_TAG_SIZE];
    if (hh_tag(fp.bytes().data(), tag) != HH_OK || fp.tag() != tag || fp.tag() != "TKSPVH") {
        return 1;
    }
    std::printf("hh %s: tag %s, %zu bytes of PNG\n", hh::version(), tag, png.size());
    return std::strcmp(hh_version(), HH_VERSION_STRING) == 0 ? 0 : 1;
}
