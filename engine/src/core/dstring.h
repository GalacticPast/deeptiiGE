#pragma once

#include "defines.h"

// Returns the length of the given string.

DAPI b8    string_compare(const char *str1, const char *str2);
DAPI b8    string_equal(const char *str1, const char *str2);
DAPI u64   string_length(const char *str);
DAPI char *string_duplicate(const char *str);
