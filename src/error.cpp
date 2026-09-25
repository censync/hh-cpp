#include <hh/error.hpp>
#include <hh/version.hpp>

namespace hh {

const char* error_message(error_code code) noexcept {
    switch (code) {
        case error_code::ok:
            return "success";
        case error_code::empty_input:
            return "the input has no bytes";
        case error_code::input_too_large:
            return "the input is longer than 1048576 bytes";
        case error_code::invalid_hex:
            return "the string is not an even number of hexadecimal digits";
        case error_code::invalid_key:
            return "the key must be 32 bytes that are not all zero";
        case error_code::invalid_digest:
            return "the base digest must be 32 bytes and must be set";
        case error_code::invalid_fingerprint:
            return "the fingerprint must be 32 bytes with a known mode and must be set";
        case error_code::invalid_size:
            return "the image size must be 16..1024 and leave room for the cells";
        case error_code::invalid_frame:
            return "the frame style does not fit the shape";
        case error_code::low_contrast:
            return "the background is too close to a palette colour";
        case error_code::invalid_quality:
            return "the JPEG quality must be 50..100";
        case error_code::invalid_image:
            return "the image dimensions or its buffer length are invalid";
        case error_code::buffer_too_small:
            return "the buffer cannot hold the result";
        case error_code::out_of_memory:
            return "an allocation failed";
        case error_code::invalid_argument:
            return "a null pointer or an unknown enumeration value";
    }
    return "unknown error";
}

const char* error_name(error_code code) noexcept {
    switch (code) {
        case error_code::ok:
            return "ok";
        case error_code::empty_input:
            return "empty_input";
        case error_code::input_too_large:
            return "input_too_large";
        case error_code::invalid_hex:
            return "invalid_hex";
        case error_code::invalid_key:
            return "invalid_key";
        case error_code::invalid_digest:
            return "invalid_digest";
        case error_code::invalid_fingerprint:
            return "invalid_fingerprint";
        case error_code::invalid_size:
            return "invalid_size";
        case error_code::invalid_frame:
            return "invalid_frame";
        case error_code::low_contrast:
            return "low_contrast";
        case error_code::invalid_quality:
            return "invalid_quality";
        case error_code::invalid_image:
            return "invalid_image";
        case error_code::buffer_too_small:
            return "buffer_too_small";
        case error_code::out_of_memory:
            return "out_of_memory";
        case error_code::invalid_argument:
            return "invalid_argument";
    }
    return "unknown";
}

const char* version() noexcept {
    return HH_VERSION_STRING;
}

}  // namespace hh
