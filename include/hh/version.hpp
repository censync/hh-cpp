#pragma once

#include "hh/export.hpp"

#define HH_VERSION_MAJOR 1
#define HH_VERSION_MINOR 0
#define HH_VERSION_PATCH 0
#define HH_VERSION_STRING "1.0.0"

namespace hh {

// The library version as "major.minor.patch". The algorithm itself is frozen
// and has no version: no release changes a fingerprint, a pixel or an encoded byte.
HH_API const char* version() noexcept;

}  // namespace hh
