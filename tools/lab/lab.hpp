#pragma once

// Shared declarations of the design lab commands.

#include <array>
#include <cstdint>
#include <string>

namespace hh {
namespace lab {

struct options {
    std::string out_dir = "lab-out";
    unsigned threads = 0;  // 0: hardware concurrency
    bool quick = false;    // smaller workloads for a smoke run
};

using fingerprint_bytes = std::array<std::uint8_t, 32>;

// SHA-256 of "<label>/<index>": the deterministic stand-in for fingerprints
// on the sheets. Stretching and keying do not change what an image looks like,
// only which address maps to it, so uniformly random bytes are what matters.
fingerprint_bytes sample_fingerprint(const std::string& label, std::uint64_t index);

// Creates `dir` and its parents; returns false on failure.
bool make_dirs(const std::string& dir);

unsigned thread_count(const options& opt);

int run_selftest(const options& opt);
int run_palette(const options& opt);
int run_contact_sheets(const options& opt);
int run_markers(const options& opt);
int run_surfaces(const options& opt);
int run_grinding(const options& opt);
int run_directions(const options& opt);
int run_stretch_bench(const options& opt);

}  // namespace lab
}  // namespace hh
