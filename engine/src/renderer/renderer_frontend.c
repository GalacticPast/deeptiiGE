#include "renderer_frontend.h"
#include "math/dmath.h"
#include "platform/platform.h"

#include "renderer_backend.h"

#include "core/dmemory.h"
#include "core/logger.h"

// TODO: temporary
#include "core/dstring.h"
#include "core/event.h"

#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb_image.h"
// end

// Backend render state.
typedef struct renderer_backend_state
{
    renderer_backend backend;

    mat4 projection;
    mat4 view;
    f32  near_clip;
    f32  far_clip;

    texture default_texture;

    // TODO: temporary
    texture test_diffuse;
    //
} renderer_backend_state;

static renderer_backend_state *backend_state_ptr;

void create_texture(texture *tex)
{
    dzero_memory(tex, sizeof(texture));
    tex->generation = INVALID_ID;
}

b8 load_texture(const char *texture_name, texture *tex)
{
    char *format_str = "assets/textures/%s.%s";

    const s32 required_channel_count = 4;

    stbi_set_flip_vertically_on_load(true);
    char full_file_path[512];

    string_format(full_file_path, format_str, texture_name, "png");

    texture temp_texture;

    u8 *data = stbi_load(full_file_path, (s32 *)&temp_texture.width, (s32 *)&temp_texture.height, (s32 *)&temp_texture.channel_count, required_channel_count);

    temp_texture.channel_count = required_channel_count;

    if (data)
    {
        u32 current_generation = tex->generation;
        tex->generation        = INVALID_ID;

        u64 total_size = temp_texture.width * temp_texture.height * required_channel_count;

        b32 has_transparency = false;

        for (s64 i = 0; i < total_size; i += required_channel_count)
        {
            u8 alpha_pixel = data[i + 3];
            if (alpha_pixel < 255)
            {
                has_transparency = true;
                break;
            }
        }

        if (stbi_failure_reason())
        {
            DWARN("Load texture() failed to load file '%s': %s", full_file_path, stbi_failure_reason());
        }

        renderer_create_texture(texture_name, true, temp_texture.width, temp_texture.height, required_channel_count, data, has_transparency, &temp_texture);

        // Take a copy of the proivded texture
        texture old = *tex;

        *tex = temp_texture;

        renderer_destroy_texture(&old);

        if (current_generation == INVALID_ID)
        {
            tex->generation = 0;
        }
        else
        {
            tex->generation = current_generation + 1;
        }

        stbi_image_free(data);
        return true;
    }
    else
    {
        if (stbi_failure_reason())
        {
            DWARN("Load texture() failed to load file '%s': %s", full_file_path, stbi_failure_reason());
        }
        return false;
    }
}

// WARN: temp

b8 debug_event_handler(u16 code, void *sender, void *listener_inst, event_context data)
{
    const char *names[3] = {"cobblestone", "paving", "paving2"};
    static s8   choice   = 0;

    load_texture(names[choice], &backend_state_ptr->test_diffuse);

    choice++;
    choice %= 3;

    return true;
}

//

