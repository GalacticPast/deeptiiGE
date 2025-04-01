#include "renderer_frontend.h"
#include "math/dmath.h"
#include "platform/platform.h"

#include "renderer_backend.h"

#include "core/dmemory.h"
#include "core/logger.h"

// Backend render state.
typedef struct renderer_backend_state
{
    renderer_backend backend;

    mat4 projection;
    mat4 view;
    f32  near_clip;
    f32  far_clip;
} renderer_backend_state;

static renderer_backend_state *backend_state_ptr;

b8 renderer_system_initialize(u64 *renderer_mem_requirements, void *state, const char *application_name)
{
    *renderer_mem_requirements = sizeof(renderer_backend_state);
    if (state == 0)
    {
        return true;
    }

    backend_state_ptr = state;

    // TODO: make this configurable.
    renderer_backend_create(RENDERER_BACKEND_TYPE_VULKAN, 0, &backend_state_ptr->backend);
    backend_state_ptr->backend.frame_number = 0;

    if (!backend_state_ptr->backend.initialize(&backend_state_ptr->backend, application_name, 0))
    {
        DFATAL("Renderer backend failed to initialize. Shutting down.");
        return false;
    }
    backend_state_ptr->near_clip = 0.1f;
    backend_state_ptr->far_clip = 1000.0f;

    mat4 projection = mat4_perspective(deg_to_rad(45.0f), 1280.0f / 700.0f, backend_state_ptr->near_clip, backend_state_ptr->far_clip);
    backend_state_ptr->projection = projection;

    backend_state_ptr->view = mat4_translation((vec3){0.0f, 0.0f, 30.0f});
    backend_state_ptr->view = mat4_inverse(backend_state_ptr->view);

    return true;
}

void renderer_system_shutdown()
{
    backend_state_ptr->backend.shutdown(&backend_state_ptr->backend);
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
        mat4 projection = mat4_perspective(deg_to_rad(45.0f), (f32)width / (f32)height, backend_state_ptr->near_clip, backend_state_ptr->far_clip);
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

        static f32 angle = 0.01f;
        angle += 0.001f;

        quat quat_axis = quat_from_axis_angle(vec3_forward(), angle, false);
        mat4 model = quat_to_rotation_matrix(quat_axis, vec3_zero());
        backend_state_ptr->backend.update_object(model);

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
