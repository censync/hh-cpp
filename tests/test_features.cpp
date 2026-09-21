#include <cstdint>
#include <string>
#include <vector>

#include "test_framework.hpp"
#include "test_util.hpp"

using hh_test::fingerprint_of;
using hh_test::from_hex;

TEST_CASE("features: figure codes and colour indices") {
    // Byte i has the figure code i / 2 (so every code appears twice) and the colour i mod 4.
    std::vector<std::uint8_t> bytes(32, 0);
    for (unsigned i = 0; i < 16; ++i) {
        bytes[i] = static_cast<std::uint8_t>(((i / 2) << 5) | ((i % 4) << 3) | 0x07u);
    }
    const hh::layout l = hh::describe(fingerprint_of(bytes, hh::mode::universal));
    const hh::figure expected[8] = {hh::figure::none,          hh::figure::none,
                                    hh::figure::square,        hh::figure::circle,
                                    hh::figure::triangle_up,   hh::figure::triangle_right,
                                    hh::figure::triangle_down, hh::figure::triangle_left};
    for (unsigned i = 0; i < 16; ++i) {
        EXPECT_EQ(l.cells[i].figure, expected[i / 2]);
        const unsigned colour = expected[i / 2] == hh::figure::none ? 0u : i % 4;
        EXPECT_EQ(static_cast<unsigned>(l.cells[i].colour), colour);
    }
    EXPECT_EQ(l.mode, hh::mode::universal);
}

TEST_CASE("features: reserved bits and unused bytes do not change the layout") {
    std::vector<std::uint8_t> a(32, 0x90);
    std::vector<std::uint8_t> b(32, 0x97);
    for (std::size_t i = 16; i < 32; ++i) {
        b[i] = static_cast<std::uint8_t>(i);
    }
    const hh::layout la = hh::describe(fingerprint_of(a, hh::mode::keyed));
    const hh::layout lb = hh::describe(fingerprint_of(b, hh::mode::keyed));
    for (std::size_t i = 0; i < 16; ++i) {
        EXPECT_EQ(la.cells[i].figure, lb.cells[i].figure);
        EXPECT_EQ(static_cast<int>(la.cells[i].colour), static_cast<int>(lb.cells[i].colour));
    }
    EXPECT_EQ(la.mode, hh::mode::keyed);
}

TEST_CASE("features: palette and frame colour") {
    const hh::layout l =
        hh::describe(fingerprint_of(std::vector<std::uint8_t>(32, 0), hh::mode::universal));
    EXPECT_EQ(l.palette_rgb[0], 0x7A96C5u);
    EXPECT_EQ(l.palette_rgb[1], 0x890AF0u);
    EXPECT_EQ(l.palette_rgb[2], 0xC10445u);
    EXPECT_EQ(l.palette_rgb[3], 0xD48200u);
    EXPECT_EQ(l.frame_rgb, 0x808080u);
}

// Tags computed independently with Python from SPEC.md section 5.3.
TEST_CASE("features: tag known answers") {
    const auto universal =
        fingerprint_of(from_hex("e212927148fcf76f6669c244a0db08bdd4f36dc50a378f6a1a3fe472807e7852"),
                       hh::mode::universal);
    EXPECT_EQ(universal.tag(), std::string{"TKSPVH"});
    const auto keyed =
        fingerprint_of(from_hex("26ea8171aab23c8e1bf7c23417d33d6dba81d881af70edd2b0675348e080b478"),
                       hh::mode::keyed);
    EXPECT_EQ(keyed.tag(), std::string{"QA0XH0"});

    std::vector<std::uint8_t> bytes(32, 0);
    EXPECT_EQ(fingerprint_of(bytes, hh::mode::universal).tag(), std::string{"000000"});
    bytes[16] = bytes[17] = bytes[18] = bytes[19] = 0xFF;
    EXPECT_EQ(fingerprint_of(bytes, hh::mode::universal).tag(), std::string{"ZZZZZZ"});
    // The two lowest bits of the word are not part of the tag.
    bytes[19] = 0xFC;
    EXPECT_EQ(fingerprint_of(bytes, hh::mode::universal).tag(), std::string{"ZZZZZZ"});
    // 0x08421084 >> 2 = 0b00001_00001_00001_00001_00001_00001.
    bytes[16] = 0x08;
    bytes[17] = 0x42;
    bytes[18] = 0x10;
    bytes[19] = 0x84;
    EXPECT_EQ(fingerprint_of(bytes, hh::mode::universal).tag(), std::string{"111111"});
}

TEST_CASE("features: an unset fingerprint has no cells") {
    const hh::fingerprint unset;
    EXPECT_TRUE(unset.empty());
    const hh::layout l = hh::describe(unset);
    for (const hh::cell& c : l.cells) {
        EXPECT_EQ(c.figure, hh::figure::none);
    }
    EXPECT_EQ(unset.tag(), std::string{"000000"});
}
