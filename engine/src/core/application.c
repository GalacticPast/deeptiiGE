#include "application.h"
#include "game_types.h"

#include "logger.h"

#include "core/dclock.h"
#include "core/dmemory.h"
#include "core/event.h"
#include "core/input.h"
#include "memory/linear_allocator.h"
#include "platform/platform.h"

#include "renderer/renderer_frontend.h"

typedef struct application_state
{
    game            *game_inst;
    b8               is_running;
    b8               is_suspended;
    platform_state   platform;
    s16              width;
    s16              height;
    clock            clock;
    f64              last_time;
    linear_allocator systems_allocator;

    u64   logging_system_mem_requirements;
    void *logging_system_state;
} application_state;

static application_state *app_state;

// Event handlers
b8 application_on_event(u16 code, void *sender, void *listener_inst, event_context context);
b8 application_on_key(u16 code, void *sender, void *listener_inst, event_context context);

b8 application_create(game *game_inst)
{

    if (game_inst->application_state)
    {
        DERROR("application_create called more than once.");
        return false;
    }

    game_inst->application_state = dallocate(sizeof(application_state), MEMORY_TAG_APPLICATION);
    app_state = game_inst->application_state;
    app_state->game_inst = game_inst;
    app_state->is_running = false;
    app_state->is_suspended = false;

    u64 systems_allocator_total_size = 64 * 1024 * 1024;
    linear_allocator_create(systems_allocator_total_size, 0, &app_state->systems_allocator);

    // Initialize subsystems.
    initialize_logging(&app_state->logging_system_mem_requirements, 0);
    app_state->logging_system_state = linear_allocator_allocate(&app_state->systems_allocator, app_state->logging_system_mem_requirements);
    b8 result = initialize_logging(&app_state->logging_system_mem_requirements, app_state->logging_system_state);

    if (!result)
    {
        DERROR("Logging system initialization failed. Shutting down.");
        return false;
    }

    input_initialize();

    // TODO: Remove this
    DFATAL("A test message: %f", 3.14f);
    DERROR("A test message: %f", 3.14f);
    DWARN("A test message: %f", 3.14f);
    DINFO("A test message: %f", 3.14f);
    DDEBUG("A test message: %f", 3.14f);
    DTRACE("A test message: %f", 3.14f);

    if (!event_initialize())
    {
        DERROR("Event system failed initialization. Application cannot continue.");
        return false;
    }

    event_register(EVENT_CODE_APPLICATION_QUIT, 0, application_on_event);
    event_register(EVENT_CODE_RESIZED, 0, application_on_event);
    event_register(EVENT_CODE_KEY_PRESSED, 0, application_on_key);
    event_register(EVENT_CODE_KEY_RELEASED, 0, application_on_key);

    if (!platform_startup(&app_state->platform, game_inst->app_config.name, game_inst->app_config.start_pos_x, game_inst->app_config.start_pos_y, game_inst->app_config.start_width, game_inst->app_config.start_height))
    {
        DFATAL("platform startup failed");
        return false;
    }

    if (!renderer_initialize(game_inst->app_config.name, &app_state->platform))
    {
        DFATAL("renderer initialze failed");
        return false;
    }

    // Initialize the game.
    if (!app_state->game_inst->initialize(app_state->game_inst))
    {
        DFATAL("Game failed to initialize.");
        return false;
    }

    if (!app_state->game_inst->render(app_state->game_inst, 0))
    {
        DFATAL("game render failed to initialize.");
        return false;
    }

    app_state->game_inst->on_resize(app_state->game_inst, app_state->width, app_state->height);

    return true;
}

b8 application_run()
{
    DINFO(get_memory_usage_str());

    while (app_state->is_running)
    {
        if (!platform_pump_messages(&app_state->platform))
        {
            app_state->is_running = false;
        }
        // TODO: nochekin

        if (!app_state->is_suspended)
        {
            if (!app_state->game_inst->update(app_state->game_inst, (f32)0))
            {
                DFATAL("Game update failed, shutting down.");
                app_state->is_running = false;
                break;
            }

            // Call the game's render routine.
            if (!app_state->game_inst->render(app_state->game_inst, (f32)0))
            {
                DFATAL("Game render failed, shutting down.");
                app_state->is_running = false;
                break;
            }

            // NOTE: Input update/state copying should always be handled
            // after any input should be recorded; I.E. before this line.
            // As a safety, input is the last thing to be updated before
            // this frame ends.
            input_update(0);
        }
    }

    app_state->is_running = false;

    // Shutdown event system.
    event_unregister(EVENT_CODE_APPLICATION_QUIT, 0, application_on_event);
    event_unregister(EVENT_CODE_RESIZED, 0, application_on_event);
    event_unregister(EVENT_CODE_KEY_PRESSED, 0, application_on_key);
    event_unregister(EVENT_CODE_KEY_RELEASED, 0, application_on_key);

    event_shutdown();
    input_shutdown();

    renderer_shutdown();

    platform_shutdown(&app_state->platform);

    return true;
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
        case EVENT_CODE_RESIZED: {
            DINFO("EVENT_CODE_RESIZED recieved, resizing");
            u32 width = context.data.u32[0];
            u32 height = context.data.u32[1];
            if (width == 0 || height == 0)
            {
                DWARN("Recieved event resize of width: %d height: %d", width, height);
                return false;
            }
            renderer_on_resized(width, height);
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
