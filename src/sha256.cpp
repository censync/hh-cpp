#include "sha256.hpp"

#include <algorithm>
#include <cstring>

#include "wipe.hpp"

namespace hh {
namespace detail {

namespace {

// The round constants K of FIPS 180-4 section 4.2.2.
constexpr std::uint32_t k[64] = {
    0x428A2F98u, 0x71374491u, 0xB5C0FBCFu, 0xE9B5DBA5u, 0x3956C25Bu, 0x59F111F1u, 0x923F82A4u,
    0xAB1C5ED5u, 0xD807AA98u, 0x12835B01u, 0x243185BEu, 0x550C7DC3u, 0x72BE5D74u, 0x80DEB1FEu,
    0x9BDC06A7u, 0xC19BF174u, 0xE49B69C1u, 0xEFBE4786u, 0x0FC19DC6u, 0x240CA1CCu, 0x2DE92C6Fu,
    0x4A7484AAu, 0x5CB0A9DCu, 0x76F988DAu, 0x983E5152u, 0xA831C66Du, 0xB00327C8u, 0xBF597FC7u,
    0xC6E00BF3u, 0xD5A79147u, 0x06CA6351u, 0x14292967u, 0x27B70A85u, 0x2E1B2138u, 0x4D2C6DFCu,
    0x53380D13u, 0x650A7354u, 0x766A0ABBu, 0x81C2C92Eu, 0x92722C85u, 0xA2BFE8A1u, 0xA81A664Bu,
    0xC24B8B70u, 0xC76C51A3u, 0xD192E819u, 0xD6990624u, 0xF40E3585u, 0x106AA070u, 0x19A4C116u,
    0x1E376C08u, 0x2748774Cu, 0x34B0BCB5u, 0x391C0CB3u, 0x4ED8AA4Au, 0x5B9CCA4Fu, 0x682E6FF3u,
    0x748F82EEu, 0x78A5636Fu, 0x84C87814u, 0x8CC70208u, 0x90BEFFFAu, 0xA4506CEBu, 0xBEF9A3F7u,
    0xC67178F2u,
};

inline std::uint32_t rotr(std::uint32_t x, unsigned n) noexcept {
    return (x >> n) | (x << (32u - n));
}

inline std::uint32_t load_be32(const std::uint8_t* p) noexcept {
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) | (std::uint32_t(p[2]) << 8) |
           std::uint32_t(p[3]);
}

inline void store_be32(std::uint8_t* p, std::uint32_t v) noexcept {
    p[0] = std::uint8_t(v >> 24);
    p[1] = std::uint8_t(v >> 16);
    p[2] = std::uint8_t(v >> 8);
    p[3] = std::uint8_t(v);
}

}  // namespace

namespace {

template <bool wipe>
inline void compress_block(sha256_state& state, const std::uint8_t* block) noexcept {
    std::uint32_t w[64];
    for (std::size_t i = 0; i < 16; ++i) {
        w[i] = load_be32(block + 4 * i);
    }
    for (std::size_t i = 16; i < 64; ++i) {
        const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    // The working variables a..h live in one array so that they can be wiped.
    std::uint32_t v[8];
    for (std::size_t i = 0; i < 8; ++i) {
        v[i] = state[i];
    }
    std::uint32_t& a = v[0];
    std::uint32_t& b = v[1];
    std::uint32_t& c = v[2];
    std::uint32_t& d = v[3];
    std::uint32_t& e = v[4];
    std::uint32_t& f = v[5];
    std::uint32_t& g = v[6];
    std::uint32_t& h = v[7];
    for (std::size_t i = 0; i < 64; ++i) {
        const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const std::uint32_t ch = (e & f) ^ (~e & g);
        const std::uint32_t t1 = h + s1 + ch + k[i] + w[i];
        const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t t2 = s0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
    if (wipe) {
        secure_wipe(w, sizeof(w));
        secure_wipe(v, sizeof(v));
    }
}

}  // namespace

void sha256_compress(sha256_state& state, const std::uint8_t* block) noexcept {
    compress_block<false>(state, block);
}

void sha256_compress_secret(sha256_state& state, const std::uint8_t* block) noexcept {
    compress_block<true>(state, block);
}

void sha256_store(const sha256_state& state, std::uint8_t* out) noexcept {
    for (std::size_t i = 0; i < 8; ++i) {
        store_be32(out + 4 * i, state[i]);
    }
}

sha256::sha256(secrecy s) noexcept
    : state_(sha256_initial_state), buffer_{}, length_(0), buffered_(0), secrecy_(s) {}

sha256::sha256(const sha256_state& state, std::uint64_t length, secrecy s) noexcept
    : state_(state), buffer_{}, length_(length), buffered_(0), secrecy_(s) {}

void sha256::compress(const std::uint8_t* block) noexcept {
    if (secrecy_ == secrecy::secret_data) {
        sha256_compress_secret(state_, block);
    } else {
        sha256_compress(state_, block);
    }
}

sha256::~sha256() {
    secure_wipe(state_.data(), sizeof(state_));
    secure_wipe(buffer_.data(), buffer_.size());
}

void sha256::reset() noexcept {
    state_ = sha256_initial_state;
    secure_wipe(buffer_.data(), buffer_.size());
    length_ = 0;
    buffered_ = 0;
}

void sha256::update(const std::uint8_t* data, std::size_t size) noexcept {
    if (size == 0) {
        return;
    }
    length_ += std::uint64_t(size);
    if (buffered_ != 0) {
        const std::size_t take = std::min(size, sha256_block_size - buffered_);
        std::memcpy(buffer_.data() + buffered_, data, take);
        buffered_ += take;
        data += take;
        size -= take;
        if (buffered_ < sha256_block_size) {
            return;
        }
        compress(buffer_.data());
        buffered_ = 0;
    }
    while (size >= sha256_block_size) {
        compress(data);
        data += sha256_block_size;
        size -= sha256_block_size;
    }
    if (size != 0) {
        std::memcpy(buffer_.data(), data, size);
        buffered_ = size;
    }
}

void sha256::finish(std::uint8_t* out) noexcept {
    // Padding of FIPS 180-4 section 5.1.1: a one bit, zeros, and the message
    // length in bits as a 64-bit big-endian integer.
    const std::uint64_t bit_length = length_ * 8u;
    buffer_[buffered_++] = 0x80;
    if (buffered_ > sha256_block_size - 8) {
        std::memset(buffer_.data() + buffered_, 0, sha256_block_size - buffered_);
        compress(buffer_.data());
        buffered_ = 0;
    }
    std::memset(buffer_.data() + buffered_, 0, sha256_block_size - 8 - buffered_);
    for (std::size_t i = 0; i < 8; ++i) {
        buffer_[sha256_block_size - 1 - i] = std::uint8_t(bit_length >> (8 * i));
    }
    compress(buffer_.data());
    sha256_store(state_, out);
    buffered_ = 0;
}

sha256_digest sha256::finish() noexcept {
    sha256_digest out{};
    finish(out.data());
    return out;
}

sha256_digest sha256_hash(const std::uint8_t* data, std::size_t size) noexcept {
    sha256 h;
    h.update(data, size);
    return h.finish();
}

}  // namespace detail
}  // namespace hh
