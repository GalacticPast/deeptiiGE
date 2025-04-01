#pragma once

#include "resources/resource_types.h"
#include "vulkan_types.h"

b8   vulkan_renderer_backend_initialize(renderer_backend *backend, const char *application_name);
void vulkan_renderer_backend_shutdown(renderer_backend *backend);

void vulkan_renderer_backend_on_resized(renderer_backend *backend, u16 width, u16 height);

b8 vulkan_renderer_backend_begin_frame(renderer_backend *backend, f32 delta_time);
b8 vulkan_renderer_backend_end_frame(renderer_backend *backend, f32 delta_time);

void vulkan_renderer_update_object(geometry_render_data data);

void vulkan_renderer_create_texture(const char *texture_name, b8 auto_release, s32 width, s32 height, s32 channel_count, const u8 *pixels, b8 has_tranparency, texture *out_texture);

void vulkan_renderer_destroy_texture(texture *out_texture);

void vulkan_renderer_update_global_state(mat4 projection, mat4 view);
