/*
 * The C ABI exercised from a C translation unit: the header must be plain C and
 * every call must behave as hh.h documents. Returns the number of failures.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hh/hh.h>

static int failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("       %s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static int hex_equals(const uint8_t* bytes, size_t size, const char* hex) {
    static const char digits[] = "0123456789abcdef";
    size_t i;
    if (strlen(hex) != 2 * size) {
        return 0;
    }
    for (i = 0; i < size; ++i) {
        if (hex[2 * i] != digits[bytes[i] >> 4] || hex[2 * i + 1] != digits[bytes[i] & 15]) {
            return 0;
        }
    }
    return 1;
}

int hh_test_c_api(void) {
    static const uint8_t address[20] = {0x5a, 0xAe, 0xb6, 0x05, 0x3F, 0x3E, 0x94, 0xC9, 0xb9, 0xA0,
                                        0x9f, 0x33, 0x66, 0x94, 0x35, 0xE7, 0xEf, 0x1B, 0xeA, 0xed};
    static const char* address_hex = "0x5aAeb6053F3E94C9b9A09f33669435E7Ef1BeAed";
    static const char* text = "bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4";
    uint8_t digest[HH_DIGEST_SIZE];
    uint8_t digest2[HH_DIGEST_SIZE];
    uint8_t fp[HH_FINGERPRINT_SIZE];
    uint8_t keyed[HH_FINGERPRINT_SIZE];
    uint8_t key[HH_KEY_SIZE];
    uint8_t kcv[HH_KCV_SIZE];
    char tag[HH_TAG_SIZE];
    hh_layout layout;
    hh_render_options options;
    hh_contrast contrast;
    uint8_t* rgba;
    uint8_t* encoded;
    size_t bytes;
    size_t encoded_size = 0;
    int i;

    failures = 0;
    CHECK(strcmp(hh_version(), "1.0.0") == 0);
    CHECK(strcmp(hh_error_name(HH_INVALID_HEX), "invalid_hex") == 0);
    CHECK(strlen(hh_error_message(HH_LOW_CONTRAST)) > 0);
    CHECK(strcmp(hh_error_name(12345), "unknown") == 0);

    CHECK(hh_base_digest(address, sizeof(address), digest) == HH_OK);
    CHECK(hex_equals(digest, 32, "e212927148fcf76f6669c244a0db08bdd4f36dc50a378f6a1a3fe472807e7852"));
    CHECK(hh_base_digest_from_hex(address_hex, strlen(address_hex), digest2) == HH_OK);
    CHECK(memcmp(digest, digest2, 32) == 0);
    CHECK(hh_base_digest_from_text(text, strlen(text), digest2) == HH_OK);
    CHECK(hex_equals(digest2, 32, "dc705192e4a205d8c403ae7693290df45f09cec04116ad38f6140f349392548f"));
    CHECK(hh_base_digest(address, 0, digest2) == HH_EMPTY_INPUT);
    CHECK(hh_base_digest(NULL, 4, digest2) == HH_INVALID_ARGUMENT);
    CHECK(hh_base_digest(address, sizeof(address), NULL) == HH_INVALID_ARGUMENT);
    CHECK(hh_base_digest_from_hex("0xzz", 4, digest2) == HH_INVALID_HEX);

    for (i = 0; i < HH_KEY_SIZE; ++i) {
        key[i] = (uint8_t)i;
    }
    CHECK(hh_universal_fingerprint(digest, fp) == HH_OK);
    CHECK(memcmp(fp, digest, 32) == 0);
    CHECK(hh_keyed_fingerprint(digest, key, keyed) == HH_OK);
    CHECK(hex_equals(keyed, 32, "26ea8171aab23c8e1bf7c23417d33d6dba81d881af70edd2b0675348e080b478"));
    CHECK(hh_key_check_value(key, kcv) == HH_OK);
    CHECK(hex_equals(kcv, 4, "6a5955cf"));
    memset(key, 0, sizeof(key));
    CHECK(hh_key_check_value(key, kcv) == HH_INVALID_KEY);
    CHECK(hh_keyed_fingerprint(digest, key, keyed) == HH_INVALID_KEY);

    CHECK(hh_tag(fp, tag) == HH_OK);
    CHECK(strcmp(tag, "TKSPVH") == 0);
    CHECK(hh_layout_of(fp, HH_MODE_UNIVERSAL, &layout) == HH_OK);
    CHECK(layout.mode == HH_MODE_UNIVERSAL);
    CHECK(layout.cells[0].figure == HH_FIGURE_TRIANGLE_LEFT && layout.cells[0].colour == 0);
    CHECK(layout.cells[1].figure == HH_FIGURE_NONE);
    CHECK(layout.palette_rgb[3] == 0xD48200u && layout.frame_rgb == 0x808080u);
    CHECK(hh_layout_of(fp, 3, &layout) == HH_INVALID_FINGERPRINT);

    hh_render_options_init(&options);
    CHECK(options.struct_size == sizeof(options));
    CHECK(options.background[0] == 255 && options.background_alpha == 255 && options.frame_alpha == 255);
    CHECK(hh_render_bytes(15) == 0 && hh_render_bytes(64) == 64u * 64u * 4u);
    bytes = hh_render_bytes(64);
    rgba = (uint8_t*)malloc(bytes);
    CHECK(rgba != NULL);
    if (rgba == NULL) {
        return failures + 1;
    }
    CHECK(hh_render(fp, HH_MODE_UNIVERSAL, 64, NULL, rgba, bytes) == HH_OK);
    CHECK(hh_render(fp, HH_MODE_UNIVERSAL, 64, &options, rgba, bytes - 1) == HH_BUFFER_TOO_SMALL);
    options.frame = HH_FRAME_THICK;
    CHECK(hh_render(fp, HH_MODE_UNIVERSAL, 64, &options, rgba, bytes) == HH_INVALID_FRAME);
    CHECK(hh_render(fp, HH_MODE_KEYED, 64, &options, rgba, bytes) == HH_OK);
    options.frame = 99;
    CHECK(hh_render(fp, HH_MODE_KEYED, 64, &options, rgba, bytes) == HH_INVALID_ARGUMENT);
    hh_render_options_init(&options);
    options.struct_size = 0; /* a structure that was never initialised */
    CHECK(hh_render(fp, HH_MODE_KEYED, 64, &options, rgba, bytes) == HH_INVALID_ARGUMENT);
    CHECK(hh_measure_contrast(&options, NULL, &contrast) == HH_INVALID_ARGUMENT);
    hh_render_options_init(&options);
    options.background[0] = options.background[1] = options.background[2] = 0x9E;
    CHECK(hh_render(fp, HH_MODE_UNIVERSAL, 64, &options, rgba, bytes) == HH_LOW_CONTRAST);
    CHECK(hh_measure_contrast(&options, NULL, &contrast) == HH_OK);
    CHECK(contrast.figures_x100 == 112);
    CHECK(hh_measure_contrast(NULL, NULL, &contrast) == HH_OK);
    CHECK(contrast.figures_x100 == 300 && contrast.frame_x100 == 394);

    CHECK(hh_render(fp, HH_MODE_UNIVERSAL, 64, NULL, rgba, bytes) == HH_OK);
    CHECK(hh_encode_png(rgba, 64, 64, NULL, 0, &encoded_size) == HH_BUFFER_TOO_SMALL);
    CHECK(encoded_size > 8);
    encoded = (uint8_t*)malloc(encoded_size);
    CHECK(encoded != NULL);
    if (encoded != NULL) {
        size_t written = 0;
        CHECK(hh_encode_png(rgba, 64, 64, encoded, encoded_size - 1, &written) == HH_BUFFER_TOO_SMALL);
        CHECK(hh_encode_png(rgba, 64, 64, encoded, encoded_size, &written) == HH_OK);
        CHECK(written == encoded_size && encoded[1] == 'P' && encoded[2] == 'N' && encoded[3] == 'G');
        free(encoded);
    }
    CHECK(hh_encode_bmp(rgba, 64, 64, NULL, NULL, 0, &encoded_size) == HH_BUFFER_TOO_SMALL);
    CHECK(encoded_size == 54u + 64u * 64u * 3u);
    CHECK(hh_encode_jpeg(rgba, 64, 64, 92, NULL, NULL, 0, &encoded_size) == HH_BUFFER_TOO_SMALL);
    CHECK(encoded_size > 600);
    CHECK(hh_encode_jpeg(rgba, 64, 64, 10, NULL, NULL, 0, &encoded_size) == HH_INVALID_QUALITY);
    CHECK(hh_encode_png(rgba, 0, 64, NULL, 0, &encoded_size) == HH_INVALID_IMAGE);
    CHECK(hh_encode_png(NULL, 64, 64, NULL, 0, &encoded_size) == HH_INVALID_ARGUMENT);
    free(rgba);
    return failures;
}
