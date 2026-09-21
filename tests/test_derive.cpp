#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "derive.hpp"
#include "test_framework.hpp"
#include "test_util.hpp"

using hh::error_code;
using hh_test::bytes_of;
using hh_test::from_hex;
using hh_test::to_hex;
using hh_test::view;

namespace {

// Expected values were computed independently with Python's hashlib and hmac
// from the formulas of SPEC.md section 4.
const char* const address_hex = "5aAeb6053F3E94C9b9A09f33669435E7Ef1BeAed";
const char* const address_d0 = "ae4d3e2068c88576bd83d3401a7f3ce07cb8567ce8300261a3d8ebf7f097d85c";
const char* const address_s = "e212927148fcf76f6669c244a0db08bdd4f36dc50a378f6a1a3fe472807e7852";
const char* const address_keyed =
    "26ea8171aab23c8e1bf7c23417d33d6dba81d881af70edd2b0675348e080b478";
const char* const bech32_text = "bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4";

std::vector<std::uint8_t> test_key() {
    std::vector<std::uint8_t> key(32);
    for (std::size_t i = 0; i < key.size(); ++i) {
        key[i] = static_cast<std::uint8_t>(i);
    }
    return key;
}

}  // namespace

TEST_CASE("derive: M1 header layout") {
    const auto h = hh::detail::m1_header(hh::detail::input_kind::text, 0x01020304u);
    EXPECT_EQ(to_hex(h), std::string{"48756d616e697a656448617368"
                                     "00"
                                     "01"
                                     "01"
                                     "01020304"});
    const auto b = hh::detail::m1_header(hh::detail::input_kind::binary, 20);
    EXPECT_EQ(to_hex(b), std::string{"48756d616e697a656448617368"
                                     "00"
                                     "01"
                                     "00"
                                     "00000014"});
}

TEST_CASE("derive: d0, base digest, keyed fingerprint and kcv known answers") {
    const auto address = from_hex(address_hex);
    EXPECT_EQ(to_hex(hh::detail::derive_d0(hh::detail::input_kind::binary, address.data(),
                                           address.size())),
              std::string{address_d0});

    hh::base_digest digest;
    EXPECT_EQ(hh::make_base_digest(view(address), digest), error_code::ok);
    EXPECT_FALSE(digest.empty());
    EXPECT_EQ(to_hex(digest.bytes()), std::string{address_s});

    hh::fingerprint universal;
    EXPECT_EQ(hh::universal_fingerprint(digest, universal), error_code::ok);
    EXPECT_EQ(universal.mode(), hh::mode::universal);
    EXPECT_EQ(to_hex(universal.bytes()), std::string{address_s});

    hh::secret_key key;
    EXPECT_EQ(hh::make_secret_key(view(test_key()), key), error_code::ok);
    EXPECT_EQ(to_hex(key.kcv()), std::string{"6a5955cf"});

    hh::fingerprint keyed;
    EXPECT_EQ(hh::keyed_fingerprint(digest, key, keyed), error_code::ok);
    EXPECT_EQ(keyed.mode(), hh::mode::keyed);
    EXPECT_EQ(to_hex(keyed.bytes()), std::string{address_keyed});
    EXPECT_TRUE(universal != keyed);

    const auto m2 = hh::detail::m2_message(digest.bytes().data());
    EXPECT_EQ(to_hex(m2), std::string{"48756d616e697a656448617368"
                                      "00"
                                      "02"} +
                              address_s);
}

TEST_CASE("derive: hex forms of one address give one digest") {
    const char* forms[] = {
        "5aAeb6053F3E94C9b9A09f33669435E7Ef1BeAed",    // EIP-55
        "0x5aAeb6053F3E94C9b9A09f33669435E7Ef1BeAed",  // with prefix
        "0X5AAEB6053F3E94C9B9A09F33669435E7EF1BEAED",  // upper case
        "5aaeb6053f3e94c9b9a09f33669435e7ef1beaed",    // lower case
    };
    for (const char* form : forms) {
        hh::base_digest digest;
        EXPECT_EQ(hh::make_base_digest_from_hex(form, digest), error_code::ok);
        EXPECT_EQ(to_hex(digest.bytes()), std::string{address_s});
    }
}

TEST_CASE("derive: invalid hex is rejected") {
    const char* bad[] = {"",     "0x",    "0",     "abc",    "0xabc",           "zz",
                         "0x0g", " abcd", "ab ",   "ab\ncd", "0x 12",           "x012",
                         "00x1", "+1ab",  "12-34", "ab:cd",  "\xC3\xA9\xC3\xA9"};
    for (const char* text : bad) {
        hh::base_digest digest;
        EXPECT_EQ(hh::make_base_digest_from_hex(text, digest), error_code::invalid_hex);
        EXPECT_TRUE(digest.empty());
    }
    // An embedded zero character is not a hex digit.
    hh::base_digest digest;
    EXPECT_EQ(hh::make_base_digest_from_hex(std::string_view("ab\0cd", 5), digest),
              error_code::invalid_hex);
}

