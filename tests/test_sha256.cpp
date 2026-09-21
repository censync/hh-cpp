#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "sha256.hpp"
#include "test_framework.hpp"
#include "test_util.hpp"

using hh::detail::sha256;
using hh::detail::sha256_hash;
using hh_test::bytes_of;
using hh_test::to_hex;

namespace {

std::string hash_hex(const std::vector<std::uint8_t>& message) {
    return to_hex(sha256_hash(message.data(), message.size()));
}

}  // namespace

// FIPS 180-4 examples (NIST CSRC "Examples with Intermediate Values", SHA-256).
TEST_CASE("sha256 fips 180-4 one-block message") {
    EXPECT_EQ(hash_hex(bytes_of("abc")),
              std::string{"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"});
}

TEST_CASE("sha256 fips 180-4 two-block message") {
    EXPECT_EQ(hash_hex(bytes_of("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")),
              std::string{"248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"});
}

TEST_CASE("sha256 fips 180-4 896-bit message") {
    EXPECT_EQ(hash_hex(bytes_of("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
                                "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu")),
              std::string{"cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1"});
}

TEST_CASE("sha256 empty message") {
    EXPECT_EQ(to_hex(sha256_hash(nullptr, 0)),
              std::string{"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"});
}

TEST_CASE("sha256 one million repetitions of a") {
    sha256 h;
    const std::vector<std::uint8_t> chunk(1000, std::uint8_t('a'));
    for (int i = 0; i < 1000; ++i) {
        h.update(chunk.data(), chunk.size());
    }
    EXPECT_EQ(to_hex(h.finish()),
              std::string{"cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"});
}

// Lengths around the padding boundaries (55/56 bytes need one/two final blocks).
// Expected values computed independently with coreutils sha256sum.
TEST_CASE("sha256 lengths around block boundaries") {
    struct vector {
        std::size_t length;
        const char* digest;
    };
    const vector vectors[] = {
        {55, "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318"},
        {56, "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a"},
        {63, "7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34"},
        {64, "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb"},
        {65, "635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0"},
        {119, "31eba51c313a5c08226adf18d4a359cfdfd8d2e816b13f4af952f7ea6584dcfb"},
        {120, "2f3d335432c70b580af0e8e1b3674a7c020d683aa5f73aaaedfdc55af904c21c"},
        {127, "c57e9278af78fa3cab38667bef4ce29d783787a2f731d4e12200270f0c32320a"},
        {128, "6836cf13bac400e9105071cd6af47084dfacad4e5e302c94bfed24e013afb73e"},
    };
    for (const auto& v : vectors) {
        const std::vector<std::uint8_t> message(v.length, std::uint8_t('a'));
        EXPECT_EQ(hash_hex(message), std::string{v.digest});
    }
}

TEST_CASE("sha256 incremental updates match the one-shot digest") {
    std::vector<std::uint8_t> message;
    for (int r = 0; r < 4; ++r) {
        for (int b = 0; b < 256; ++b) {
            message.push_back(std::uint8_t(b));
        }
    }
    const std::string expected{"785b0751fc2c53dc14a4ce3d800e69ef9ce1009eb327ccf458afe09c242c26c9"};
    EXPECT_EQ(hash_hex(message), expected);
    for (std::size_t step = 1; step <= 130; ++step) {
        sha256 h;
        for (std::size_t pos = 0; pos < message.size(); pos += step) {
            const std::size_t n = std::min(step, message.size() - pos);
            h.update(message.data() + pos, n);
        }
        EXPECT_EQ(to_hex(h.finish()), expected);
    }
}

TEST_CASE("sha256 reset allows reuse") {
    sha256 h;
    const auto abc = bytes_of("abc");
    h.update(abc.data(), abc.size());
    const auto first = h.finish();
    h.reset();
    h.update(abc.data(), abc.size());
    EXPECT_EQ(to_hex(h.finish()), to_hex(first));
}
