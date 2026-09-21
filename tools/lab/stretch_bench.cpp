// Stretching benchmark: the cost of the base
// digest of SPEC.md section 4 for C = 2^12 .. 2^16 on one core. The algorithm
// uses C = 2^14; the other counts show what the choice costs. Budget: 10 ms per
// address on the desktop.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "derive.hpp"
#include "lab.hpp"

#ifndef HH_LAB_BUILD_FLAGS
#define HH_LAB_BUILD_FLAGS "unknown"
#endif

namespace hh {
namespace lab {

namespace {

std::array<std::uint8_t, 32> base_digest(const std::uint8_t* data, std::size_t size,
                                         std::uint32_t iterations) {
    const auto d0 = hh::detail::derive_d0(hh::detail::input_kind::binary, data, size);
    return hh::detail::stretch(d0, iterations);
}

std::string hex(const std::array<std::uint8_t, 32>& b) {
    static const char digits[] = "0123456789abcdef";
    std::string out;
    for (std::uint8_t v : b) {
        out.push_back(digits[v >> 4]);
        out.push_back(digits[v & 15]);
    }
    return out;
}

}  // namespace

int run_stretch_bench(const options& opt) {
    const std::string dir = opt.out_dir + "/bench";
    make_dirs(dir);
    const int addresses = opt.quick ? 20 : 200;

    // The same check value is printed by the hh-kotlin benchmark.
    const std::uint8_t zeros[20] = {};
    const std::string check = hex(base_digest(zeros, sizeof(zeros), 1u << 14));

    std::FILE* f = std::fopen((dir + "/stretch_cpp.tsv").c_str(), "w");
    if (f == nullptr) {
        return 1;
    }
    std::fprintf(f, "# C++ stretching benchmark, one thread; compiler %s; flags %s\n", __VERSION__,
                 HH_LAB_BUILD_FLAGS);
    std::fprintf(f, "# check value, 20 zero bytes, C = 2^14: %s\n", check.c_str());
    std::fprintf(f,
                 "log2_c\tc\taddresses\tmean_ms\tmedian_ms\tmin_ms\tmax_ms\tns_per_"
                 "compression\tbudget_10ms\n");
    std::printf("check value (20 zero bytes, C = 2^14): %s\n", check.c_str());

    for (int log2c = 12; log2c <= 16; ++log2c) {
        const std::uint32_t c = 1u << log2c;
        std::vector<double> ms;
        std::uint8_t sink = 0;
        for (int i = -5; i < addresses; ++i) {  // five warm-up runs
            const auto address =
                sample_fingerprint("hh-lab/bench", static_cast<std::uint64_t>(i + 5));
            const auto t0 = std::chrono::steady_clock::now();
            const auto s = base_digest(address.data(), 20, c);
            const auto t1 = std::chrono::steady_clock::now();
            sink = static_cast<std::uint8_t>(sink ^ s[0]);
            if (i >= 0) {
                ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
            }
        }
        std::sort(ms.begin(), ms.end());
        double mean = 0;
        for (double v : ms) {
            mean += v;
        }
        mean /= static_cast<double>(ms.size());
        const double median = ms[ms.size() / 2];
        // Two compressions per iteration, plus about six for M1, the key setup and U_1.
        const double compressions = 2.0 * c + 6.0;
        std::fprintf(f, "%d\t%u\t%d\t%.3f\t%.3f\t%.3f\t%.3f\t%.1f\t%s\n", log2c, c, addresses, mean,
                     median, ms.front(), ms.back(), median * 1e6 / compressions,
                     median <= 10.0 ? "within" : "over");
        std::printf("C = 2^%d: median %.3f ms, mean %.3f ms, %.1f ns per compression (sink %u)\n",
                    log2c, median, mean, median * 1e6 / compressions, sink);
    }
    std::fclose(f);
    std::printf("wrote %s\n", (dir + "/stretch_cpp.tsv").c_str());
    return 0;
}

}  // namespace lab
}  // namespace hh
