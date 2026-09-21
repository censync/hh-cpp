#pragma once

// Helpers shared by the test files: hex conversion and byte strings.

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <hh/hh.hpp>

namespace hh_test {

inline std::string to_hex(const std::uint8_t* data, std::size_t size) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        out.push_back(digits[data[i] >> 4]);
        out.push_back(digits[data[i] & 0x0F]);
    }
    return out;
}

template <typename Container>
std::string to_hex(const Container& bytes) {
    return to_hex(bytes.data(), bytes.size());
}

inline std::vector<std::uint8_t> from_hex(std::string_view hex) {
    auto nibble = [](char c) -> std::uint8_t {
        if (c >= '0' && c <= '9') {
            return std::uint8_t(c - '0');
        }
        if (c >= 'a' && c <= 'f') {
            return std::uint8_t(c - 'a' + 10);
        }
        return std::uint8_t(c - 'A' + 10);
    };
    std::vector<std::uint8_t> out;
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        out.push_back(std::uint8_t((nibble(hex[i]) << 4) | nibble(hex[i + 1])));
    }
    return out;
}

inline std::vector<std::uint8_t> bytes_of(std::string_view text) {
    return std::vector<std::uint8_t>(text.begin(), text.end());
}

inline hh::byte_view view(const std::vector<std::uint8_t>& bytes) {
    return {bytes.data(), bytes.size()};
}

// The fingerprint of 32 given bytes.
inline hh::fingerprint fingerprint_of(const std::vector<std::uint8_t>& bytes, hh::mode m) {
    hh::fingerprint fp;
    hh::import_fingerprint(view(bytes), m, fp);
    return fp;
}

// A small deterministic generator for the fuzz and property tests (xorshift64*).
class prng {
public:
    explicit prng(std::uint64_t seed) : state_(seed == 0 ? 0x9E3779B97F4A7C15ull : seed) {}

    std::uint64_t next() {
        state_ ^= state_ >> 12;
        state_ ^= state_ << 25;
        state_ ^= state_ >> 27;
        return state_ * 0x2545F4914F6CDD1Dull;
    }

    std::uint32_t below(std::uint32_t bound) { return static_cast<std::uint32_t>(next() % bound); }

    std::vector<std::uint8_t> bytes(std::size_t count) {
        std::vector<std::uint8_t> out(count);
        for (auto& b : out) {
            b = static_cast<std::uint8_t>(next() >> 56);
        }
        return out;
    }

private:
    std::uint64_t state_;
};

}  // namespace hh_test

namespace hh {

inline std::ostream& operator<<(std::ostream& os, error_code ec) {
    return os << error_name(ec);
}

inline std::ostream& operator<<(std::ostream& os, mode m) {
    return os << (m == mode::keyed ? "keyed" : "universal");
}

inline std::ostream& operator<<(std::ostream& os, figure f) {
    return os << static_cast<int>(f);
}

inline std::ostream& operator<<(std::ostream& os, frame_style s) {
    return os << static_cast<int>(s);
}

}  // namespace hh
