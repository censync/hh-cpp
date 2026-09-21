#pragma once

// HMAC-SHA-256 (RFC 2104, FIPS 198-1). Internal to hh.

#include <cstddef>
#include <cstdint>

#include "sha256.hpp"

namespace hh {
namespace detail {

// hmac_sha256 computes HMAC-SHA-256 incrementally. The key is processed once
// in the constructor into the inner and outer mid-states; the key bytes are
// not retained. Keys longer than the block size are hashed first, shorter
// keys are zero-padded, as RFC 2104 specifies. Every compression that touches
// the key or a key-dependent state wipes its stack.
class hmac_sha256 {
public:
    hmac_sha256(const std::uint8_t* key, std::size_t key_size) noexcept;
    ~hmac_sha256();

    hmac_sha256(const hmac_sha256&) = default;
    hmac_sha256& operator=(const hmac_sha256&) = default;

    void update(const std::uint8_t* data, std::size_t size) noexcept;

    // finish writes the 32-byte tag. The object must not be updated afterwards.
    void finish(std::uint8_t* out) noexcept;
    sha256_digest finish() noexcept;

    // The mid-states after absorbing key ^ ipad and key ^ opad. PBKDF2 uses
    // them to run each iteration as exactly two compressions.
    const sha256_state& inner_state() const noexcept { return inner_state_; }
    const sha256_state& outer_state() const noexcept { return outer_state_; }

private:
    sha256_state inner_state_;
    sha256_state outer_state_;
    sha256 inner_;
};

// hmac_sha256_tag is the one-shot convenience form.
sha256_digest hmac_sha256_tag(const std::uint8_t* key, std::size_t key_size,
                              const std::uint8_t* data, std::size_t size) noexcept;

}  // namespace detail
}  // namespace hh
