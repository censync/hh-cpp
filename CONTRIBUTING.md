# Contributing to hh-cpp

hh-cpp is the C++17 reference implementation of Humanized Hash (`hh`). It owns `docs/SPEC.md`,
`docs/SECURITY.md` and the canonical golden vectors in `testdata/`. The other implementations
(hh-kotlin, hh-ts, go-hh, hh-python) copy the vectors byte for byte and record the hh-cpp tag they
came from.

Bug reports and patches are welcome: open an issue or a pull request. Security problems are
reported privately, as `docs/SECURITY.md` describes.

## The algorithm is frozen

`docs/SPEC.md` is normative and `testdata/vectors.tsv` holds the golden vectors. The algorithm has
no version and never changes: no release may alter a fingerprint, a pixel or an encoded byte. A
change that makes a vector fail is a bug in the change. What the specification leaves open (API
shape, error texts, performance) may evolve under SemVer.

hh is a standalone library: it knows nothing about the applications that use it, and nothing
application-specific belongs in this repository.

## Dependencies and license

- No third-party code, anywhere: not in `src/`, not in `tests/`, not in `tools/lab`. SHA-256,
  HMAC, PBKDF2, CRC-32, Adler-32, deflate and the PNG/BMP/JPEG encoders are written here.
- C++17 standard library only. License: MIT (`LICENSE`); contributions are accepted under it.

## Style

- Everything is English: code, comments, documentation, commit messages. No emoji.
- `namespace hh`; internals in `namespace hh::detail`, written as nested `namespace hh {
  namespace detail {`.
- snake_case for types, functions and variables; data members end with an underscore.
- `#pragma once`. Public headers only in `include/hh/*.hpp` (umbrella `hh/hh.hpp`, C ABI
  `hh/hh.h`); private headers sit beside their sources in `src/`.
- Format with `.clang-format` (4 spaces, 100 columns).

## Library rules (src/, include/)

- Integer arithmetic only. No `float`, `double`, `<cmath>`. Floating point is allowed only in
  `tools/lab`.
- Total functions: every entry point returns a defined result or an error code. No exceptions
  from the core API, no undefined behaviour, no I/O, no clock, no RNG, no globals, no threads.
- Unsigned 32-bit arithmetic or explicit floor division; no signed overflow, no shifts of negative
  values. Buffers that held key material are wiped before release.
- Output is byte-identical with the other implementations; any behaviour change needs a spec
  change first.

## Build and test

- Minimum CMake 3.16; `FILE_SET` is not used. Presets need CMake 3.21.
- Plain build: `cmake -S . -B build && cmake --build build -j && ctest --test-dir build`
  (`--test-dir` needs CMake 3.20; before that, run `ctest` inside `build`).
- Zero warnings under `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion` (the
  presets add `-Werror`), with GCC and with Clang.
- The test suite must pass under ASan and UBSan: `cmake --preset asan-ubsan`,
  `cmake --build --preset asan-ubsan`, `ctest --preset asan-ubsan`.
- Tests use `tests/test_framework.hpp` (no test library). Primitives have known-answer tests
  against their standards (FIPS 180-4, RFC 4231, RFC 7914); `tests/test_vectors.cpp` reproduces
  `testdata/vectors.tsv`; `gen_vectors --check testdata` (a ctest) proves the vectors are what the
  library produces; `tests/consumer/run.sh` checks the installed package.
- `tools/lab` builds only with `-DHH_BUILD_LAB=ON`, is never installed and may use floating point
  and threads. It is research tooling, not part of the library.

## Commits

Atomic, imperative, lower case, for example "add sha-256 with fips 180-4 vectors". Every commit
builds and passes the tests.
