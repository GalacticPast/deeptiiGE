#include "application.h"
#include "game_types.h"

#include "logger.h"

#include "core/clock.h"
#include "core/dmemory.h"
#include "core/dstring.h"
#include "core/event.h"
#include "core/input.h"
#include "platform/platform.h"

#include "memory/linear_allocator.h"

#include "renderer/renderer_frontend.h"

// systems
#include "systems/geometry_system.h"
#include "systems/material_system.h"
#include "systems/resource_system.h"
#include "systems/texture_system.h"

// TODO: temp
#include "math/dmath.h"
// TODO: end temp

typedef struct application_state
{
    game *game_inst;
    b8 is_running;
    b8 is_suspended;
    s16 width;
    s16 height;
    clock clock;
    f64 last_time;
    linear_allocator systems_allocator;

    u64 event_system_memory_requirement;
    void *event_system_state;

    u64 memory_system_memory_requirement;
    void *memory_system_state;

    u64 logging_system_memory_requirement;
    void *logging_system_state;

    u64 input_system_memory_requirement;
    void *input_system_state;

    u64 platform_system_memory_requirement;
    void *platform_system_state;

    u64 resource_system_memory_requirement;
    void *resource_system_state;

    u64 renderer_system_memory_requirement;
    void *renderer_system_state;

    u64 texture_system_memory_requirement;
    void *texture_system_state;

    u64 material_system_memory_requirement;
    void *material_system_state;

    u64 geometry_system_memory_requirement;
    void *geometry_system_state;

    // TODO: temp
    geometry *test_geometry;
    // TODO: end temp

} application_state;

static application_state *app_state;

// Event handlers
b8 application_on_event(u16 code, void *sender, void *listener_inst, event_context context);
b8 application_on_key(u16 code, void *sender, void *listener_inst, event_context context);
b8 application_on_resized(u16 code, void *sender, void *listener_inst, event_context context);

// TODO: temp
b8 event_on_debug_event(u16 code, void *sender, void *listener_inst, event_context data)
{
    const char *names[3] = {"cobblestone", "paving", "paving2"};
    static s8 choice     = 2;

    // Save off the old name.
    const char *old_name = names[choice];

    choice++;
    choice %= 3;

    // Acquire the new texture.
    if (app_state->test_geometry)
    {
        app_state->test_geometry->material->diffuse_map.texture = texture_system_acquire(names[choice], true);
        if (!app_state->test_geometry->material->diffuse_map.texture)
        {
            DWARN("event_on_debug_event no texture! using default");
            app_state->test_geometry->material->diffuse_map.texture = texture_system_get_default_texture();
        }

        // Release the old texture.
        texture_system_release(old_name);
    }

    return true;
}
// TODO: end temp

