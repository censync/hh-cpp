#pragma once

// Canonicalisation and derivation (SPEC.md sections 3 and 4). Internal.

#include <array>
#include <cstddef>
#include <cstdint>

#include <hh/digest.hpp>
#include <hh/fingerprint.hpp>
#include <hh/key.hpp>

#include "sha256.hpp"

namespace hh {
namespace detail {

constexpr std::uint32_t stretch_iterations = 16384;
constexpr std::size_t max_input_size = 1048576;

enum class input_kind : std::uint8_t {
    binary = 0x00,
    text = 0x01,
};

constexpr std::size_t m1_header_size = 20;
constexpr std::size_t m2_size = 47;
constexpr std::size_t kcv_message_size = 15;

// M1 without the data: DST || 00 || 01 || kind || u32be(length).
std::array<std::uint8_t, m1_header_size> m1_header(input_kind kind, std::uint32_t length) noexcept;

// d0 = SHA-256(M1).
sha256_digest derive_d0(input_kind kind, const std::uint8_t* data, std::size_t size) noexcept;

// s = PBKDF2-HMAC-SHA-256(d0, "HumanizedHash/stretch", iterations, 32). The library always
// uses stretch_iterations; tests pass smaller counts to keep statistical runs short.
std::array<std::uint8_t, 32> stretch(const sha256_digest& d0, std::uint32_t iterations) noexcept;

// M2 = DST || 00 || 02 || s.
std::array<std::uint8_t, m2_size> m2_message(const std::uint8_t* s) noexcept;

// fp = HMAC-SHA-256(key, M2).
std::array<std::uint8_t, 32> keyed_bytes(const std::uint8_t* key, const std::uint8_t* s) noexcept;

// The first 4 bytes of HMAC-SHA-256(key, DST || 00 || 03).
std::array<std::uint8_t, 4> key_check_value(const std::uint8_t* key) noexcept;

// Access to the private state of the public value types.
struct digest_access {
    static void set(base_digest& d, const std::uint8_t* bytes) noexcept;
    static void clear(base_digest& d) noexcept;
};

struct key_access {
    static const std::uint8_t* bytes(const secret_key& k) noexcept { return k.bytes_.data(); }
    static void set(secret_key& k, const std::uint8_t* bytes) noexcept;
    static void clear(secret_key& k) noexcept;
};

struct fingerprint_access {
    static void set(fingerprint& f, const std::uint8_t* bytes, mode m) noexcept;
    static void clear(fingerprint& f) noexcept;
};

}  // namespace detail
}  // namespace hh
