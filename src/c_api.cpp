// The C ABI (include/hh/hh.h): a thin layer over the C++ API.

#include <cstring>
#include <vector>

#include <hh/hh.h>
#include <hh/hh.hpp>

#include "encode_util.hpp"

namespace {

// The C constants are the C++ enumerators.
static_assert(HH_OK == static_cast<int>(hh::error_code::ok), "hh_error");
static_assert(HH_EMPTY_INPUT == static_cast<int>(hh::error_code::empty_input), "hh_error");
static_assert(HH_INPUT_TOO_LARGE == static_cast<int>(hh::error_code::input_too_large), "hh_error");
static_assert(HH_INVALID_HEX == static_cast<int>(hh::error_code::invalid_hex), "hh_error");
static_assert(HH_INVALID_KEY == static_cast<int>(hh::error_code::invalid_key), "hh_error");
static_assert(HH_INVALID_DIGEST == static_cast<int>(hh::error_code::invalid_digest), "hh_error");
static_assert(HH_INVALID_FINGERPRINT == static_cast<int>(hh::error_code::invalid_fingerprint),
              "hh_error");
static_assert(HH_INVALID_SIZE == static_cast<int>(hh::error_code::invalid_size), "hh_error");
static_assert(HH_INVALID_FRAME == static_cast<int>(hh::error_code::invalid_frame), "hh_error");
static_assert(HH_LOW_CONTRAST == static_cast<int>(hh::error_code::low_contrast), "hh_error");
static_assert(HH_INVALID_QUALITY == static_cast<int>(hh::error_code::invalid_quality), "hh_error");
static_assert(HH_INVALID_IMAGE == static_cast<int>(hh::error_code::invalid_image), "hh_error");
static_assert(HH_BUFFER_TOO_SMALL == static_cast<int>(hh::error_code::buffer_too_small),
              "hh_error");
static_assert(HH_OUT_OF_MEMORY == static_cast<int>(hh::error_code::out_of_memory), "hh_error");
static_assert(HH_INVALID_ARGUMENT == static_cast<int>(hh::error_code::invalid_argument),
              "hh_error");
static_assert(HH_MODE_UNIVERSAL == static_cast<int>(hh::mode::universal), "mode");
static_assert(HH_MODE_KEYED == static_cast<int>(hh::mode::keyed), "mode");
static_assert(HH_SHAPE_SQUARE == static_cast<int>(hh::image_shape::square), "shape");
static_assert(HH_SHAPE_ROUND == static_cast<int>(hh::image_shape::round), "shape");
static_assert(HH_FRAME_AUTOMATIC == static_cast<int>(hh::frame_style::automatic), "frame");
static_assert(HH_FRAME_NONE == static_cast<int>(hh::frame_style::none), "frame");
static_assert(HH_FRAME_PLAIN == static_cast<int>(hh::frame_style::plain), "frame");
static_assert(HH_FRAME_ROUNDED == static_cast<int>(hh::frame_style::rounded), "frame");
static_assert(HH_FRAME_CHAMFERED == static_cast<int>(hh::frame_style::chamfered), "frame");
static_assert(HH_FRAME_DOUBLE == static_cast<int>(hh::frame_style::double_line), "frame");
static_assert(HH_FRAME_THICK == static_cast<int>(hh::frame_style::thick), "frame");
static_assert(HH_FRAME_BRACKETS == static_cast<int>(hh::frame_style::brackets), "frame");
static_assert(HH_FRAME_TICKS == static_cast<int>(hh::frame_style::ticks), "frame");
static_assert(HH_FRAME_GAPS == static_cast<int>(hh::frame_style::gaps), "frame");
static_assert(HH_FIGURE_TRIANGLE_LEFT == static_cast<int>(hh::figure::triangle_left), "figure");
static_assert(HH_DIGEST_SIZE == hh::base_digest::size && HH_KEY_SIZE == hh::secret_key::size &&
                  HH_FINGERPRINT_SIZE == hh::fingerprint::size,
              "sizes");

int code(hh::error_code ec) noexcept {
    return static_cast<int>(ec);
}

hh::error_code digest_out(hh::error_code ec, const hh::base_digest& digest,
                          std::uint8_t* out) noexcept {
    if (ec == hh::error_code::ok) {
        std::memcpy(out, digest.bytes().data(), hh::base_digest::size);
    }
    return ec;
}

hh::rgb colour_or_white(const std::uint8_t* c) noexcept {
    return c == nullptr ? hh::rgb{255, 255, 255} : hh::rgb{c[0], c[1], c[2]};
}

// Null means the defaults. A structure that was not initialised, or that comes from
// a release whose fields this one does not know, is refused.
bool options_of(const hh_render_options* o, hh::render_options& out) noexcept {
    out = hh::render_options{};
    if (o == nullptr) {
        return true;
    }
    if (o->struct_size != sizeof(hh_render_options)) {
        return false;
    }
    out.shape = static_cast<hh::image_shape>(o->shape);
    out.frame = static_cast<hh::frame_style>(o->frame);
    out.background = {o->background[0], o->background[1], o->background[2]};
    out.background_alpha = o->background_alpha;
    out.frame_alpha = o->frame_alpha;
    return true;
}

hh::error_code make_fingerprint(const std::uint8_t* bytes, int mode, hh::fingerprint& fp) noexcept {
    if (bytes == nullptr) {
        return hh::error_code::invalid_argument;
    }
    if (mode != HH_MODE_UNIVERSAL && mode != HH_MODE_KEYED) {
        return hh::error_code::invalid_fingerprint;
    }
    return hh::import_fingerprint({bytes, hh::fingerprint::size}, static_cast<hh::mode>(mode), fp);
}

// Encodes straight from the caller's pixels; only the output is allocated.
template <typename Encoder>
int encode_with(const std::uint8_t* rgba, std::uint32_t width, std::uint32_t height,
                std::uint8_t* out, std::size_t capacity, std::size_t* out_size,
                Encoder encoder) noexcept {
    if (rgba == nullptr || out_size == nullptr) {
        return HH_INVALID_ARGUMENT;
    }
    *out_size = 0;
    if (!hh::detail::valid_dimensions(width, height)) {
        return HH_INVALID_IMAGE;
    }
    std::vector<std::uint8_t> encoded;
    const hh::error_code ec = encoder(hh::detail::pixel_view{rgba, width, height}, encoded);
    if (ec != hh::error_code::ok) {
        return code(ec);
    }
    *out_size = encoded.size();
    if (out == nullptr || capacity < encoded.size()) {
        return HH_BUFFER_TOO_SMALL;
    }
    std::memcpy(out, encoded.data(), encoded.size());
    return HH_OK;
}

}  // namespace