b8 application_create(game *game_inst)
{
    if (game_inst->application_state)
    {
        DERROR("application_create called more than once.");
        return false;
    }

    game_inst->application_state = dallocate(sizeof(application_state), MEMORY_TAG_APPLICATION);
    app_state                    = game_inst->application_state;
    app_state->game_inst         = game_inst;
    app_state->is_running        = false;
    app_state->is_suspended      = false;

    u64 systems_allocator_total_size = 64 * 1024 * 1024; // 64 mb
    linear_allocator_create(systems_allocator_total_size, 0, &app_state->systems_allocator);

    // Initialize subsystems.

    // Events
    event_system_initialize(&app_state->event_system_memory_requirement, 0);
    app_state->event_system_state =
        linear_allocator_allocate(&app_state->systems_allocator, app_state->event_system_memory_requirement);
    event_system_initialize(&app_state->event_system_memory_requirement, app_state->event_system_state);

    // Memory
    memory_system_initialize(&app_state->memory_system_memory_requirement, 0);
    app_state->memory_system_state =
        linear_allocator_allocate(&app_state->systems_allocator, app_state->memory_system_memory_requirement);
    memory_system_initialize(&app_state->memory_system_memory_requirement, app_state->memory_system_state);

    // Logging
    initialize_logging(&app_state->logging_system_memory_requirement, 0);
    app_state->logging_system_state =
        linear_allocator_allocate(&app_state->systems_allocator, app_state->logging_system_memory_requirement);
    if (!initialize_logging(&app_state->logging_system_memory_requirement, app_state->logging_system_state))
    {
        DERROR("Failed to initialize logging system; shutting down.");
        return false;
    }

    // Input
    input_system_initialize(&app_state->input_system_memory_requirement, 0);
    app_state->input_system_state =
        linear_allocator_allocate(&app_state->systems_allocator, app_state->input_system_memory_requirement);
    input_system_initialize(&app_state->input_system_memory_requirement, app_state->input_system_state);

    // Register for engine-level events.
    event_register(EVENT_CODE_APPLICATION_QUIT, 0, application_on_event);
    event_register(EVENT_CODE_KEY_PRESSED, 0, application_on_key);
    event_register(EVENT_CODE_KEY_RELEASED, 0, application_on_key);
    event_register(EVENT_CODE_RESIZED, 0, application_on_resized);
    // TODO: temp
    event_register(EVENT_CODE_DEBUG0, 0, event_on_debug_event);
    // TODO: end temp

    // Platform
    platform_system_startup(&app_state->platform_system_memory_requirement, 0, 0, 0, 0, 0, 0);
    app_state->platform_system_state =
        linear_allocator_allocate(&app_state->systems_allocator, app_state->platform_system_memory_requirement);
    if (!platform_system_startup(&app_state->platform_system_memory_requirement, app_state->platform_system_state,
                                 game_inst->app_config.name, game_inst->app_config.start_pos_x,
                                 game_inst->app_config.start_pos_y, game_inst->app_config.start_width,
                                 game_inst->app_config.start_height))
    {
        return false;
    }

    // Resource system.
    resource_system_config resource_sys_config;
    resource_sys_config.asset_base_path  = "../assets";
    resource_sys_config.max_loader_count = 32;
    resource_system_initialize(&app_state->resource_system_memory_requirement, 0, resource_sys_config);
    app_state->resource_system_state =
        linear_allocator_allocate(&app_state->systems_allocator, app_state->resource_system_memory_requirement);
    if (!resource_system_initialize(&app_state->resource_system_memory_requirement, app_state->resource_system_state,
                                    resource_sys_config))
    {
        DFATAL("Failed to initialize resource system. Aborting application.");
        return false;
    }

    // Renderer system
    renderer_system_initialize(&app_state->renderer_system_memory_requirement, 0, 0);
    app_state->renderer_system_state =
        linear_allocator_allocate(&app_state->systems_allocator, app_state->renderer_system_memory_requirement);
    if (!renderer_system_initialize(&app_state->renderer_system_memory_requirement, app_state->renderer_system_state,
                                    game_inst->app_config.name))
    {
        DFATAL("Failed to initialize renderer. Aborting application.");
        return false;
    }

    // Texture system.
    texture_system_config texture_sys_config;
    texture_sys_config.max_texture_count = 65536;
    texture_system_initialize(&app_state->texture_system_memory_requirement, 0, texture_sys_config);
    app_state->texture_system_state =
        linear_allocator_allocate(&app_state->systems_allocator, app_state->texture_system_memory_requirement);
    if (!texture_system_initialize(&app_state->texture_system_memory_requirement, app_state->texture_system_state,
                                   texture_sys_config))
    {
        DFATAL("Failed to initialize texture system. Application cannot continue.");
        return false;
    }

    // Material system.
    material_system_config material_sys_config;
    material_sys_config.max_material_count = 4096;
    material_system_initialize(&app_state->material_system_memory_requirement, 0, material_sys_config);
    app_state->material_system_state =
        linear_allocator_allocate(&app_state->systems_allocator, app_state->material_system_memory_requirement);
    if (!material_system_initialize(&app_state->material_system_memory_requirement, app_state->material_system_state,
                                    material_sys_config))
    {
        DFATAL("Failed to initialize material system. Application cannot continue.");
        return false;
    }

    // Geometry system.
    geometry_system_config geometry_sys_config;
    geometry_sys_config.max_geometry_count = 4096;
    geometry_system_initialize(&app_state->geometry_system_memory_requirement, 0, geometry_sys_config);
    app_state->geometry_system_state =
        linear_allocator_allocate(&app_state->systems_allocator, app_state->material_system_memory_requirement);
    if (!geometry_system_initialize(&app_state->geometry_system_memory_requirement, app_state->geometry_system_state,
                                    geometry_sys_config))
    {
        DFATAL("Failed to initialize geometry system. Application cannot continue.");
        return false;
    }

    // TODO: temp

    // Load up a plane configuration, and load geometry from it.
    geometry_config g_config =
        geometry_system_generate_plane_config(10.0f, 5.0f, 5, 5, 5.0f, 2.0f, "test geometry", "test_material");
    app_state->test_geometry = geometry_system_acquire_from_config(g_config, true);

    // Clean up the allocations for the geometry config.
    dfree(g_config.vertices, sizeof(vertex_3d) * g_config.vertex_count, MEMORY_TAG_ARRAY);
    dfree(g_config.indices, sizeof(u32) * g_config.index_count, MEMORY_TAG_ARRAY);

    // Load up default geometry.
    // app_state->test_geometry = geometry_system_get_default();
    // TODO: end temp

    // Initialize the game.
    if (!app_state->game_inst->initialize(app_state->game_inst))
    {
        DFATAL("Game failed to initialize.");
        return false;
    }

    // Call resize once to ensure the proper size has been set.
    app_state->game_inst->on_resize(app_state->game_inst, app_state->width, app_state->height);

    return true;
}

