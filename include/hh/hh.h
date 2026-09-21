/*
 * C ABI of hh (Humanized Hash), for bindings such as JNI, Swift, Go and Python.
 *
 * Plain data only: fixed-size byte arrays, integers and caller-allocated
 * buffers. Every function returns an hh_error (0 is success) and is safe to
 * call from any thread; the library keeps no global state. docs/SPEC.md
 * defines the algorithm, docs/INTEGRATION.md shows the calls in context.
 */
#ifndef HH_HH_H
#define HH_HH_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(HH_SHARED)
#if defined(HH_BUILDING)
#define HH_C_API __declspec(dllexport)
#else
#define HH_C_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define HH_C_API __attribute__((visibility("default")))
#else
#define HH_C_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum hh_error {
    HH_OK = 0,
    HH_EMPTY_INPUT = 1,
    HH_INPUT_TOO_LARGE = 2,
    HH_INVALID_HEX = 3,
    HH_INVALID_KEY = 4,
    HH_INVALID_DIGEST = 5,
    HH_INVALID_FINGERPRINT = 6,
    HH_INVALID_SIZE = 7,
    HH_INVALID_FRAME = 8,
    HH_LOW_CONTRAST = 9,
    HH_INVALID_QUALITY = 10,
    HH_INVALID_IMAGE = 11,
    HH_BUFFER_TOO_SMALL = 12,
    HH_OUT_OF_MEMORY = 13,
    HH_INVALID_ARGUMENT = 14
} hh_error;

enum { HH_MODE_UNIVERSAL = 1, HH_MODE_KEYED = 2 };
enum { HH_SHAPE_SQUARE = 0, HH_SHAPE_ROUND = 1 };
enum {
    HH_FRAME_AUTOMATIC = 0,
    HH_FRAME_NONE = 1,
    HH_FRAME_PLAIN = 2,
    HH_FRAME_ROUNDED = 3,
    HH_FRAME_CHAMFERED = 4,
    HH_FRAME_DOUBLE = 5,
    HH_FRAME_THICK = 6,
    HH_FRAME_BRACKETS = 7,
    HH_FRAME_TICKS = 8,
    HH_FRAME_GAPS = 9
};
enum {
    HH_FIGURE_NONE = 0,
    HH_FIGURE_SQUARE = 1,
    HH_FIGURE_CIRCLE = 2,
    HH_FIGURE_TRIANGLE_UP = 3,
    HH_FIGURE_TRIANGLE_RIGHT = 4,
    HH_FIGURE_TRIANGLE_DOWN = 5,
    HH_FIGURE_TRIANGLE_LEFT = 6
};

#define HH_DIGEST_SIZE 32
#define HH_KEY_SIZE 32
#define HH_FINGERPRINT_SIZE 32
#define HH_KCV_SIZE 4
#define HH_TAG_SIZE 7 /* six characters and a terminating zero */

/*
 * Always initialise with hh_render_options_init(): it sets struct_size, which
 * lets the structure grow in later releases without breaking old callers.
 */
typedef struct hh_render_options {
    uint32_t struct_size;     /* sizeof(hh_render_options) of the caller */
    uint8_t shape;            /* HH_SHAPE_* */
    uint8_t frame;            /* HH_FRAME_* */
    uint8_t background[3];    /* sRGB: R, G, B */
    uint8_t background_alpha; /* 0 transparent .. 255 opaque */
    uint8_t frame_alpha;
} hh_render_options;

typedef struct hh_cell {
    uint8_t figure; /* HH_FIGURE_* */
    uint8_t colour; /* palette index 0..3, 0 for an empty cell */
} hh_cell;

typedef struct hh_layout {
    uint8_t mode;
    hh_cell cells[16];       /* row-major from the top left */
    uint32_t palette_rgb[4]; /* 0xRRGGBB */
    uint32_t frame_rgb;
} hh_layout;

