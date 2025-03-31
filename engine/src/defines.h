#pragma once

// Unsigned int types.
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

// Signed int types.
typedef signed char      s8;
typedef signed short     s16;
typedef signed int       s32;
typedef signed long long s64;

// Floating point types
typedef float  f32;
typedef double f64;

// Boolean types
typedef int  b32;
typedef char b8;

// Properly define static assertions.
#if defined(__clang__) || defined(__gcc__)
#define STATIC_ASSERT _Static_assert
#else
#define STATIC_ASSERT static_assert
#endif

// Ensure all types are of the correct size.
STATIC_ASSERT(sizeof(u8) == 1, "Expected u8 to be 1 byte.");
STATIC_ASSERT(sizeof(u16) == 2, "Expected u16 to be 2 bytes.");
STATIC_ASSERT(sizeof(u32) == 4, "Expected u32 to be 4 bytes.");
STATIC_ASSERT(sizeof(u64) == 8, "Expected u64 to be 8 bytes.");

STATIC_ASSERT(sizeof(s8) == 1, "Expected s8 to be 1 byte.");
STATIC_ASSERT(sizeof(s16) == 2, "Expected s16 to be 2 bytes.");
STATIC_ASSERT(sizeof(s32) == 4, "Expected s32 to be 4 bytes.");
STATIC_ASSERT(sizeof(s64) == 8, "Expected s64 to be 8 bytes.");

STATIC_ASSERT(sizeof(f32) == 4, "Expected f32 to be 4 bytes.");
STATIC_ASSERT(sizeof(f64) == 8, "Expected f64 to be 8 bytes.");

#define true 1
#define false 0

// Platform detection
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#ifndef _WIN64
#error "64-bit is required on Windows!"
#endif
#elif defined(__linux__) || defined(__gnu_linux__)
// Linux OS
#define DPLATFORM_LINUX 1
#if defined(__ANDROID__)
#define DPLATFORM_ANDROID 1
#endif
#elif defined(__unix__)
// Catch anything not caught by the above.
#define DPLATFORM_UNIX 1
#elif defined(_POSIX_VERSION)
// Posix
#define DPLATFORM_POSIX 1
#elif __APPLE__
// Apple platforms
#define DPLATFORM_APPLE 1
#include <TargetConditionals.h>
#if TARGET_IPHONE_SIMULATOR
// iOS Simulator
#define DPLATFORM_IOS 1
#define DPLATFORM_IOS_SIMULATOR 1
#elif TARGET_OS_IPHONE
#define DPLATFORM_IOS 1
// iOS device
#elif TARGET_OS_MAC
// Other kinds of Mac OS
#else
#error "Unknown Apple platform"
#endif
#else
#error "Unknown platform!"
#endif

#ifdef DEXPORT
// Exports
#ifdef _MSC_VER
#define DAPI __declspec(dllexport)
#else
#define DAPI __attribute__((visibility("default")))
#endif
#else
// Imports
#ifdef _MSC_VER
#define DAPI __declspec(dllimport)
#else
#define DAPI
#endif
#endif
#define DCLAMP(value, min, max) (value <= min) ? min : (value >= max) ? max : value;

#ifdef _MSC_VER
#define DINLINE __forceinline
#define KNOINLINE __declspec(noinline)
#else
#define DINLINE static inline
#define KNOINLINE
#endif
