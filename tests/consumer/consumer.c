/* What a C consumer of the installed package compiles: the C ABI only. */
#include <stdio.h>
#include <string.h>

#include <hh/hh.h>

int main(void) {
    static const unsigned char address[20] = {0x5a, 0xAe, 0xb6, 0x05, 0x3F, 0x3E, 0x94,
                                              0xC9, 0xb9, 0xA0, 0x9f, 0x33, 0x66, 0x94,
                                              0x35, 0xE7, 0xEf, 0x1B, 0xeA, 0xed};
    unsigned char digest[HH_DIGEST_SIZE];
    unsigned char rgba[32 * 32 * 4];
    char tag[HH_TAG_SIZE];
    if (hh_base_digest(address, sizeof(address), digest) != HH_OK || hh_tag(digest, tag) != HH_OK ||
        hh_render(digest, HH_MODE_UNIVERSAL, 32, NULL, rgba, sizeof(rgba)) != HH_OK) {
        return 1;
    }
    printf("hh %s from C: tag %s\n", hh_version(), tag);
    return strcmp(tag, "TKSPVH") == 0 ? 0 : 1;
}
