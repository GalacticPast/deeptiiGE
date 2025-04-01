#pragma once

#include "defines.h"
#include "math/dmath.h"

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

typedef struct renderer_backend
{
    struct platform_state *plat_state;
    u64                    frame_number;

    b8 (*initialize)(struct renderer_backend *backend, const char *application_name, struct platform_state *plat_state);

    void (*shutdown)(struct renderer_backend *backend);

    void (*resized)(struct renderer_backend *backend, u16 width, u16 height);

    b8 (*begin_frame)(struct renderer_backend *backend, f32 delta_time);

    void (*update_global_game_state)(mat4 projection, mat4 view);

    b8 (*end_frame)(struct renderer_backend *backend, f32 delta_time);

    void (*update_object)(mat4 model);
} renderer_backend;

typedef struct render_packet
{
    f32 delta_time;
} render_packet;
