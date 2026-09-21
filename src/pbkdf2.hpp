#pragma once

// PBKDF2 with HMAC-SHA-256 as the PRF (RFC 8018 section 5.2). Internal to hh.

#include <cstddef>
#include <cstdint>

namespace hh {
namespace detail {

// pbkdf2_hmac_sha256 derives `out_size` bytes from the password and salt with
// `iterations` rounds. An iteration count of 0 is treated as 1. With a 32-byte
// or shorter output, the cost is two SHA-256 compressions per iteration plus
// a constant (key setup and the first HMAC).
//
// The iteration loop uses the fast compression, which does not wipe its stack:
// hh stretches only the public digest d0. Do not feed this function a secret
// password without changing that.
void pbkdf2_hmac_sha256(const std::uint8_t* password, std::size_t password_size,
                        const std::uint8_t* salt, std::size_t salt_size, std::uint32_t iterations,
                        std::uint8_t* out, std::size_t out_size) noexcept;

}  // namespace detail
}  // namespace hh