b8 application_run()
{
    app_state->is_running = true;
    clock_start(&app_state->clock);
    clock_update(&app_state->clock);
    app_state->last_time     = app_state->clock.elapsed;
    f64 running_time         = 0;
    u8 frame_count           = 0;
    f64 target_frame_seconds = 1.0f / 60;

    DINFO(get_memory_usage_str());

    while (app_state->is_running)
    {
        if (!platform_pump_messages())
        {
            app_state->is_running = false;
        }

        if (!app_state->is_suspended)
        {
            // Update clock and get delta time.
            clock_update(&app_state->clock);
            f64 current_time     = app_state->clock.elapsed;
            f64 delta            = (current_time - app_state->last_time);
            f64 frame_start_time = platform_get_absolute_time();

            if (!app_state->game_inst->update(app_state->game_inst, (f32)delta))
            {
                DFATAL("Game update failed, shutting down.");
                app_state->is_running = false;
                break;
            }

            // Call the game's render routine.
            if (!app_state->game_inst->render(app_state->game_inst, (f32)delta))
            {
                DFATAL("Game render failed, shutting down.");
                app_state->is_running = false;
                break;
            }

            // TODO: refactor packet creation
            render_packet packet;
            packet.delta_time = delta;

            // TODO: temp
            geometry_render_data test_render;
            test_render.geometry = app_state->test_geometry;
            test_render.model    = mat4_identity();

            packet.geometry_count = 1;
            packet.geometries     = &test_render;
            // TODO: end temp

            renderer_draw_frame(&packet);

            // Figure out how long the frame took and, if below
            f64 frame_end_time     = platform_get_absolute_time();
            f64 frame_elapsed_time = frame_end_time - frame_start_time;
            running_time += frame_elapsed_time;
            f64 remaining_seconds = target_frame_seconds - frame_elapsed_time;

            if (remaining_seconds > 0)
            {
                u64 remaining_ms = (remaining_seconds * 1000);

                // If there is time left, give it back to the OS.
                b8 limit_frames = false;
                if (remaining_ms > 0 && limit_frames)
                {
                    platform_sleep(remaining_ms - 1);
                }

                frame_count++;
            }

            // NOTE: Input update/state copying should always be handled
            // after any input should be recorded; I.E. before this line.
            // As a safety, input is the last thing to be updated before
            // this frame ends.
            input_update(delta);

            // Update last time
            app_state->last_time = current_time;
        }
    }

    app_state->is_running = false;

    // Shutdown event system.
    event_unregister(EVENT_CODE_APPLICATION_QUIT, 0, application_on_event);
    event_unregister(EVENT_CODE_KEY_PRESSED, 0, application_on_key);
    event_unregister(EVENT_CODE_KEY_RELEASED, 0, application_on_key);
    // TODO: temp
    event_unregister(EVENT_CODE_DEBUG0, 0, event_on_debug_event);
    // TODO: end temp

    input_system_shutdown(app_state->input_system_state);

    geometry_system_shutdown(app_state->geometry_system_state);

    material_system_shutdown(app_state->material_system_state);

    texture_system_shutdown(app_state->texture_system_state);

    renderer_system_shutdown(app_state->renderer_system_state);

    resource_system_shutdown(app_state->resource_system_state);

    platform_system_shutdown(app_state->platform_system_state);

    memory_system_shutdown(app_state->memory_system_state);

    event_system_shutdown(app_state->event_system_state);

    return true;
}

