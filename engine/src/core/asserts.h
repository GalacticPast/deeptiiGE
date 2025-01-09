#pragma once

#include "defines.h"

// Disable assertions by commenting out the below line.
#define DASSERTIONS_ENABLED

#ifdef DASSERTIONS_ENABLED
#if _MSC_VER
#include <intrin.h>
#define debugBreak() __debugbreak()
#else
#define debugBreak() __builtin_trap()
#endif

DAPI void report_assertion_failure(const char *expression, const char *message, const char *file, i32 line);

#define DASSERT(expr)                                                                                                  \
    {                                                                                                                  \
        if (expr)                                                                                                      \
        {                                                                                                              \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            report_assertion_failure(#expr, "", __FILE__, __LINE__);                                                   \
            debugBreak();                                                                                              \
        }                                                                                                              \
    }

#define DASSERT_MSG(expr, message)                                                                                     \
    {                                                                                                                  \
        if (expr)                                                                                                      \
        {                                                                                                              \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            report_assertion_failure(#expr, message, __FILE__, __LINE__);                                              \
            debugBreak();                                                                                              \
        }                                                                                                              \
    }

#ifdef _DEBUG
#define DASSERT_DEBUG(expr)                                                                                            \
    {                                                                                                                  \
        if (expr)                                                                                                      \
        {                                                                                                              \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            report_assertion_failure(#expr, "", __FILE__, __LINE__);                                                   \
            debugBreak();                                                                                              \
        }                                                                                                              \
    }
#else
#define DASSERT_DEBUG(expr) // Does nothing at all
#endif

#else
#define DASSERT(expr)              // Does nothing at all
#define DASSERT_MSG(expr, message) // Does nothing at all
#define DASSERT_DEBUG(expr)        // Does nothing at all
#endif
