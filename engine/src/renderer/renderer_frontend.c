#include "renderer_frontend.h"
#include "math/dmath.h"
#include "platform/platform.h"

#include "renderer_backend.h"

#include "core/dmemory.h"
#include "core/logger.h"

// TODO: temporary
#include "core/dstring.h"
#include "core/event.h"

#include "systems/material_system.h"
#include "systems/texture_system.h"

// Backend render state.
typedef struct renderer_backend_state
{
    renderer_backend backend;

    mat4 projection;
    mat4 view;
    f32  near_clip;
    f32  far_clip;

    // TODO: temporary
    material *test_material;
    //
} renderer_backend_state;

static renderer_backend_state *backend_state_ptr;

b8 debug_event_handler(u16 code, void *sender, void *listener_inst, event_context data)
{
    const char *names[3] = {"cobblestone", "paving", "paving2"};
    static s8   choice   = 2;

    const char *old_name = names[choice];

    choice++;
    choice %= 3;

    backend_state_ptr->test_material->diffuse_map.texture = texture_system_acquire(names[choice], true);

    if (!backend_state_ptr->test_material->diffuse_map.texture)
    {
        DWARN("debug_event_handler i.e(texture switching) no texture! Using default texture");
        backend_state_ptr->test_material->diffuse_map.texture = texture_system_get_default_texture();
    }

    texture_system_release(old_name);

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

    event_register(EVENT_CODE_DEBUG0, 0, debug_event_handler);

    mat4 projection               = mat4_perspective(deg_to_rad(45.0f), 1280.0f / 700.0f, backend_state_ptr->near_clip, backend_state_ptr->far_clip);
    backend_state_ptr->projection = projection;

    backend_state_ptr->view = mat4_translation((vec3){0.0f, 0.0f, 30.0f});
    backend_state_ptr->view = mat4_inverse(backend_state_ptr->view);

    renderer_backend_create(RENDERER_BACKEND_TYPE_VULKAN, &backend_state_ptr->backend);
    backend_state_ptr->backend.frame_number = 0;

    if (!backend_state_ptr->backend.initialize(&backend_state_ptr->backend, application_name))
    {
        DFATAL("Renderer backend failed to initialize. Shutting down.");
        return false;
    }

    return true;
}
void renderer_system_shutdown()
{
    if (backend_state_ptr)
    {
        event_unregister(EVENT_CODE_DEBUG0, 0, debug_event_handler);
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

        quat quat_axis = quat_from_axis_angle(vec3_forward(), 0.0f, false);
        mat4 model     = quat_to_rotation_matrix(quat_axis, vec3_zero());

        geometry_render_data data = {};
        data.model                = model;

        // TODO: temporary.
        // Create a default material if does not exist.
        if (!backend_state_ptr->test_material)
        {
            // Automatic config
            backend_state_ptr->test_material = material_system_acquire("test_material");
            if (!backend_state_ptr->test_material)
            {
                DWARN("Automatic material load failed, falling back to manual default material.");
                // Manual config
                material_config config;
                string_ncopy(config.name, "test_material", MATERIAL_NAME_MAX_LENGTH);
                config.auto_release   = false;
                config.diffuse_color = vec4_one(); // white
                string_ncopy(config.diffuse_map_name, DEFAULT_TEXTURE_NAME, TEXTURE_NAME_MAX_LENGTH);
                backend_state_ptr->test_material = material_system_acquire_from_config(config);
            }
        }

        data.material = backend_state_ptr->test_material;
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

void renderer_create_texture(struct texture *out_texture, const u8 *pixels)
{
    backend_state_ptr->backend.create_texture(out_texture, pixels);
}
void renderer_destroy_texture(struct texture *out_texture)
{
    backend_state_ptr->backend.destroy_texture(out_texture);
}

b8 renderer_create_material(struct material *material)
{
    b8 result = backend_state_ptr->backend.create_material(material);
    return result;
}
void renderer_destroy_material(struct material *material)
{
    backend_state_ptr->backend.destroy_material(material);
}