typedef struct hh_contrast {
    uint32_t figures_x100; /* WCAG contrast times 100 of the weakest palette colour */
    uint32_t frame_x100;
} hh_contrast;

/* The library version, "major.minor.patch". */
HH_C_API const char* hh_version(void);

/* A short English description, and the specification's name, of an error. Never null. */
HH_C_API const char* hh_error_message(int error);
HH_C_API const char* hh_error_name(int error);

/* The base digest: slow (about 16 000 HMAC calls), public, cacheable per input. */
HH_C_API int hh_base_digest(const uint8_t* data, size_t size, uint8_t out_digest[HH_DIGEST_SIZE]);
HH_C_API int hh_base_digest_from_hex(const char* hex, size_t length,
                                     uint8_t out_digest[HH_DIGEST_SIZE]);
HH_C_API int hh_base_digest_from_text(const char* utf8, size_t length,
                                      uint8_t out_digest[HH_DIGEST_SIZE]);

/* The universal fingerprint is the digest itself; the keyed one is an HMAC under the key. */
HH_C_API int hh_universal_fingerprint(const uint8_t digest[HH_DIGEST_SIZE],
                                      uint8_t out_fingerprint[HH_FINGERPRINT_SIZE]);
HH_C_API int hh_keyed_fingerprint(const uint8_t digest[HH_DIGEST_SIZE],
                                  const uint8_t key[HH_KEY_SIZE],
                                  uint8_t out_fingerprint[HH_FINGERPRINT_SIZE]);

/* The key check value; HH_INVALID_KEY for an all-zero key. */
HH_C_API int hh_key_check_value(const uint8_t key[HH_KEY_SIZE], uint8_t out_kcv[HH_KCV_SIZE]);

HH_C_API int hh_tag(const uint8_t fingerprint[HH_FINGERPRINT_SIZE], char out_tag[HH_TAG_SIZE]);
HH_C_API int hh_layout_of(const uint8_t fingerprint[HH_FINGERPRINT_SIZE], int mode,
                          hh_layout* out_layout);

/* White opaque background, square shape, automatic frame, opaque frame. */
HH_C_API void hh_render_options_init(hh_render_options* options);

/* The number of bytes hh_render writes for a size: size * size * 4, or 0 if out of range. */
HH_C_API size_t hh_render_bytes(uint32_t size);

/* RGBA8888 with straight alpha, row-major from the top left. `options` may be null. */
HH_C_API int hh_render(const uint8_t fingerprint[HH_FINGERPRINT_SIZE], int mode, uint32_t size,
                       const hh_render_options* options, uint8_t* out_rgba, size_t capacity);

/* `page` is the sRGB colour under a translucent background; it may be null (white). */
HH_C_API int hh_measure_contrast(const hh_render_options* options, const uint8_t* page,
                                 hh_contrast* out_contrast);

/*
 * Encoders. `rgba` holds width * height * 4 bytes. `*out_size` receives the
 * encoded size whenever the input is valid. With `out` null or `capacity` too
 * small the result is HH_BUFFER_TOO_SMALL and nothing is written, so a first
 * call with a null buffer is the size query; it costs a full encode. `matte` is
 * the sRGB colour transparent pixels are flattened over; null is white.
 */
HH_C_API int hh_encode_png(const uint8_t* rgba, uint32_t width, uint32_t height, uint8_t* out,
                           size_t capacity, size_t* out_size);
HH_C_API int hh_encode_bmp(const uint8_t* rgba, uint32_t width, uint32_t height,
                           const uint8_t* matte, uint8_t* out, size_t capacity, size_t* out_size);
HH_C_API int hh_encode_jpeg(const uint8_t* rgba, uint32_t width, uint32_t height, int quality,
                            const uint8_t* matte, uint8_t* out, size_t capacity,
                            size_t* out_size);

#ifdef __cplusplus
}
#endif

#endif /* HH_HH_H */
