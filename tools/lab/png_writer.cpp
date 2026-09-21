// PNG writer for the lab sheets: CRC-32, Adler-32 and a zlib stream with one
// fixed-Huffman deflate block and hash-chain LZ77 matching. Written here
// because the project uses no third-party code; the library's own encoder
// is specified separately and is byte-exact across languages.

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "canvas.hpp"

namespace hh {
namespace lab {

namespace {

const std::array<std::uint32_t, 256>& crc_table() {
    static const std::array<std::uint32_t, 256> table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t n = 0; n < 256; ++n) {
            std::uint32_t c = n;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            }
            t[n] = c;
        }
        return t;
    }();
    return table;
}

std::uint32_t crc32(const std::uint8_t* data, std::size_t size, std::uint32_t crc = 0) {
    crc = ~crc;
    for (std::size_t i = 0; i < size; ++i) {
        crc = crc_table()[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    }
    return ~crc;
}

std::uint32_t adler32(const std::vector<std::uint8_t>& data) {
    std::uint32_t a = 1;
    std::uint32_t b = 0;
    for (std::uint8_t v : data) {
        a = (a + v) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

class bit_writer {
public:
    void put(std::uint32_t bits, int count) {
        acc_ |= static_cast<std::uint64_t>(bits) << used_;
        used_ += count;
        while (used_ >= 8) {
            out_.push_back(static_cast<std::uint8_t>(acc_));
            acc_ >>= 8;
            used_ -= 8;
        }
    }

    // Huffman codes are defined most significant bit first.
    void put_code(std::uint32_t code, int count) {
        std::uint32_t reversed = 0;
        for (int i = 0; i < count; ++i) {
            reversed = (reversed << 1) | ((code >> i) & 1u);
        }
        put(reversed, count);
    }

    std::vector<std::uint8_t> finish() {
        if (used_ > 0) {
            out_.push_back(static_cast<std::uint8_t>(acc_));
        }
        acc_ = 0;
        used_ = 0;
        return std::move(out_);
    }

private:
    std::vector<std::uint8_t> out_;
    std::uint64_t acc_ = 0;
    int used_ = 0;
};

void put_literal(bit_writer& w, unsigned symbol) {
    if (symbol < 144) {
        w.put_code(0x30u + symbol, 8);
    } else if (symbol < 256) {
        w.put_code(0x190u + (symbol - 144u), 9);
    } else if (symbol < 280) {
        w.put_code(symbol - 256u, 7);
    } else {
        w.put_code(0xC0u + (symbol - 280u), 8);
    }
}

constexpr unsigned length_base[29] = {3,  4,  5,  6,   7,   8,   9,   10,  11, 13,
                                      15, 17, 19, 23,  27,  31,  35,  43,  51, 59,
                                      67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr int length_extra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                  2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr unsigned dist_base[30] = {1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
                                    33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
                                    1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr int dist_extra[30] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

void put_match(bit_writer& w, unsigned length, unsigned dist) {
    int li = 28;
    while (length_base[li] > length) {
        --li;
    }
    put_literal(w, 257u + static_cast<unsigned>(li));
    w.put(length - length_base[li], length_extra[li]);
    int di = 29;
    while (dist_base[di] > dist) {
        --di;
    }
    w.put_code(static_cast<std::uint32_t>(di), 5);
    w.put(dist - dist_base[di], dist_extra[di]);
}

std::vector<std::uint8_t> deflate_fixed(const std::vector<std::uint8_t>& in) {
    constexpr std::size_t window = 32768;
    constexpr std::size_t hash_size = 1u << 16;
    constexpr int max_chain = 48;
    constexpr std::size_t max_match = 258;

    std::vector<std::int64_t> head(hash_size, -1);
    std::vector<std::int64_t> prev(window, -1);
    auto hash_at = [&](std::size_t i) {
        const std::uint32_t v =
            (std::uint32_t(in[i]) << 16) | (std::uint32_t(in[i + 1]) << 8) | in[i + 2];
        return (v * 2654435761u) >> 16;
    };
    auto insert = [&](std::size_t i) {
        if (i + 2 < in.size()) {
            const std::uint32_t h = hash_at(i);
            prev[i % window] = head[h];
            head[h] = static_cast<std::int64_t>(i);
        }
    };

    bit_writer w;
    w.put(1, 1);  // BFINAL
    w.put(1, 2);  // BTYPE = 01, fixed Huffman codes
    std::size_t i = 0;
    while (i < in.size()) {
        std::size_t best_len = 0;
        std::size_t best_dist = 0;
        if (i + 2 < in.size()) {
            std::int64_t cand = head[hash_at(i)];
            int chain = 0;
            const std::size_t limit = std::min(max_match, in.size() - i);
            while (cand >= 0 && chain++ < max_chain) {
                const std::size_t c = static_cast<std::size_t>(cand);
                if (i - c > window - 1) {
                    break;
                }
                std::size_t len = 0;
                while (len < limit && in[c + len] == in[i + len]) {
                    ++len;
                }
                if (len > best_len) {
                    best_len = len;
                    best_dist = i - c;
                    if (len == limit) {
                        break;
                    }
                }
                cand = prev[c % window];
            }
        }
        if (best_len >= 3) {
            put_match(w, static_cast<unsigned>(best_len), static_cast<unsigned>(best_dist));
            for (std::size_t k = 0; k < best_len; ++k) {
                insert(i + k);
            }
            i += best_len;
        } else {
            put_literal(w, in[i]);
            insert(i);
            ++i;
        }
    }
    put_literal(w, 256);
    return w.finish();
}

void put_be32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v >> 24));
    out.push_back(static_cast<std::uint8_t>(v >> 16));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
    out.push_back(static_cast<std::uint8_t>(v));
}

void put_chunk(std::vector<std::uint8_t>& out, const char* type,
               const std::vector<std::uint8_t>& data) {
    put_be32(out, static_cast<std::uint32_t>(data.size()));
    const std::size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    put_be32(out, crc32(out.data() + start, out.size() - start));
}

}  // namespace

bool write_png(const std::string& path, const canvas& c, bool with_alpha) {
    const int channels = with_alpha ? 4 : 3;
    std::vector<std::uint8_t> raw;
    raw.reserve(static_cast<std::size_t>(c.height()) *
                (static_cast<std::size_t>(c.width()) * static_cast<std::size_t>(channels) + 1));
    for (int y = 0; y < c.height(); ++y) {
        raw.push_back(0);  // filter type None
        for (int x = 0; x < c.width(); ++x) {
            const rgba8 p = c.get(x, y);
            raw.push_back(p.r);
            raw.push_back(p.g);
            raw.push_back(p.b);
            if (with_alpha) {
                raw.push_back(p.a);
            }
        }
    }

    std::vector<std::uint8_t> zlib = {0x78, 0x01};
    const std::vector<std::uint8_t> body = deflate_fixed(raw);
    zlib.insert(zlib.end(), body.begin(), body.end());
    put_be32(zlib, adler32(raw));

    std::vector<std::uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<std::uint8_t> ihdr;
    put_be32(ihdr, static_cast<std::uint32_t>(c.width()));
    put_be32(ihdr, static_cast<std::uint32_t>(c.height()));
    ihdr.push_back(8);
    ihdr.push_back(with_alpha ? 6 : 2);
    ihdr.push_back(0);
    ihdr.push_back(0);
    ihdr.push_back(0);
    put_chunk(png, "IHDR", ihdr);
    put_chunk(png, "sRGB", {0});
    put_chunk(png, "IDAT", zlib);
    put_chunk(png, "IEND", {});

    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) {
        return false;
    }
    const bool ok = std::fwrite(png.data(), 1, png.size(), f) == png.size();
    return std::fclose(f) == 0 && ok;
}

}  // namespace lab
}  // namespace hh
