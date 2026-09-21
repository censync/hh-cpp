#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "pbkdf2.hpp"
#include "test_framework.hpp"
#include "test_util.hpp"

using hh::detail::pbkdf2_hmac_sha256;
using hh_test::bytes_of;
using hh_test::to_hex;

namespace {

std::string derive_hex(const char* password, const char* salt, std::uint32_t iterations,
                       std::size_t size) {
    const auto p = bytes_of(password);
    const auto s = bytes_of(salt);
    std::vector<std::uint8_t> out(size);
    pbkdf2_hmac_sha256(p.data(), p.size(), s.data(), s.size(), iterations, out.data(), out.size());
    return to_hex(out);
}

}  // namespace

// RFC 7914 section 11, the two PBKDF2-HMAC-SHA256 test vectors.
TEST_CASE("pbkdf2-hmac-sha256 rfc 7914 one iteration") {
    EXPECT_EQ(derive_hex("passwd", "salt", 1, 64),
              std::string{"55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc"
                          "49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783"});
}

TEST_CASE("pbkdf2-hmac-sha256 rfc 7914 80000 iterations") {
    EXPECT_EQ(derive_hex("Password", "NaCl", 80000, 64),
              std::string{"4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56"
                          "a1d425a1225833549adb841b51c9b3176a272bdebba1d078478f62b397f33c8d"});
}

// Further values computed independently with Python's hashlib.pbkdf2_hmac:
// a 32-byte output (the size hh uses) and a truncated partial block.
TEST_CASE("pbkdf2-hmac-sha256 single-block and truncated outputs") {
    EXPECT_EQ(derive_hex("password", "salt", 4096, 32),
              std::string{"c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a"});
    EXPECT_EQ(derive_hex("password", "salt", 2, 20),
              std::string{"ae4d0c95af6b46d32d0adff928f06dd02a303f8e"});
}

TEST_CASE("pbkdf2-hmac-sha256 treats zero iterations as one") {
    EXPECT_EQ(derive_hex("passwd", "salt", 0, 64), derive_hex("passwd", "salt", 1, 64));
}

TEST_CASE("pbkdf2-hmac-sha256 output prefix does not depend on the output length") {
    const std::string full = derive_hex("Password", "NaCl", 3, 96);
    EXPECT_EQ(derive_hex("Password", "NaCl", 3, 32), full.substr(0, 64));
    EXPECT_EQ(derive_hex("Password", "NaCl", 3, 33), full.substr(0, 66));
    EXPECT_EQ(derive_hex("Password", "NaCl", 3, 1), full.substr(0, 2));
}
