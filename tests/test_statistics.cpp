// Statistical properties: the features follow the specified
// distribution and the derivation diffuses every input bit. The stretch runs with
// one iteration here, which keeps the runs short and changes nothing about what is
// measured: the distribution comes from the feature table, the diffusion from
// SHA-256 and HMAC.

#include <array>
#include <cstdint>
#include <vector>

#include "derive.hpp"
#include "features.hpp"
#include "test_framework.hpp"
#include "test_util.hpp"

namespace {

std::array<std::uint8_t, 32> quick_fingerprint(const std::vector<std::uint8_t>& data) {
    const auto d0 = hh::detail::derive_d0(hh::detail::input_kind::binary, data.data(), data.size());
    return hh::detail::stretch(d0, 1);
}

// Chi-square statistic times 1000 for observed counts against expected shares in 1/8ths.
std::uint64_t chi_square_x1000(const std::vector<std::uint64_t>& observed,
                               const std::vector<std::uint64_t>& eighths, std::uint64_t total) {
    std::uint64_t sum = 0;
    for (std::size_t i = 0; i < observed.size(); ++i) {
        const std::uint64_t expected = total * eighths[i] / 8;
        const std::uint64_t diff =
            observed[i] > expected ? observed[i] - expected : expected - observed[i];
        sum += diff * diff * 1000 / expected;
    }
    return sum;
}

}  // namespace

TEST_CASE("statistics: figures and colours follow the specified distribution") {
    // 62 500 inputs give 10^6 cells.
    constexpr std::uint64_t inputs = 62500;
    std::vector<std::uint64_t> figures(7, 0);
    std::vector<std::uint64_t> colours(4, 0);
    std::uint64_t filled = 0;
    std::vector<std::uint8_t> data(8);
    for (std::uint64_t n = 0; n < inputs; ++n) {
        for (std::size_t i = 0; i < 8; ++i) {
            data[i] = static_cast<std::uint8_t>(n >> (8 * i));
        }
        const auto fp = quick_fingerprint(data);
        for (const hh::cell& c : hh::detail::cells_of(fp.data())) {
            ++figures[static_cast<std::size_t>(c.figure)];
            if (c.figure != hh::figure::none) {
                ++colours[c.colour];
                ++filled;
            }
        }
    }
    // none 2/8, every other figure 1/8: 6 degrees of freedom, the 99.9 % quantile is 22.46.
    EXPECT_TRUE(chi_square_x1000(figures, {2, 1, 1, 1, 1, 1, 1}, inputs * 16) < 22460);
    // Four colours at 2/8 each: 3 degrees of freedom, the 99.9 % quantile is 16.27.
    EXPECT_TRUE(chi_square_x1000(colours, {2, 2, 2, 2}, filled) < 16270);
}

TEST_CASE("statistics: one flipped input bit changes about half of the fingerprint bits") {
    hh_test::prng random(1999);
    std::uint64_t flipped = 0;
    std::uint64_t compared = 0;
    std::uint64_t cells_changed = 0;
    constexpr std::uint64_t rounds = 2000;
    for (std::uint64_t round = 0; round < rounds; ++round) {
        std::vector<std::uint8_t> data = random.bytes(20);
        const auto before = quick_fingerprint(data);
        const std::uint32_t bit = random.below(160);
        data[bit / 8] = static_cast<std::uint8_t>(data[bit / 8] ^ (1u << (bit % 8)));
        const auto after = quick_fingerprint(data);
        for (std::size_t i = 0; i < 16; ++i) {  // the cell bytes
            std::uint8_t x = static_cast<std::uint8_t>(before[i] ^ after[i]);
            for (; x != 0; x = static_cast<std::uint8_t>(x & (x - 1))) {
                ++flipped;
            }
            compared += 8;
        }
        const auto a = hh::detail::cells_of(before.data());
        const auto b = hh::detail::cells_of(after.data());
        for (std::size_t i = 0; i < 16; ++i) {
            cells_changed += (a[i].figure != b[i].figure || a[i].colour != b[i].colour) ? 1u : 0u;
        }
    }
    // 256 000 compared bits: the share of flipped bits stays within 49 .. 51 %.
    EXPECT_TRUE(flipped * 100 > compared * 49 && flipped * 100 < compared * 51);
    // Two random cells agree if both are empty, (1/4)^2, or show the same figure in the same
    // colour, 24 * (1/32)^2: 0.0859 together. They differ with probability 0.9141.
    EXPECT_TRUE(cells_changed * 1000 > rounds * 16 * 904 &&
                cells_changed * 1000 < rounds * 16 * 924);
}

TEST_CASE("statistics: the two modes of one input are unrelated") {
    hh_test::prng random(77);
    std::uint64_t equal_cells = 0;
    constexpr std::uint64_t rounds = 3000;
    const std::vector<std::uint8_t> key = random.bytes(32);
    for (std::uint64_t round = 0; round < rounds; ++round) {
        const auto s = quick_fingerprint(random.bytes(20));
        const auto keyed = hh::detail::keyed_bytes(key.data(), s.data());
        const auto a = hh::detail::cells_of(s.data());
        const auto b = hh::detail::cells_of(keyed.data());
        for (std::size_t i = 0; i < 16; ++i) {
            equal_cells += (a[i].figure == b[i].figure && a[i].colour == b[i].colour) ? 1u : 0u;
        }
    }
    // Two unrelated cells agree with probability 0.0859: between 7.8 and 9.4 % here.
    EXPECT_TRUE(equal_cells * 1000 > rounds * 16 * 78 && equal_cells * 1000 < rounds * 16 * 94);
}
