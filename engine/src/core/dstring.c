#include "core/dstring.h"
#include "core/dmemory.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

s32 string_format_v(char *dest, const char *format, void *va_listp);

u64 string_length(const char *str)
{
    return strlen(str);
}

char *string_duplicate(const char *str)
{
    u64   length = string_length(str);
    char *copy   = dallocate(length + 1, MEMORY_TAG_STRING);
    dcopy_memory(copy, str, length + 1);
    return copy;
}

// Case sensitive
b8 strings_equal(const char *str1, const char *str2)
{
    s32 result = strcmp(str1, str2);
    return result == 0 ? true : false;
}
// Case insensitive
b8 strings_equali(const char *str1, const char *str2)
{
#if defined(__GNUC__)
    return strcasecmp(str1, str2) == 0;
#elif (defined _MSC_VER)
    return _strcmpi(str1, str2) == 0;
#endif
}

s32 string_format(char *dest, const char *format, ...)
{
    if (dest)
    {
        __builtin_va_list arg_ptr;
        va_start(arg_ptr, format);
        s32 written = string_format_v(dest, format, arg_ptr);
        va_end(arg_ptr);
        return written;
    }
    return -1;
}

s32 string_format_v(char *dest, const char *format, void *va_listp)
{
    if (dest)
    {
        // Big, but can fit on the stack.
        char buffer[32000];
        s32  written    = vsnprintf(buffer, 32000, format, va_listp);
        buffer[written] = 0;
        dcopy_memory(dest, buffer, written + 1);

        return written;
    }
    return -1;
}
