#pragma once

#include "defines.h"

b8 platform_system_startup(u64 *platform_mem_requirements, void *plat_state, const char *application_name, s32 x, s32 y, s32 width, s32 height);

void platform_system_shutdown(void *state);

b8 platform_pump_messages();

void *platform_allocate(u64 size, b8 aligned);
void  platform_free(void *block, b8 aligned);
void *platform_zero_memory(void *block, u64 size);
void *platform_copy_memory(void *dest, const void *source, u64 size);
void *platform_set_memory(void *dest, s32 value, u64 size);

void platform_console_write(const char *message, u8 color);
void platform_console_write_error(const char *message, u8 color);

f64 platform_get_absolute_time();

void platform_get_window_dimensions(u32 *width, u32 *height);
// Sleep on the thread for the provided ms. This blocks the main thread.
// Should only be used for giving time back to the OS for unused update power.
// Therefore it is not exported.
void platform_sleep(u64 ms);
