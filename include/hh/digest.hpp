#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "hh/error.hpp"
#include "hh/export.hpp"
#include "hh/types.hpp"

namespace hh {

namespace detail {
struct digest_access;
}

// The base digest `s`: the stretched, public 32-byte value of an input
// (SPEC.md section 4). It is the only slow step (about 16 000 HMAC calls), so
// hosts cache it per input; both modes and any key derive from it cheaply.
class base_digest {
public:
    static constexpr std::size_t size = 32;

    // An unset digest. Functions that take it report invalid_digest.
    HH_API base_digest() noexcept;

    bool empty() const noexcept { return !set_; }

    // The 32 bytes; all zero if the digest is unset.
    const std::array<std::uint8_t, size>& bytes() const noexcept { return bytes_; }

    friend bool operator==(const base_digest& a, const base_digest& b) noexcept {
        return a.set_ == b.set_ && a.bytes_ == b.bytes_;
    }
    friend bool operator!=(const base_digest& a, const base_digest& b) noexcept {
        return !(a == b);
    }

private:
    friend struct detail::digest_access;

    std::array<std::uint8_t, size> bytes_;
    bool set_;
};

// Binary input: the bytes of an address, a public key or a hash.
HH_API error_code make_base_digest(byte_view data, base_digest& out) noexcept;

// Binary input given as hexadecimal text: optional "0x" or "0X", then an even,
// non-zero number of hex digits of either case.
HH_API error_code make_base_digest_from_hex(std::string_view hex, base_digest& out) noexcept;

// Text input: the UTF-8 bytes of `text`, verbatim. Text and binary inputs with
// equal bytes give different digests.
HH_API error_code make_base_digest_from_text(std::string_view text, base_digest& out) noexcept;

// Restores a cached digest from its 32 bytes.
HH_API error_code import_base_digest(byte_view bytes32, base_digest& out) noexcept;

}  // namespace hh
