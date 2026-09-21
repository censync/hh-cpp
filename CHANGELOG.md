# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html). The algorithm itself is frozen and
has no version: no release changes a fingerprint, a pixel or an encoded byte.

## [1.0.0] - 2026-09-21

The first release, and the freeze of the algorithm.

### Added

- `docs/SPEC.md`: the normative specification, with golden vectors in `testdata/vectors.tsv` and
  reference renders in `testdata/golden/`.
- Public C++17 API under `hh::` (`include/hh/*.hpp`): base digest from bytes, hexadecimal or
  text; 32-byte secret key with a key check value; universal and keyed fingerprints; layout and
  six-character tag; renderer with square and round shapes, keyed-mode frame markers, any
  background colour and transparency, frame transparency and a WCAG contrast rule; PNG, BMP and
  JPEG encoders with byte-exact output.
- C ABI in `include/hh/hh.h` for bindings: plain C99, caller-allocated buffers, a sized options
  structure that can grow.
- Internal SHA-256 (FIPS 180-4), HMAC-SHA-256 (RFC 2104), PBKDF2-HMAC-SHA-256 (RFC 8018),
  CRC-32, Adler-32, fixed-Huffman deflate and a baseline JPEG encoder; no dependencies.
- Examples `hh_cli` and `hh_compare`; vector generator `tools/gen_vectors` with a check mode.
- Tests: known-answer tests of the primitives (FIPS 180-4, RFC 4231, RFC 7914), the golden
  vectors, encoder round trips through an independent inflater, the C ABI from a C translation
  unit, deterministic fuzz loops, statistical tests of the feature distribution and of the
  diffusion; all clean under AddressSanitizer and UndefinedBehaviorSanitizer.
- CMake 3.16 build with install rules, `hh::hh` package config, a relocatable pkg-config file,
  presets, consumer smoke tests (`find_package`, `pkg-config` from C++ and from C,
  `add_subdirectory`, static and shared). Verified on x86-64 (GCC 11, Clang 14) and on Android
  arm64-v8a and armeabi-v7a (NDK Clang).
- `docs/SECURITY.md`, `docs/INTEGRATION.md`, and the design record in `docs/design/`.
- Design lab `tools/lab` (`-DHH_BUILD_LAB=ON`, never installed): palette gate, contact sheets,
  frame markers, host surfaces, lookalike grinding, direction sheets, stretching benchmark.

[1.0.0]: https://github.com/censync/hh-cpp/releases/tag/v1.0.0
