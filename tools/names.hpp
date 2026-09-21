#pragma once

// The names the specification and the golden vectors use for options, shared by
// the command line example, the vector generator and the tests.

#include <cstdint>
#include <string>
#include <vector>

#include <hh/hh.hpp>

namespace hh_tools {

inline const char* shape_name(hh::image_shape s) {
    return s == hh::image_shape::round ? "round" : "square";
}

inline bool parse_shape(const std::string& name, hh::image_shape& out) {
    if (name == "square") {
        out = hh::image_shape::square;
        return true;
    }
    if (name == "round") {
        out = hh::image_shape::round;
        return true;
    }
    return false;
}

inline const char* const frame_names[] = {"automatic", "none",  "plain",    "rounded", "chamfered",
                                          "double",    "thick", "brackets", "ticks",   "gaps"};

inline const char* frame_name(hh::frame_style s) {
    return frame_names[static_cast<unsigned>(s)];
}

inline bool parse_frame(const std::string& name, hh::frame_style& out) {
    for (unsigned i = 0; i < 10; ++i) {
        if (name == frame_names[i]) {
            out = static_cast<hh::frame_style>(i);
            return true;
        }
    }
    return false;
}

inline const char* mode_name(hh::mode m) {
    return m == hh::mode::keyed ? "keyed" : "universal";
}

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
std::string to_hex(const Container& c) {
    return to_hex(c.data(), c.size());
}

// Strict hex: an even number of digits of either case. Returns false otherwise.
inline bool from_hex(const std::string& hex, std::vector<std::uint8_t>& out) {
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }
        return -1;
    };
    out.clear();
    if (hex.size() % 2 != 0) {
        return false;
    }
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        const int hi = nibble(hex[i]);
        const int lo = nibble(hex[i + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        out.push_back(static_cast<std::uint8_t>(hi * 16 + lo));
    }
    return true;
}

// "RRGGBB" into a colour, "RRGGBBAA" into a colour and an alpha.
inline bool parse_rgb(const std::string& text, hh::rgb& out) {
    std::vector<std::uint8_t> b;
    if (!from_hex(text, b) || b.size() != 3) {
        return false;
    }
    out = {b[0], b[1], b[2]};
    return true;
}

inline bool parse_rgba(const std::string& text, hh::rgb& out, std::uint8_t& alpha) {
    std::vector<std::uint8_t> b;
    if (!from_hex(text, b) || b.size() != 4) {
        return false;
    }
    out = {b[0], b[1], b[2]};
    alpha = b[3];
    return true;
}

inline std::string rgba_text(hh::rgb c, std::uint8_t alpha) {
    const std::uint8_t b[4] = {c.r, c.g, c.b, alpha};
    return to_hex(b, 4);
}

inline std::string rgb_text(hh::rgb c) {
    const std::uint8_t b[3] = {c.r, c.g, c.b};
    return to_hex(b, 3);
}

// The test image patterns of SPEC.md section 15: "flat:<RRGGBBAA>", "noise:<seed>" and
// "opaque:<seed>". Returns false for anything else.
inline bool pattern_image(const std::string& pattern, std::uint32_t width, std::uint32_t height,
                          hh::image& out) {
    out.width = width;
    out.height = height;
    out.rgba.assign(static_cast<std::size_t>(width) * height * 4, 0);
    const std::size_t colon = pattern.find(':');
    if (colon == std::string::npos) {
        return false;
    }
    const std::string kind = pattern.substr(0, colon);
    const std::string argument = pattern.substr(colon + 1);
    if (kind == "flat") {
        std::vector<std::uint8_t> value;
        if (!from_hex(argument, value) || value.size() != 4) {
            return false;
        }
        for (std::size_t i = 0; i < out.rgba.size(); ++i) {
            out.rgba[i] = value[i % 4];
        }
        return true;
    }
    if (kind != "noise" && kind != "opaque") {
        return false;
    }
    std::uint32_t x = static_cast<std::uint32_t>(std::stoul(argument));
    for (std::size_t i = 0; i < out.rgba.size(); ++i) {
        // Arithmetic modulo 2^32 and a mask give the value modulo 2^31.
        x = (x * 1103515245u + 12345u) & 0x7FFFFFFFu;
        out.rgba[i] = static_cast<std::uint8_t>((x >> 16) & 0xFFu);
    }
    if (kind == "opaque") {
        for (std::size_t i = 3; i < out.rgba.size(); i += 4) {
            out.rgba[i] = 0xFF;
        }
    }
    return true;
}

}  // namespace hh_tools
