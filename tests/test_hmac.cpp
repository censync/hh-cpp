#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "hmac.hpp"
#include "test_framework.hpp"
#include "test_util.hpp"

using hh::detail::hmac_sha256;
using hh::detail::hmac_sha256_tag;
using hh_test::bytes_of;
using hh_test::to_hex;

namespace {

std::string tag_hex(const std::vector<std::uint8_t>& key, const std::vector<std::uint8_t>& data) {
    return to_hex(hmac_sha256_tag(key.data(), key.size(), data.data(), data.size()));
}

std::vector<std::uint8_t> repeat(std::uint8_t byte, std::size_t count) {
    return std::vector<std::uint8_t>(count, byte);
}

}  // namespace

// RFC 4231 section 4, HMAC-SHA-256 results of test cases 1 to 7.
TEST_CASE("hmac-sha256 rfc 4231 case 1") {
    EXPECT_EQ(tag_hex(repeat(0x0B, 20), bytes_of("Hi There")),
              std::string{"b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"});
}

TEST_CASE("hmac-sha256 rfc 4231 case 2 (key shorter than the output)") {
    EXPECT_EQ(tag_hex(bytes_of("Jefe"), bytes_of("what do ya want for nothing?")),
              std::string{"5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"});
}

TEST_CASE("hmac-sha256 rfc 4231 case 3 (combined length above the block size)") {
    EXPECT_EQ(tag_hex(repeat(0xAA, 20), repeat(0xDD, 50)),
              std::string{"773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe"});
}

TEST_CASE("hmac-sha256 rfc 4231 case 4 (combined length above the block size)") {
    std::vector<std::uint8_t> key;
    for (std::uint8_t b = 0x01; b <= 0x19; ++b) {
        key.push_back(b);
    }
    EXPECT_EQ(tag_hex(key, repeat(0xCD, 50)),
              std::string{"82558a389a443c0ea4cc819899f2083a85f0faa3e578f8077a2e3ff46729665b"});
}

TEST_CASE("hmac-sha256 rfc 4231 case 5 (truncation to 128 bits)") {
    const std::string tag = tag_hex(repeat(0x0C, 20), bytes_of("Test With Truncation"));
    EXPECT_EQ(tag.substr(0, 32), std::string{"a3b6167473100ee06e0c796c2955552b"});
}

TEST_CASE("hmac-sha256 rfc 4231 case 6 (key larger than the block size)") {
    EXPECT_EQ(tag_hex(repeat(0xAA, 131),
                      bytes_of("Test Using Larger Than Block-Size Key - Hash Key First")),
              std::string{"60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54"});
}

TEST_CASE("hmac-sha256 rfc 4231 case 7 (key and data larger than the block size)") {
    EXPECT_EQ(tag_hex(repeat(0xAA, 131),
                      bytes_of("This is a test using a larger than block-size key and a larger "
                               "than block-size data. The key needs to be hashed before being "
                               "used by the HMAC algorithm.")),
              std::string{"9b09ffa71b942fcb27635fbcd5b0e944bfdc63644f0713938a7f51535c3a35e2"});
}

// Key lengths at the block boundary and the empty key; expected values computed
// independently with Python's hmac module.
TEST_CASE("hmac-sha256 key lengths at the block boundary") {
    EXPECT_EQ(tag_hex(repeat(0x55, 64), bytes_of("hh")),
              std::string{"a3a13d0a104e002f7998293bf9b0edc93adf3aba32109607a5cc6d8c69b2a93e"});
    EXPECT_EQ(tag_hex(repeat(0x55, 65), bytes_of("hh")),
              std::string{"4742601e923f054465a5482683c5df7ac5d98d7b5e8107c3cd7212a5bd049de8"});
    EXPECT_EQ(tag_hex({}, bytes_of("hh")),
              std::string{"1d7771f7c99967222404295da900e866d9351b17739ef7508840e184dbe2bd6f"});
}

// RFC 2104 zero-pads short keys: K and K || 0x00 are the same key. The keyed
// mode of hh therefore takes exactly 32 bytes and rejects the all-zero key.
TEST_CASE("hmac-sha256 zero padding makes trailing zero bytes of a short key irrelevant") {
    const auto data = bytes_of("hh");
    std::vector<std::uint8_t> key = bytes_of("Jefe");
    const std::string plain = tag_hex(key, data);
    key.push_back(0x00);
    EXPECT_EQ(tag_hex(key, data), plain);
    EXPECT_EQ(tag_hex({}, data), tag_hex(repeat(0x00, 32), data));
}

TEST_CASE("hmac-sha256 incremental updates match the one-shot tag") {
    const auto key = repeat(0xAA, 131);
    const auto data = bytes_of(
        "This is a test using a larger than block-size key and a larger "
        "than block-size data. The key needs to be hashed before being "
        "used by the HMAC algorithm.");
    const std::string expected = tag_hex(key, data);
    for (std::size_t step = 1; step <= data.size(); step += 7) {
        hmac_sha256 mac(key.data(), key.size());
        for (std::size_t pos = 0; pos < data.size(); pos += step) {
            mac.update(data.data() + pos, std::min(step, data.size() - pos));
        }
        EXPECT_EQ(to_hex(mac.finish()), expected);
    }
}
