#pragma once

#include "defines.h"

// Returns the length of the given string.

// Case sensitive
DAPI b8 strings_equal(const char *str1, const char *str2);
// Case insensitive
DAPI b8 strings_equali(const char *str1, const char *str2);

DAPI u64   string_length(const char *str);
DAPI char *string_duplicate(const char *str);

DAPI s32 string_format(char *dest, const char *format, ...);
