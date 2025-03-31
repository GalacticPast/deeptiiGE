#include "renderer_frontend.h"

#include "renderer_backend.h"

#include "core/dmemory.h"
#include "core/logger.h"

// Backend render context.
static renderer_backend *backend_ptr = 0;

b8 renderer_initialize(u64 *renderer_mem_requirements, void *state, const char *application_name)
{
    *renderer_mem_requirements = sizeof(renderer_backend);
    if (state == 0)
    {
        return true;
    }

    backend_ptr = state;

    // TODO: make this configurable.
    renderer_backend_create(RENDERER_BACKEND_TYPE_VULKAN, 0, backend_ptr);
    backend_ptr->frame_number = 0;

    if (!backend_ptr->initialize(backend_ptr, application_name, 0))
    {
        DFATAL("Renderer backend failed to initialize. Shutting down.");
        return false;
    }

    return true;
}

void renderer_shutdown()
{
    backend_ptr->shutdown(backend_ptr);
    backend_ptr = 0;
}

b8 renderer_begin_frame(f32 delta_time)
{
    return backend_ptr->begin_frame(backend_ptr, delta_time);
}

b8 renderer_end_frame(f32 delta_time)
{
    if (backend_ptr)
    {
        b8 result = backend_ptr->end_frame(backend_ptr, delta_time);
        backend_ptr->frame_number++;
        return result;
    }
    return false;
}

void renderer_on_resized(u16 width, u16 height)
{
    if (backend_ptr)
    {
        backend_ptr->resized(backend_ptr, width, height);
    }
    else
    {
        DWARN("renderer backend does not exist to accept resize: %i %i", width, height);
    }
}

DAPI b8 renderer_draw_frame(render_packet *packet)
{
    // If the begin frame returned successfully, mid-frame operations may continue.
    if (renderer_begin_frame(packet->delta_time))
    {

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
