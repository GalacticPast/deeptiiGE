#pragma once

#include "defines.h"
#include "math/dmath.h"
#include "resources/resource_types.h"

typedef enum renderer_backend_type
{
    RENDERER_BACKEND_TYPE_VULKAN,
    RENDERER_BACKEND_TYPE_OPENGL,
    RENDERER_BACKEND_TYPE_DIRECTX
} renderer_backend_type;

typedef struct global_uniform_object
{
    mat4 projection;  // 64 bytes
    mat4 view;        // 64 bytes
    mat4 m_reserved0; // INFO: padding because nvidia requires global uniform objects to be 256 bytes wide
    mat4 m_reserved1;
} global_uniform_object;

typedef struct object_uniform_object
{
    vec4 diffuse_color; // 16 bytes
    vec4 m_reserved0;   // INFO: padding because nvidia requires local uniform object to be 64 bytes wide
    vec4 m_reserved1;
    vec4 m_reserved2;
} object_uniform_object;

typedef struct geometry_render_data
{
    u32      object_id;
    mat4     model;
    texture *textures[16];
} geometry_render_data;

// declaration so that we dont have to forward define and include bunch of header files

typedef struct renderer_backend
{

    u64 frame_number;

    b8 (*initialize)(struct renderer_backend *backend, const char *application_name);

    void (*shutdown)(struct renderer_backend *backend);

    void (*resized)(struct renderer_backend *backend, u16 width, u16 height);

    b8 (*begin_frame)(struct renderer_backend *backend, f32 delta_time);

    b8 (*end_frame)(struct renderer_backend *backend, f32 delta_time);

    void (*update_global_game_state)(mat4 projection, mat4 view);

    void (*update_object)(geometry_render_data data);

    void (*create_texture)(const char *texture_name, s32 width, s32 height, s32 channel_count, const u8 *pixels, b8 has_tranparency, struct texture *out_texture);

    void (*destroy_texture)(texture *out_texture);

} renderer_backend;

typedef struct render_packet
{
    f32 delta_time;
} render_packet;