TEST_CASE("derive: the syntax of hexadecimal input is checked before its length") {
    hh::base_digest digest;
    std::string too_long(2 * (hh::detail::max_input_size + 1), 'a');
    EXPECT_EQ(hh::make_base_digest_from_hex(too_long, digest), error_code::input_too_large);
    too_long.back() = 'g';
    EXPECT_EQ(hh::make_base_digest_from_hex(too_long, digest), error_code::invalid_hex);
    too_long.pop_back();  // an odd number of digits
    EXPECT_EQ(hh::make_base_digest_from_hex(too_long, digest), error_code::invalid_hex);
}

TEST_CASE("derive: text and binary inputs are separated") {
    hh::base_digest text;
    hh::base_digest binary;
    EXPECT_EQ(hh::make_base_digest_from_text(bech32_text, text), error_code::ok);
    EXPECT_EQ(hh::make_base_digest(view(bytes_of(bech32_text)), binary), error_code::ok);
    EXPECT_EQ(to_hex(text.bytes()),
              std::string{"dc705192e4a205d8c403ae7693290df45f09cec04116ad38f6140f349392548f"});
    EXPECT_EQ(to_hex(binary.bytes()),
              std::string{"3f35c44a4304080d9c3d013b231e7c56c3cbe57cd3abe2ad308c86571dae5d44"});
}

TEST_CASE("derive: embedded zero bytes are data") {
    const std::vector<std::uint8_t> zeros(20, 0);
    hh::base_digest digest;
    EXPECT_EQ(hh::make_base_digest(view(zeros), digest), error_code::ok);
    EXPECT_EQ(to_hex(digest.bytes()),
              std::string{"f04a24e8db9c3d750e54d1c0f0224e0da795606a1e0a4af97b20b2015307f94b"});
    hh::base_digest shorter;
    const std::vector<std::uint8_t> zeros19(19, 0);
    EXPECT_EQ(hh::make_base_digest(view(zeros19), shorter), error_code::ok);
    EXPECT_TRUE(digest != shorter);
}

TEST_CASE("derive: input length limits") {
    hh::base_digest digest;
    EXPECT_EQ(hh::make_base_digest({nullptr, 0}, digest), error_code::empty_input);
    EXPECT_EQ(hh::make_base_digest_from_text("", digest), error_code::empty_input);
    EXPECT_EQ(hh::make_base_digest({nullptr, 4}, digest), error_code::invalid_argument);

    const std::vector<std::uint8_t> big(hh::detail::max_input_size + 1, 0xAB);
    EXPECT_EQ(hh::make_base_digest({big.data(), big.size()}, digest), error_code::input_too_large);
    EXPECT_TRUE(digest.empty());
    EXPECT_EQ(hh::make_base_digest({big.data(), big.size() - 1}, digest), error_code::ok);

    // The hex form decodes in chunks; it must agree with the binary form at any length.
    const std::string big_hex(2 * hh::detail::max_input_size, 'A');
    hh::base_digest from_hex_form;
    EXPECT_EQ(hh::make_base_digest_from_hex(big_hex, from_hex_form), error_code::ok);
    const std::vector<std::uint8_t> same(hh::detail::max_input_size, 0xAA);
    hh::base_digest from_bytes;
    EXPECT_EQ(hh::make_base_digest(view(same), from_bytes), error_code::ok);
    EXPECT_TRUE(from_hex_form == from_bytes);
    EXPECT_EQ(hh::make_base_digest_from_hex(big_hex + "AA", digest), error_code::input_too_large);
}

TEST_CASE("derive: hex and binary agree around the chunk and block boundaries") {
    for (std::size_t length : {1u, 35u, 36u, 43u, 44u, 45u, 255u, 256u, 257u, 511u, 512u, 513u}) {
        std::vector<std::uint8_t> data(length);
        for (std::size_t i = 0; i < length; ++i) {
            data[i] = static_cast<std::uint8_t>(i * 7 + length);
        }
        hh::base_digest a;
        hh::base_digest b;
        EXPECT_EQ(hh::make_base_digest(view(data), a), error_code::ok);
        EXPECT_EQ(hh::make_base_digest_from_hex(to_hex(data), b), error_code::ok);
        EXPECT_TRUE(a == b);
    }
}

TEST_CASE("derive: keys must be 32 bytes and not all zero") {
    hh::secret_key key;
    EXPECT_TRUE(key.empty());
    EXPECT_EQ(to_hex(key.kcv()), std::string{"00000000"});
    for (std::size_t length : {0u, 1u, 31u, 33u, 64u}) {
        const std::vector<std::uint8_t> bytes(length, 0x11);
        EXPECT_EQ(hh::make_secret_key(view(bytes), key), error_code::invalid_key);
        EXPECT_TRUE(key.empty());
    }
    EXPECT_EQ(hh::make_secret_key(view(std::vector<std::uint8_t>(32, 0)), key),
              error_code::invalid_key);
    EXPECT_EQ(hh::make_secret_key({nullptr, 32}, key), error_code::invalid_key);
    std::vector<std::uint8_t> almost(32, 0);
    almost[31] = 1;
    EXPECT_EQ(hh::make_secret_key(view(almost), key), error_code::ok);
    EXPECT_FALSE(key.empty());
}

