#pragma once

// Umbrella include for the public C++ API of hh (Humanized Hash).
//
// hh turns a blockchain address, a public key or any hash into a deterministic
// 4 x 4 picture of simple solid figures that a person can compare at a glance.
// Three steps:
//
//   1. the base digest of the input: slow (about 16 000 HMAC calls), public,
//      cacheable                                      make_base_digest*()
//   2. the fingerprint for a mode: universal (the same for everyone) or keyed
//      (only holders of the 32-byte key can compute it)  *_fingerprint()
//   3. the picture: pixels, or a PNG, BMP or JPEG file    render(), encode_*()
//
// Every function reports an error_code and throws nothing. The library has no
// dependencies, does no I/O, keeps no global state and uses integer arithmetic
// only; its output is byte-identical with the other implementations of hh.
// docs/SPEC.md is the normative description.

#include "hh/digest.hpp"
#include "hh/encode.hpp"
#include "hh/error.hpp"
#include "hh/fingerprint.hpp"
#include "hh/image.hpp"
#include "hh/key.hpp"
#include "hh/types.hpp"
#include "hh/version.hpp"
