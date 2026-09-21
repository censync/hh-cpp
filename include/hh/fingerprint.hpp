#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "hh/digest.hpp"
#include "hh/error.hpp"
#include "hh/export.hpp"
#include "hh/key.hpp"
#include "hh/types.hpp"

namespace hh {

namespace detail {
struct fingerprint_access;
}

// Universal pictures are the same for everyone; keyed pictures can be computed
// only with the secret key. The two pictures of one input are unrelated.
enum class mode : std::uint8_t {
    universal = 1,
    keyed = 2,
};

enum class figure : std::uint8_t {
    none = 0,
    square = 1,
    circle = 2,
    triangle_up = 3,
    triangle_right = 4,
    triangle_down = 5,
    triangle_left = 6,
};

// One cell of the 4 x 4 matrix. `colour` is a palette index 0..3 and is 0 for
// an empty cell.
struct cell {
    hh::figure figure;
    std::uint8_t colour;
};

// What a fingerprint shows, for hosts that draw vectors themselves. Cells are
// row-major from the top left; colours are 0xRRGGBB. The raster of render() is
// the canonical form and the only one covered by byte-exact vectors.
struct layout {
    hh::mode mode;
    std::array<cell, 16> cells;
    std::array<std::uint32_t, 4> palette_rgb;
    std::uint32_t frame_rgb;
};

// 32 bytes and the mode they were derived in.
class fingerprint {
public:
    static constexpr std::size_t size = 32;

    // An unset fingerprint. Functions that take it report invalid_fingerprint.
    HH_API fingerprint() noexcept;

    bool empty() const noexcept { return !set_; }
    hh::mode mode() const noexcept { return mode_; }
    const std::array<std::uint8_t, size>& bytes() const noexcept { return bytes_; }

    // The 6-character Crockford Base32 tag followed by a terminating zero
    // ("K7QM2X"); hosts display it as "K7Q-M2X". All '0' if unset.
    HH_API std::array<char, 7> tag_chars() const noexcept;
    HH_API std::string tag() const;

    friend bool operator==(const fingerprint& a, const fingerprint& b) noexcept {
        return a.set_ == b.set_ && a.mode_ == b.mode_ && a.bytes_ == b.bytes_;
    }
    friend bool operator!=(const fingerprint& a, const fingerprint& b) noexcept {
        return !(a == b);
    }

private:
    friend struct detail::fingerprint_access;

    std::array<std::uint8_t, size> bytes_;
    hh::mode mode_;
    bool set_;
};

// The universal fingerprint is the base digest itself.
HH_API error_code universal_fingerprint(const base_digest& digest, fingerprint& out) noexcept;

// The keyed fingerprint: one HMAC of the base digest under the key.
HH_API error_code keyed_fingerprint(const base_digest& digest, const secret_key& key,
                                    fingerprint& out) noexcept;

// For hosts that compute the keyed HMAC elsewhere (for example inside a secure
// element): takes the 32 fingerprint bytes and the mode they belong to.
HH_API error_code import_fingerprint(byte_view bytes32, mode m, fingerprint& out) noexcept;

// The cells, the palette and the mode. An unset fingerprint gives 16 empty cells.
HH_API layout describe(const fingerprint& fp) noexcept;

}  // namespace hh

namespace std {

template <>
struct hash<hh::fingerprint> {
    size_t operator()(const hh::fingerprint& fp) const noexcept {
        // FNV-1a over the mode and the bytes, in the width of size_t.
        constexpr size_t prime =
            static_cast<size_t>(sizeof(size_t) >= 8 ? 1099511628211ull : 16777619ull);
        size_t h = static_cast<size_t>(fp.mode());
        for (unsigned char b : fp.bytes()) {
            h = (h ^ b) * prime;
        }
        return h;
    }
};

}  // namespace std
