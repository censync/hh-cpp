#pragma once

#include <cstddef>

namespace hh {
namespace detail {

// secure_wipe overwrites memory with zeros through a volatile pointer, so the
// optimiser cannot drop the stores as dead. Used for buffers that held key
// material before they are released.
void secure_wipe(void* data, std::size_t size) noexcept;

}  // namespace detail
}  // namespace hh
