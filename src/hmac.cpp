#include "hmac.hpp"

#include <array>
#include <cstring>

#include "wipe.hpp"

namespace hh {
namespace detail {

namespace {

// Writes the mid-state straight into its destination: a returned temporary
// would be a key-equivalent copy the caller cannot wipe.
void absorb_padded_key(const std::array<std::uint8_t, sha256_block_size>& key_block,
                       std::uint8_t pad, sha256_state& state) noexcept {
    std::array<std::uint8_t, sha256_block_size> block{};
    for (std::size_t i = 0; i < sha256_block_size; ++i) {
        block[i] = std::uint8_t(key_block[i] ^ pad);
    }
    state = sha256_initial_state;
    sha256_compress_secret(state, block.data());
    secure_wipe(block.data(), block.size());
}

}  // namespace

hmac_sha256::hmac_sha256(const std::uint8_t* key, std::size_t key_size) noexcept
    : inner_state_{}, outer_state_{}, inner_(secrecy::secret_data) {
    std::array<std::uint8_t, sha256_block_size> key_block{};
    if (key_size > sha256_block_size) {
        sha256 hasher(secrecy::secret_data);
        hasher.update(key, key_size);
        hasher.finish(key_block.data());
    } else if (key_size != 0) {
        std::memcpy(key_block.data(), key, key_size);
    }
    absorb_padded_key(key_block, 0x36, inner_state_);
    absorb_padded_key(key_block, 0x5C, outer_state_);
    secure_wipe(key_block.data(), key_block.size());
    inner_ = sha256(inner_state_, sha256_block_size, secrecy::secret_data);
}

hmac_sha256::~hmac_sha256() {
    secure_wipe(inner_state_.data(), sizeof(inner_state_));
    secure_wipe(outer_state_.data(), sizeof(outer_state_));
}

void hmac_sha256::update(const std::uint8_t* data, std::size_t size) noexcept {
    inner_.update(data, size);
}

void hmac_sha256::finish(std::uint8_t* out) noexcept {
    sha256_digest inner_digest = inner_.finish();
    sha256 outer(outer_state_, sha256_block_size, secrecy::secret_data);
    outer.update(inner_digest.data(), inner_digest.size());
    outer.finish(out);
    secure_wipe(inner_digest.data(), inner_digest.size());
}

sha256_digest hmac_sha256::finish() noexcept {
    sha256_digest out{};
    finish(out.data());
    return out;
}

sha256_digest hmac_sha256_tag(const std::uint8_t* key, std::size_t key_size,
                              const std::uint8_t* data, std::size_t size) noexcept {
    hmac_sha256 mac(key, key_size);
    mac.update(data, size);
    return mac.finish();
}

}  // namespace detail
}  // namespace hh
