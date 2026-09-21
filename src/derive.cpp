#include "derive.hpp"

#include <algorithm>
#include <cstring>

#include "hmac.hpp"
#include "pbkdf2.hpp"
#include "wipe.hpp"

namespace hh {
namespace detail {

namespace {

constexpr char dst[] = "HumanizedHash";
constexpr std::size_t dst_size = sizeof(dst) - 1;
constexpr char stretch_salt[] = "HumanizedHash/stretch";

constexpr std::uint8_t stage_digest = 0x01;
constexpr std::uint8_t stage_keyed = 0x02;
constexpr std::uint8_t stage_kcv = 0x03;

// Writes DST || 00 || stage and returns the number of bytes written.
std::size_t put_prefix(std::uint8_t* out, std::uint8_t stage) noexcept {
    std::memcpy(out, dst, dst_size);
    out[dst_size] = 0x00;
    out[dst_size + 1] = stage;
    return dst_size + 2;
}

int hex_value(char c) noexcept {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

error_code check_length(std::size_t size) noexcept {
    if (size == 0) {
        return error_code::empty_input;
    }
    if (size > max_input_size) {
        return error_code::input_too_large;
    }
    return error_code::ok;
}

error_code digest_of(input_kind kind, const std::uint8_t* data, std::size_t size,
                     base_digest& out) noexcept {
    digest_access::clear(out);
    if (data == nullptr && size != 0) {
        return error_code::invalid_argument;
    }
    const error_code ec = check_length(size);
    if (ec != error_code::ok) {
        return ec;
    }
    const sha256_digest d0 = derive_d0(kind, data, size);
    const auto s = stretch(d0, stretch_iterations);
    digest_access::set(out, s.data());
    return error_code::ok;
}

}  // namespace

std::array<std::uint8_t, m1_header_size> m1_header(input_kind kind, std::uint32_t length) noexcept {
    std::array<std::uint8_t, m1_header_size> h{};
    std::size_t n = put_prefix(h.data(), stage_digest);
    h[n++] = static_cast<std::uint8_t>(kind);
    h[n++] = static_cast<std::uint8_t>(length >> 24);
    h[n++] = static_cast<std::uint8_t>(length >> 16);
    h[n++] = static_cast<std::uint8_t>(length >> 8);
    h[n++] = static_cast<std::uint8_t>(length);
    return h;
}

sha256_digest derive_d0(input_kind kind, const std::uint8_t* data, std::size_t size) noexcept {
    const auto header = m1_header(kind, static_cast<std::uint32_t>(size));
    sha256 h;
    h.update(header.data(), header.size());
    h.update(data, size);
    return h.finish();
}

std::array<std::uint8_t, 32> stretch(const sha256_digest& d0, std::uint32_t iterations) noexcept {
    std::array<std::uint8_t, 32> s{};
    pbkdf2_hmac_sha256(d0.data(), d0.size(), reinterpret_cast<const std::uint8_t*>(stretch_salt),
                       sizeof(stretch_salt) - 1, iterations, s.data(), s.size());
    return s;
}

std::array<std::uint8_t, m2_size> m2_message(const std::uint8_t* s) noexcept {
    std::array<std::uint8_t, m2_size> m{};
    const std::size_t n = put_prefix(m.data(), stage_keyed);
    std::memcpy(m.data() + n, s, 32);
    return m;
}

std::array<std::uint8_t, 32> keyed_bytes(const std::uint8_t* key, const std::uint8_t* s) noexcept {
    const auto message = m2_message(s);
    return hmac_sha256_tag(key, 32, message.data(), message.size());
}

std::array<std::uint8_t, 4> key_check_value(const std::uint8_t* key) noexcept {
    std::array<std::uint8_t, kcv_message_size> message{};
    put_prefix(message.data(), stage_kcv);
    sha256_digest tag = hmac_sha256_tag(key, 32, message.data(), message.size());
    std::array<std::uint8_t, 4> out{};
    std::copy(tag.begin(), tag.begin() + 4, out.begin());
    secure_wipe(tag.data(), tag.size());
    return out;
}

void digest_access::set(base_digest& d, const std::uint8_t* bytes) noexcept {
    std::memcpy(d.bytes_.data(), bytes, d.bytes_.size());
    d.set_ = true;
}

void digest_access::clear(base_digest& d) noexcept {
    d.bytes_.fill(0);
    d.set_ = false;
}

void key_access::set(secret_key& k, const std::uint8_t* bytes) noexcept {
    std::memcpy(k.bytes_.data(), bytes, k.bytes_.size());
    k.set_ = true;
}

void key_access::clear(secret_key& k) noexcept {
    secure_wipe(k.bytes_.data(), k.bytes_.size());
    k.set_ = false;
}

void fingerprint_access::set(fingerprint& f, const std::uint8_t* bytes, mode m) noexcept {
    std::memcpy(f.bytes_.data(), bytes, f.bytes_.size());
    f.mode_ = m;
    f.set_ = true;
}

void fingerprint_access::clear(fingerprint& f) noexcept {
    f.bytes_.fill(0);
    f.mode_ = mode::universal;
    f.set_ = false;
}

}  // namespace detail

// ---- base_digest -----------------------------------------------------------

base_digest::base_digest() noexcept : bytes_{}, set_(false) {}

error_code make_base_digest(byte_view data, base_digest& out) noexcept {
    return detail::digest_of(detail::input_kind::binary, data.data, data.size, out);
}

error_code make_base_digest_from_text(std::string_view text, base_digest& out) noexcept {
    return detail::digest_of(detail::input_kind::text,
                             reinterpret_cast<const std::uint8_t*>(text.data()), text.size(), out);
}

error_code make_base_digest_from_hex(std::string_view hex, base_digest& out) noexcept {
    detail::digest_access::clear(out);
    if (hex.size() >= 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) {
        hex.remove_prefix(2);
    }
    if (hex.empty() || hex.size() % 2 != 0) {
        return error_code::invalid_hex;
    }
    for (char c : hex) {
        if (detail::hex_value(c) < 0) {
            return error_code::invalid_hex;
        }
    }
    const std::size_t size = hex.size() / 2;
    const error_code ec = detail::check_length(size);
    if (ec != error_code::ok) {
        return ec;
    }
    // Decode in chunks straight into the hash: no allocation, whatever the length.
    const auto header =
        detail::m1_header(detail::input_kind::binary, static_cast<std::uint32_t>(size));
    detail::sha256 h;
    h.update(header.data(), header.size());
    std::array<std::uint8_t, 256> chunk{};
    std::size_t pos = 0;
    while (pos < size) {
        const std::size_t n = std::min(chunk.size(), size - pos);
        for (std::size_t i = 0; i < n; ++i) {
            const int hi = detail::hex_value(hex[2 * (pos + i)]);
            const int lo = detail::hex_value(hex[2 * (pos + i) + 1]);
            chunk[i] = static_cast<std::uint8_t>(hi * 16 + lo);
        }
        h.update(chunk.data(), n);
        pos += n;
    }
    const detail::sha256_digest d0 = h.finish();
    const auto s = detail::stretch(d0, detail::stretch_iterations);
    detail::digest_access::set(out, s.data());
    return error_code::ok;
}

error_code import_base_digest(byte_view bytes32, base_digest& out) noexcept {
    if (bytes32.data == nullptr || bytes32.size != base_digest::size) {
        detail::digest_access::clear(out);
        return error_code::invalid_digest;
    }
    // The view may be the bytes of `out` itself.
    std::array<std::uint8_t, base_digest::size> copy{};
    std::memcpy(copy.data(), bytes32.data, copy.size());
    detail::digest_access::set(out, copy.data());
    return error_code::ok;
}

// ---- secret_key ------------------------------------------------------------

secret_key::secret_key() noexcept : bytes_{}, set_(false) {}

secret_key::~secret_key() {
    detail::secure_wipe(bytes_.data(), bytes_.size());
}

secret_key::secret_key(const secret_key& other) noexcept : bytes_(other.bytes_), set_(other.set_) {}

secret_key& secret_key::operator=(const secret_key& other) noexcept {
    if (this != &other) {
        bytes_ = other.bytes_;
        set_ = other.set_;
    }
    return *this;
}

secret_key::secret_key(secret_key&& other) noexcept : bytes_(other.bytes_), set_(other.set_) {
    detail::key_access::clear(other);
}

secret_key& secret_key::operator=(secret_key&& other) noexcept {
    if (this != &other) {
        bytes_ = other.bytes_;
        set_ = other.set_;
        detail::key_access::clear(other);
    }
    return *this;
}

std::array<std::uint8_t, 4> secret_key::kcv() const noexcept {
    if (!set_) {
        return {};
    }
    return detail::key_check_value(bytes_.data());
}

error_code make_secret_key(byte_view bytes32, secret_key& out) noexcept {
    detail::key_access::clear(out);
    if (bytes32.data == nullptr || bytes32.size != secret_key::size) {
        return error_code::invalid_key;
    }
    std::uint8_t any = 0;
    for (std::size_t i = 0; i < bytes32.size; ++i) {
        any = static_cast<std::uint8_t>(any | bytes32.data[i]);
    }
    if (any == 0) {
        return error_code::invalid_key;
    }
    detail::key_access::set(out, bytes32.data);
    return error_code::ok;
}

// ---- fingerprint -----------------------------------------------------------

fingerprint::fingerprint() noexcept : bytes_{}, mode_(hh::mode::universal), set_(false) {}

error_code universal_fingerprint(const base_digest& digest, fingerprint& out) noexcept {
    detail::fingerprint_access::clear(out);
    if (digest.empty()) {
        return error_code::invalid_digest;
    }
    detail::fingerprint_access::set(out, digest.bytes().data(), mode::universal);
    return error_code::ok;
}

error_code keyed_fingerprint(const base_digest& digest, const secret_key& key,
                             fingerprint& out) noexcept {
    detail::fingerprint_access::clear(out);
    if (digest.empty()) {
        return error_code::invalid_digest;
    }
    if (key.empty()) {
        return error_code::invalid_key;
    }
    auto fp = detail::keyed_bytes(detail::key_access::bytes(key), digest.bytes().data());
    detail::fingerprint_access::set(out, fp.data(), mode::keyed);
    detail::secure_wipe(fp.data(), fp.size());
    return error_code::ok;
}

error_code import_fingerprint(byte_view bytes32, mode m, fingerprint& out) noexcept {
    if (bytes32.data == nullptr || bytes32.size != fingerprint::size ||
        (m != mode::universal && m != mode::keyed)) {
        detail::fingerprint_access::clear(out);
        return error_code::invalid_fingerprint;
    }
    // The view may be the bytes of `out` itself.
    std::array<std::uint8_t, fingerprint::size> copy{};
    std::memcpy(copy.data(), bytes32.data, copy.size());
    detail::fingerprint_access::set(out, copy.data(), m);
    return error_code::ok;
}

}  // namespace hh