extern "C" {

const char* hh_version(void) {
    return hh::version();
}

const char* hh_error_message(int error) {
    return hh::error_message(static_cast<hh::error_code>(error));
}

const char* hh_error_name(int error) {
    return hh::error_name(static_cast<hh::error_code>(error));
}

int hh_base_digest(const uint8_t* data, size_t size, uint8_t* out_digest) {
    if (out_digest == nullptr || (data == nullptr && size != 0)) {
        return HH_INVALID_ARGUMENT;
    }
    hh::base_digest digest;
    return code(digest_out(hh::make_base_digest({data, size}, digest), digest, out_digest));
}

int hh_base_digest_from_hex(const char* hex, size_t length, uint8_t* out_digest) {
    if (out_digest == nullptr || (hex == nullptr && length != 0)) {
        return HH_INVALID_ARGUMENT;
    }
    hh::base_digest digest;
    const std::string_view view =
        hex == nullptr ? std::string_view{} : std::string_view{hex, length};
    return code(digest_out(hh::make_base_digest_from_hex(view, digest), digest, out_digest));
}

int hh_base_digest_from_text(const char* utf8, size_t length, uint8_t* out_digest) {
    if (out_digest == nullptr || (utf8 == nullptr && length != 0)) {
        return HH_INVALID_ARGUMENT;
    }
    hh::base_digest digest;
    const std::string_view view =
        utf8 == nullptr ? std::string_view{} : std::string_view{utf8, length};
    return code(digest_out(hh::make_base_digest_from_text(view, digest), digest, out_digest));
}

int hh_universal_fingerprint(const uint8_t* digest, uint8_t* out_fingerprint) {
    if (digest == nullptr || out_fingerprint == nullptr) {
        return HH_INVALID_ARGUMENT;
    }
    std::memmove(out_fingerprint, digest, HH_FINGERPRINT_SIZE);
    return HH_OK;
}

int hh_keyed_fingerprint(const uint8_t* digest, const uint8_t* key, uint8_t* out_fingerprint) {
    if (digest == nullptr || key == nullptr || out_fingerprint == nullptr) {
        return HH_INVALID_ARGUMENT;
    }
    hh::base_digest d;
    hh::secret_key k;
    hh::fingerprint fp;
    hh::error_code ec = hh::import_base_digest({digest, HH_DIGEST_SIZE}, d);
    if (ec == hh::error_code::ok) {
        ec = hh::make_secret_key({key, HH_KEY_SIZE}, k);
    }
    if (ec == hh::error_code::ok) {
        ec = hh::keyed_fingerprint(d, k, fp);
    }
    if (ec == hh::error_code::ok) {
        std::memcpy(out_fingerprint, fp.bytes().data(), HH_FINGERPRINT_SIZE);
    }
    return code(ec);
}

int hh_key_check_value(const uint8_t* key, uint8_t* out_kcv) {
    if (key == nullptr || out_kcv == nullptr) {
        return HH_INVALID_ARGUMENT;
    }
    hh::secret_key k;
    const hh::error_code ec = hh::make_secret_key({key, HH_KEY_SIZE}, k);
    if (ec == hh::error_code::ok) {
        const auto kcv = k.kcv();
        std::memcpy(out_kcv, kcv.data(), kcv.size());
    }
    return code(ec);
}

int hh_tag(const uint8_t* fingerprint, char* out_tag) {
    if (out_tag == nullptr) {
        return HH_INVALID_ARGUMENT;
    }
    hh::fingerprint fp;
    const hh::error_code ec = make_fingerprint(fingerprint, HH_MODE_UNIVERSAL, fp);
    if (ec == hh::error_code::ok) {
        const auto tag = fp.tag_chars();
        std::memcpy(out_tag, tag.data(), tag.size());
    }
    return code(ec);
}

int hh_layout_of(const uint8_t* fingerprint, int mode, hh_layout* out_layout) {
    if (out_layout == nullptr) {
        return HH_INVALID_ARGUMENT;
    }
    hh::fingerprint fp;
    const hh::error_code ec = make_fingerprint(fingerprint, mode, fp);
    if (ec != hh::error_code::ok) {
        return code(ec);
    }
    const hh::layout l = hh::describe(fp);
    out_layout->mode = static_cast<uint8_t>(l.mode);
    for (std::size_t i = 0; i < l.cells.size(); ++i) {
        out_layout->cells[i].figure = static_cast<uint8_t>(l.cells[i].figure);
        out_layout->cells[i].colour = l.cells[i].colour;
    }
    for (std::size_t i = 0; i < l.palette_rgb.size(); ++i) {
        out_layout->palette_rgb[i] = l.palette_rgb[i];
    }
    out_layout->frame_rgb = l.frame_rgb;
    return HH_OK;
}

void hh_render_options_init(hh_render_options* options) {
    if (options == nullptr) {
        return;
    }
    options->struct_size = sizeof(hh_render_options);
    options->shape = HH_SHAPE_SQUARE;
    options->frame = HH_FRAME_AUTOMATIC;
    options->background[0] = 255;
    options->background[1] = 255;
    options->background[2] = 255;
    options->background_alpha = 255;
    options->frame_alpha = 255;
}

size_t hh_render_bytes(uint32_t size) {
    if (size < hh::min_image_size || size > hh::max_image_size) {
        return 0;
    }
    return static_cast<size_t>(size) * size * 4;
}

int hh_render(const uint8_t* fingerprint, int mode, uint32_t size, const hh_render_options* options,
              uint8_t* out_rgba, size_t capacity) {
    if (out_rgba == nullptr) {
        return HH_INVALID_ARGUMENT;
    }
    hh::fingerprint fp;
    const hh::error_code ec = make_fingerprint(fingerprint, mode, fp);
    if (ec != hh::error_code::ok) {
        return code(ec);
    }
    hh::render_options o;
    if (!options_of(options, o)) {
        return HH_INVALID_ARGUMENT;
    }
    return code(hh::render_into(fp, size, o, out_rgba, capacity));
}

int hh_measure_contrast(const hh_render_options* options, const uint8_t* page,
                        hh_contrast* out_contrast) {
    if (out_contrast == nullptr) {
        return HH_INVALID_ARGUMENT;
    }
    hh::render_options o;
    if (!options_of(options, o)) {
        return HH_INVALID_ARGUMENT;
    }
    const hh::contrast_report r = hh::measure_contrast(o, colour_or_white(page));
    out_contrast->figures_x100 = r.figures_x100;
    out_contrast->frame_x100 = r.frame_x100;
    return HH_OK;
}

int hh_encode_png(const uint8_t* rgba, uint32_t width, uint32_t height, uint8_t* out,
                  size_t capacity, size_t* out_size) {
    return encode_with(rgba, width, height, out, capacity, out_size,
                       [](hh::detail::pixel_view img, std::vector<std::uint8_t>& encoded) {
                           return hh::detail::encode_png_view(img, encoded);
                       });
}

int hh_encode_bmp(const uint8_t* rgba, uint32_t width, uint32_t height, const uint8_t* matte,
                  uint8_t* out, size_t capacity, size_t* out_size) {
    const hh::rgb m = colour_or_white(matte);
    return encode_with(rgba, width, height, out, capacity, out_size,
                       [m](hh::detail::pixel_view img, std::vector<std::uint8_t>& encoded) {
                           return hh::detail::encode_bmp_view(img, m, encoded);
                       });
}

int hh_encode_jpeg(const uint8_t* rgba, uint32_t width, uint32_t height, int quality,
                   const uint8_t* matte, uint8_t* out, size_t capacity, size_t* out_size) {
    const hh::rgb m = colour_or_white(matte);
    return encode_with(
        rgba, width, height, out, capacity, out_size,
        [m, quality](hh::detail::pixel_view img, std::vector<std::uint8_t>& encoded) {
            return hh::detail::encode_jpeg_view(img, quality, m, encoded);
        });
}

}  // extern "C"