TEST_CASE("derive: moving a key empties the source") {
    hh::secret_key key;
    EXPECT_EQ(hh::make_secret_key(view(test_key()), key), error_code::ok);
    const auto kcv = key.kcv();
    hh::secret_key copy = key;
    hh::secret_key moved = std::move(key);
    EXPECT_TRUE(key.empty());  // NOLINT: the moved-from state is specified
    EXPECT_EQ(to_hex(moved.kcv()), to_hex(kcv));
    EXPECT_EQ(to_hex(copy.kcv()), to_hex(kcv));
    hh::secret_key assigned;
    assigned = std::move(moved);
    EXPECT_TRUE(moved.empty());  // NOLINT
    EXPECT_EQ(to_hex(assigned.kcv()), to_hex(kcv));
}

TEST_CASE("derive: unset values are reported, never used") {
    hh::base_digest no_digest;
    hh::secret_key no_key;
    hh::fingerprint fp;
    EXPECT_EQ(hh::universal_fingerprint(no_digest, fp), error_code::invalid_digest);
    EXPECT_TRUE(fp.empty());

    hh::base_digest digest;
    EXPECT_EQ(hh::import_base_digest(view(from_hex(address_s)), digest), error_code::ok);
    EXPECT_EQ(hh::keyed_fingerprint(no_digest, no_key, fp), error_code::invalid_digest);
    EXPECT_EQ(hh::keyed_fingerprint(digest, no_key, fp), error_code::invalid_key);
    EXPECT_TRUE(fp.empty());

    EXPECT_EQ(hh::import_base_digest(view(std::vector<std::uint8_t>(31, 1)), digest),
              error_code::invalid_digest);
    EXPECT_TRUE(digest.empty());
}

TEST_CASE("derive: imported values behave like derived ones") {
    hh::base_digest digest;
    EXPECT_EQ(hh::import_base_digest(view(from_hex(address_s)), digest), error_code::ok);
    hh::secret_key key;
    EXPECT_EQ(hh::make_secret_key(view(test_key()), key), error_code::ok);
    hh::fingerprint keyed;
    EXPECT_EQ(hh::keyed_fingerprint(digest, key, keyed), error_code::ok);

    hh::fingerprint imported;
    EXPECT_EQ(hh::import_fingerprint(view(from_hex(address_keyed)), hh::mode::keyed, imported),
              error_code::ok);
    EXPECT_TRUE(imported == keyed);
    EXPECT_EQ(std::hash<hh::fingerprint>{}(imported), std::hash<hh::fingerprint>{}(keyed));

    hh::fingerprint as_universal;
    EXPECT_EQ(
        hh::import_fingerprint(view(from_hex(address_keyed)), hh::mode::universal, as_universal),
        error_code::ok);
    EXPECT_TRUE(as_universal != keyed);

    EXPECT_EQ(
        hh::import_fingerprint(view(std::vector<std::uint8_t>(33, 1)), hh::mode::keyed, imported),
        error_code::invalid_fingerprint);
    EXPECT_EQ(
        hh::import_fingerprint(view(from_hex(address_keyed)), static_cast<hh::mode>(3), imported),
        error_code::invalid_fingerprint);
    EXPECT_TRUE(imported.empty());

    // A value may be imported from its own bytes, for example to relabel its mode.
    hh::fingerprint relabelled = keyed;
    EXPECT_EQ(hh::import_fingerprint({relabelled.bytes().data(), relabelled.bytes().size()},
                                     hh::mode::universal, relabelled),
              error_code::ok);
    EXPECT_TRUE(relabelled == as_universal);
    hh::base_digest same = digest;
    EXPECT_EQ(hh::import_base_digest({same.bytes().data(), same.bytes().size()}, same),
              error_code::ok);
    EXPECT_TRUE(same == digest);
}

TEST_CASE("derive: different keys give unrelated fingerprints and check values") {
    hh::base_digest digest;
    EXPECT_EQ(hh::import_base_digest(view(from_hex(address_s)), digest), error_code::ok);
    auto bytes = test_key();
    hh::secret_key a;
    EXPECT_EQ(hh::make_secret_key(view(bytes), a), error_code::ok);
    bytes[0] ^= 1;
    hh::secret_key b;
    EXPECT_EQ(hh::make_secret_key(view(bytes), b), error_code::ok);
    hh::fingerprint fa;
    hh::fingerprint fb;
    EXPECT_EQ(hh::keyed_fingerprint(digest, a, fa), error_code::ok);
    EXPECT_EQ(hh::keyed_fingerprint(digest, b, fb), error_code::ok);
    EXPECT_TRUE(fa != fb);
    EXPECT_NE(to_hex(a.kcv()), to_hex(b.kcv()));
}