b8 renderer_system_initialize(u64 *renderer_mem_requirements, void *state, const char *application_name)
{
    *renderer_mem_requirements = sizeof(renderer_backend_state);
    if (state == 0)
    {
        return true;
    }

    backend_state_ptr = state;

    backend_state_ptr->near_clip = 0.1f;
    backend_state_ptr->far_clip  = 1000.0f;

    event_register(EVENT_CODE_DEBUG0, NULL, debug_event_handler);

    mat4 projection               = mat4_perspective(deg_to_rad(45.0f), 1280.0f / 700.0f, backend_state_ptr->near_clip, backend_state_ptr->far_clip);
    backend_state_ptr->projection = projection;

    backend_state_ptr->view = mat4_translation((vec3){0.0f, 0.0f, 30.0f});
    backend_state_ptr->view = mat4_inverse(backend_state_ptr->view);

    backend_state_ptr->backend.default_diffuse = &backend_state_ptr->default_texture;

    renderer_backend_create(RENDERER_BACKEND_TYPE_VULKAN, &backend_state_ptr->backend);
    backend_state_ptr->backend.frame_number = 0;

    if (!backend_state_ptr->backend.initialize(&backend_state_ptr->backend, application_name))
    {
        DFATAL("Renderer backend failed to initialize. Shutting down.");
        return false;
    }

    // NOTE: Create default texture, a 256x256 blue/white checkerboard pattern.
    // This is done in code to eliminate asset dependencies.
    DDEBUG("Creating default texture...");
    const u32 tex_dimension = 256;
    const u32 channels      = 4;
    const u32 pixel_count   = tex_dimension * tex_dimension;
    u8        pixels[pixel_count * channels];
    // u8* pixels = kallocate(sizeof(u8) * pixel_count * bpp, MEMORY_TAG_TEXTURE);
    dset_memory(pixels, 255, sizeof(u8) * pixel_count * channels);

    // Each pixel.
    for (u64 row = 0; row < tex_dimension; ++row)
    {
        for (u64 col = 0; col < tex_dimension; ++col)
        {
            u64 index     = (row * tex_dimension) + col;
            u64 index_bpp = index * channels;
            if (row % 2)
            {
                if (col % 2)
                {
                    pixels[index_bpp + 0] = 0;
                    pixels[index_bpp + 1] = 0;
                }
            }
            else
            {
                if (!(col % 2))
                {
                    pixels[index_bpp + 0] = 0;
                    pixels[index_bpp + 1] = 0;
                }
            }
        }
    }
    renderer_create_texture("default", false, tex_dimension, tex_dimension, 4, pixels, false, &backend_state_ptr->default_texture);
    // because its a default texture and it shouldnt have a valid id and the function above sets it to 0
    backend_state_ptr->default_texture.generation = INVALID_ID;

    create_texture(&backend_state_ptr->test_diffuse);

    // TODO: make this configurable.

    return true;
}
void renderer_system_shutdown()
{
    if (backend_state_ptr)
    {
        event_unregister(EVENT_CODE_DEBUG0, NULL, debug_event_handler);
        renderer_destroy_texture(&backend_state_ptr->default_texture);
        renderer_destroy_texture(&backend_state_ptr->test_diffuse);
        backend_state_ptr->backend.shutdown(&backend_state_ptr->backend);
    }
    backend_state_ptr = 0;
}

b8 renderer_begin_frame(f32 delta_time)
{
    return backend_state_ptr->backend.begin_frame(&backend_state_ptr->backend, delta_time);
}

b8 renderer_end_frame(f32 delta_time)
{
    if (backend_state_ptr)
    {
        b8 result = backend_state_ptr->backend.end_frame(&backend_state_ptr->backend, delta_time);
        backend_state_ptr->backend.frame_number++;
        return result;
    }
    return false;
}

void renderer_on_resized(u16 width, u16 height)
{
    if (backend_state_ptr)
    {
        mat4 projection               = mat4_perspective(deg_to_rad(45.0f), (f32)width / (f32)height, backend_state_ptr->near_clip, backend_state_ptr->far_clip);
        backend_state_ptr->projection = projection;
        backend_state_ptr->backend.resized(&backend_state_ptr->backend, width, height);
    }
    else
    {

        DWARN("renderer backend does not exist to accept resize: %i %i", width, height);
    }
}

DAPI b8 renderer_draw_frame(render_packet *packet)
{
    // If the begin frame returned successfully, mid-frame operations may continue.
    if (!backend_state_ptr)
    {
        DWARN("renderer backend does not exist to accept to update global state");
        return false;
    }

    if (renderer_begin_frame(packet->delta_time))
    {
        backend_state_ptr->backend.update_global_game_state(backend_state_ptr->projection, backend_state_ptr->view);

        quat                 quat_axis = quat_from_axis_angle(vec3_forward(), 0.0f, false);
        mat4                 model     = quat_to_rotation_matrix(quat_axis, vec3_zero());
        geometry_render_data data      = {};
        data.object_id                 = 0;
        data.model                     = model;
        data.textures[0]               = &backend_state_ptr->test_diffuse;
        backend_state_ptr->backend.update_object(data);

        // End the frame. If this fails, it is likely unrecoverable.
        b8 result = renderer_end_frame(packet->delta_time);

        if (!result)
        {
            DERROR("renderer_end_frame failed. Application shutting down...");
            return false;
        }
    }

    return true;
}

void renderer_set_view(mat4 view)
{
    backend_state_ptr->view = view;
}

void renderer_create_texture(const char *texture_name, b8 auto_release, s32 width, s32 height, s32 channel_count, const u8 *pixels, b8 has_tranparency, struct texture *out_texture)
{
    backend_state_ptr->backend.create_texture(texture_name, auto_release, width, height, channel_count, pixels, has_tranparency, out_texture);
}
void renderer_destroy_texture(struct texture *out_texture)
{
    backend_state_ptr->backend.destroy_texture(out_texture);
}
