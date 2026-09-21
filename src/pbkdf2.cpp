#include "pbkdf2.hpp"

#include <algorithm>
#include <array>
#include <cstring>

#include "hmac.hpp"
#include "sha256.hpp"
#include "wipe.hpp"

namespace hh {
namespace detail {

void pbkdf2_hmac_sha256(const std::uint8_t* password, std::size_t password_size,
                        const std::uint8_t* salt, std::size_t salt_size, std::uint32_t iterations,
                        std::uint8_t* out, std::size_t out_size) noexcept {
    const hmac_sha256 prf(password, password_size);

    // Every U_j for j >= 2 is the HMAC of a 32-byte message, so both the inner
    // and the outer hash absorb exactly one more block: 32 bytes of data, the
    // 0x80 terminator and the bit length of 64 + 32 bytes.
    std::array<std::uint8_t, sha256_block_size> block{};
    block[sha256_digest_size] = 0x80;
    constexpr std::uint32_t bit_length = (sha256_block_size + sha256_digest_size) * 8u;
    block[sha256_block_size - 2] = std::uint8_t(bit_length >> 8);
    block[sha256_block_size - 1] = std::uint8_t(bit_length);

    std::array<std::uint8_t, sha256_digest_size> t{};
    sha256_state state{};
    std::uint32_t block_index = 1;
    for (std::size_t offset = 0; offset < out_size; offset += sha256_digest_size, ++block_index) {
        // U_1 = PRF(P, S || INT(i)).
        hmac_sha256 first = prf;
        first.update(salt, salt_size);
        const std::uint8_t index_be[4] = {
            std::uint8_t(block_index >> 24),
            std::uint8_t(block_index >> 16),
            std::uint8_t(block_index >> 8),
            std::uint8_t(block_index),
        };
        first.update(index_be, sizeof(index_be));
        first.finish(block.data());
        std::memcpy(t.data(), block.data(), sha256_digest_size);

        // U_j = PRF(P, U_{j-1}); T = U_1 ^ U_2 ^ ... ^ U_c.
        for (std::uint32_t j = 1; j < iterations; ++j) {
            state = prf.inner_state();
            sha256_compress(state, block.data());
            sha256_store(state, block.data());
            state = prf.outer_state();
            sha256_compress(state, block.data());
            sha256_store(state, block.data());
            for (std::size_t k = 0; k < sha256_digest_size; ++k) {
                t[k] = std::uint8_t(t[k] ^ block[k]);
            }
        }

        const std::size_t take = std::min(sha256_digest_size, out_size - offset);
        std::memcpy(out + offset, t.data(), take);
    }
    secure_wipe(state.data(), sizeof(state));
    secure_wipe(block.data(), block.size());
    secure_wipe(t.data(), t.size());
}

}  // namespace detail
}  // namespace hh
