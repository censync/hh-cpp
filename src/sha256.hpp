#pragma once

// SHA-256 (FIPS 180-4). Internal to hh; the public API never exposes it directly.

#include <array>
#include <cstddef>
#include <cstdint>

namespace hh {
namespace detail {

constexpr std::size_t sha256_block_size = 64;
constexpr std::size_t sha256_digest_size = 32;

using sha256_digest = std::array<std::uint8_t, sha256_digest_size>;
using sha256_state = std::array<std::uint32_t, 8>;

// The initial hash value H(0) of FIPS 180-4 section 5.3.3.
constexpr sha256_state sha256_initial_state = {
    0x6A09E667u, 0xBB67AE85u, 0x3C6EF372u, 0xA54FF53Au,
    0x510E527Fu, 0x9B05688Cu, 0x1F83D9ABu, 0x5BE0CD19u,
};

// sha256_compress applies the compression function to one 64-byte block. It
// leaves the message schedule and the working variables on the stack, so it is
// for public data only.
void sha256_compress(sha256_state& state, const std::uint8_t* block) noexcept;

// The same, but the message schedule and the working variables are wiped. For
// blocks or states that depend on a key: the schedule of the first HMAC block
// is the key itself, and the working variables of any keyed block can be run
// backwards to the key-equivalent state.
void sha256_compress_secret(sha256_state& state, const std::uint8_t* block) noexcept;

// Whether a hasher processes public or key-dependent data.
enum class secrecy {
    public_data,
    secret_data,
};

// sha256_store writes the state as the big-endian 32-byte digest.
void sha256_store(const sha256_state& state, std::uint8_t* out) noexcept;

// sha256 is the incremental hasher. After finish() the object must be reset()
// before it is used again.
class sha256 {
public:
    explicit sha256(secrecy s = secrecy::public_data) noexcept;
    // Resumes hashing from a mid-state. `length` is the number of bytes
    // already absorbed into `state` and must be a multiple of the block size.
    sha256(const sha256_state& state, std::uint64_t length, secrecy s) noexcept;
    ~sha256();

    sha256(const sha256&) = default;
    sha256& operator=(const sha256&) = default;

    void reset() noexcept;
    void update(const std::uint8_t* data, std::size_t size) noexcept;
    void finish(std::uint8_t* out) noexcept;
    sha256_digest finish() noexcept;

private:
    sha256_state state_;
    std::array<std::uint8_t, sha256_block_size> buffer_;
    std::uint64_t length_;
    std::size_t buffered_;
    secrecy secrecy_;

    void compress(const std::uint8_t* block) noexcept;
};

// sha256_hash is the one-shot convenience form.
sha256_digest sha256_hash(const std::uint8_t* data, std::size_t size) noexcept;

}  // namespace detail
}  // namespace hh