void application_get_framebuffer_size(u32 *width, u32 *height)
{
    *width  = app_state->width;
    *height = app_state->height;
}

b8 application_on_event(u16 code, void *sender, void *listener_inst, event_context context)
{
    switch (code)
    {
    case EVENT_CODE_APPLICATION_QUIT: {
        DINFO("EVENT_CODE_APPLICATION_QUIT recieved, shutting down.\n");
        app_state->is_running = false;
        return true;
    }
    }

    return false;
}

b8 application_on_key(u16 code, void *sender, void *listener_inst, event_context context)
{
    if (code == EVENT_CODE_KEY_PRESSED)
    {
        u16 key_code = context.data.u16[0];
        if (key_code == KEY_ESCAPE)
        {
            // NOTE: Technically firing an event to itself, but there may be other listeners.
            event_context data = {};
            event_fire(EVENT_CODE_APPLICATION_QUIT, 0, data);

            // Block anything else from processing this.
            return true;
        }
        else if (key_code == KEY_A)
        {
            // Example on checking for a key
            DDEBUG("Explicit - A key pressed!");
        }
        else
        {
            DDEBUG("'%c' key pressed in window.", key_code);
        }
    }
    else if (code == EVENT_CODE_KEY_RELEASED)
    {
        u16 key_code = context.data.u16[0];
        if (key_code == KEY_B)
        {
            // Example on checking for a key
            DDEBUG("Explicit - B key released!");
        }
        else
        {
            DDEBUG("'%c' key released in window.", key_code);
        }
    }
    return false;
}

b8 application_on_resized(u16 code, void *sender, void *listener_inst, event_context context)
{
    if (code == EVENT_CODE_RESIZED)
    {
        u16 width  = context.data.u32[0];
        u16 height = context.data.u32[1];

        // Check if different. If so, trigger a resize event.
        if (width != app_state->width || height != app_state->height)
        {
            app_state->width  = width;
            app_state->height = height;

            DDEBUG("Window resize: %i, %i", width, height);

            // Handle minimization
            if (width == 0 || height == 0)
            {
                DINFO("Window minimized, suspending application.");
                app_state->is_suspended = true;
                return true;
            }
            else
            {
                if (app_state->is_suspended)
                {
                    DINFO("Window restored, resuming application.");
                    app_state->is_suspended = false;
                }
                app_state->game_inst->on_resize(app_state->game_inst, width, height);
                renderer_on_resized(width, height);
            }
        }
    }

    // Event purposely not handled to allow other listeners to get this.
    return false;
}
