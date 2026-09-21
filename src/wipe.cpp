#include "wipe.hpp"

namespace hh {
namespace detail {

void secure_wipe(void* data, std::size_t size) noexcept {
    volatile unsigned char* p = static_cast<volatile unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        p[i] = 0;
    }
}

}  // namespace detail
}  // namespace hh
