#include "core/dstring.h"
#include "core/dmemory.h"

#include <string.h>

u64 string_length(const char *str)
{
    return strlen(str);
}

b8 string_compare(const char *str1, const char *str2)
{
    i32 result = strcmp(str1, str2);
    return result == 0 ? true : false;
}

char *string_duplicate(const char *str)
{
    u64   length = string_length(str);
    char *copy = dallocate(length + 1, MEMORY_TAG_STRING);
    dcopy_memory(copy, str, length + 1);
    return copy;
}
