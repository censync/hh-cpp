#pragma once

// HH_API marks the public symbols. The library is built with hidden visibility,
// so a shared build exports exactly what carries this macro.

#if defined(_WIN32) && defined(HH_SHARED)
#if defined(HH_BUILDING)
#define HH_API __declspec(dllexport)
#else
#define HH_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define HH_API __attribute__((visibility("default")))
#else
#define HH_API
#endif
