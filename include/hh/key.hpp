#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "hh/error.hpp"
#include "hh/export.hpp"
#include "hh/types.hpp"

namespace hh {

namespace detail {
struct key_access;
}

// The 32-byte secret of keyed mode. The bytes are wiped when the object is
// destroyed, assigned over or moved from. The key must be uniformly random or
// the output of a key derivation function; there is no passphrase form.
class secret_key {
public:
    static constexpr std::size_t size = 32;

    // An unset key. Functions that take it report invalid_key.
    HH_API secret_key() noexcept;
    HH_API ~secret_key();

    HH_API secret_key(const secret_key& other) noexcept;
    HH_API secret_key& operator=(const secret_key& other) noexcept;
    HH_API secret_key(secret_key&& other) noexcept;
    HH_API secret_key& operator=(secret_key&& other) noexcept;

    bool empty() const noexcept { return !set_; }

    // The key check value: 4 bytes a host stores beside its cached data to
    // notice that the key, and with it every keyed picture, changed. All zero
    // if the key is unset.
    HH_API std::array<std::uint8_t, 4> kcv() const noexcept;

private:
    friend struct detail::key_access;

    std::array<std::uint8_t, size> bytes_;
    bool set_;
};

// Accepts exactly 32 bytes that are not all zero; anything else is invalid_key.
HH_API error_code make_secret_key(byte_view bytes32, secret_key& out) noexcept;

}  // namespace hh
