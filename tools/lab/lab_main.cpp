// hh_lab: the design lab. Research tooling only; it
// is built with -DHH_BUILD_LAB=ON and never installed.

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <thread>

#include "lab.hpp"
#include "sha256.hpp"

namespace hh {
namespace lab {

fingerprint_bytes sample_fingerprint(const std::string& label, std::uint64_t index) {
    const std::string text = label + "/" + std::to_string(index);
    const auto digest =
        hh::detail::sha256_hash(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
    fingerprint_bytes out{};
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = digest[i];
    }
    return out;
}

bool make_dirs(const std::string& dir) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return !ec;
}

unsigned thread_count(const options& opt) {
    if (opt.threads != 0) {
        return opt.threads;
    }
    const unsigned n = std::thread::hardware_concurrency();
    return n == 0 ? 1 : n;
}

}  // namespace lab
}  // namespace hh

namespace {

struct command {
    const char* name;
    int (*run)(const hh::lab::options&);
    const char* help;
};

const command commands[] = {
    {"selftest", hh::lab::run_selftest,
     "check the colour science and the prior-art ports against reference values"},
    {"palette", hh::lab::run_palette, "palette gate (Petroff 2021 method) and palette search"},
    {"sheets", hh::lab::run_contact_sheets,
     "contact sheets: 200 images, 4 sizes, 2 backgrounds, 5 vision conditions"},
    {"markers", hh::lab::run_markers, "keyed-mode marker candidates"},
    {"surfaces", hh::lab::run_surfaces, "host-chosen background colours, alpha and frame colours"},
    {"grind", hh::lab::run_grinding,
     "salience-ordered lookalike grinding, hh against Blockies and Jazzicon styles"},
    {"directions", hh::lab::run_directions, "pairs that differ only in triangle direction"},
    {"bench", hh::lab::run_stretch_bench, "stretching benchmark, C = 2^12 .. 2^16"},
};

void usage() {
    std::printf("usage: hh_lab <command> [--out DIR] [--threads N] [--quick]\ncommands:\n");
    for (const command& c : commands) {
        std::printf("  %-11s %s\n", c.name, c.help);
    }
    std::printf("  %-11s %s\n", "all", "every command above");
}

}  // namespace

int main(int argc, char** argv) {
    using namespace hh::lab;
    if (argc < 2) {
        usage();
        return 2;
    }
    options opt;
    const std::string name = argv[1];
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            opt.out_dir = argv[++i];
        } else if (std::strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            opt.threads = static_cast<unsigned>(std::stoul(argv[++i]));
        } else if (std::strcmp(argv[i], "--quick") == 0) {
            opt.quick = true;
        } else {
            usage();
            return 2;
        }
    }
    if (!make_dirs(opt.out_dir)) {
        std::fprintf(stderr, "cannot create %s\n", opt.out_dir.c_str());
        return 1;
    }
    int rc = 0;
    bool found = false;
    for (const command& c : commands) {
        if (name == "all" || name == c.name) {
            found = true;
            rc |= c.run(opt);
        }
    }
    if (!found) {
        usage();
        return 2;
    }
    return rc;
}
