#pragma once

#include "hh/export.hpp"

namespace hh {

// Every entry point reports one of these codes; the core API throws nothing.
// The numeric values are part of the C ABI (SPEC.md section 14).
enum class error_code : int {
    ok = 0,
    empty_input = 1,
    input_too_large = 2,
    invalid_hex = 3,
    invalid_key = 4,
    invalid_digest = 5,
    invalid_fingerprint = 6,
    invalid_size = 7,
    invalid_frame = 8,
    low_contrast = 9,
    invalid_quality = 10,
    invalid_image = 11,
    buffer_too_small = 12,
    out_of_memory = 13,
    invalid_argument = 14,
};

// A short English description of the code. Never null.
HH_API const char* error_message(error_code code) noexcept;

// The name of the code as the specification and the golden vectors spell it
// ("invalid_hex"). Never null.
HH_API const char* error_name(error_code code) noexcept;

}  // namespace hh
