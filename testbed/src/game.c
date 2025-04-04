#include "game.h"
#include "core/dmemory.h"
#include "math/dmath.h"

#include <core/event.h>
#include <core/logger.h>

// HACK: this should not be availbale to the game for some reason that im not sure now. Future me should know and future me if you dont know are you slacking off again?? 01/04/2025
#include <renderer/renderer_frontend.h>
#include <renderer/renderer_types.h>
//

#include <core/input.h>

void recalculate_view_matrix(game_state *state)
{
    if (state->camera_view_dirty)
    {
        mat4 rotation    = mat4_euler_xyz(state->camera_euler.x, state->camera_euler.y, state->camera_euler.z);
        mat4 translation = mat4_translation(state->camera_position);

        state->view = mat4_mul(rotation, translation);
        state->view = mat4_inverse(state->view);

        state->camera_view_dirty = false;
    }
}

void camera_pitch(game_state *state, f32 amount_to_pitch)
{
    state->camera_euler.x += amount_to_pitch;
    state->camera_view_dirty = true;
}

void camera_yaw(game_state *state, f32 amount_to_yaw)
{
    state->camera_euler.y += amount_to_yaw;
    state->camera_view_dirty = true;
}

void camera_roll(game_state *state, f32 amount_to_roll)
{
    state->camera_euler.z += amount_to_roll;

    f32 limit             = deg_to_rad(89.0f);
    state->camera_euler.x = DCLAMP(state->camera_euler.x, -limit, limit);

    state->camera_view_dirty = true;
}

b8 game_initialize(game *game_inst)
{
    game_state *state = (game_state *)game_inst->state;

    state->camera_position = (vec3){0.0f, 0.0f, 30.0f};
    state->camera_euler    = vec3_zero();

    state->view              = mat4_translation(state->camera_position);
    state->view              = mat4_inverse(state->view);
    state->camera_view_dirty = true;

    DDEBUG("game_initialize() called!");
    return true;
}

void game_shutdown(game *game_inst)
{
    DDEBUG("Shutting down game.");
    dfree(game_inst->state, sizeof(game_state), MEMORY_TAG_GAME);
}

b8 game_update(game *game_inst, f32 delta_time)
{
    game_state *state              = (game_state *)game_inst->state;
    static u64  alloc_count        = 0;
    f32         camera_sensitivity = 1.0f;

    u64 prev_alloc_count = alloc_count;
    alloc_count          = get_memory_alloc_count();

    if (input_is_key_up('M') && input_was_key_down('M'))
    {
        DDEBUG("Allocations: %llu (%llu this frame)", alloc_count, alloc_count - prev_alloc_count);
    }

    // HACK: hack for dirty camera

    // INFO: left and right
    if (input_is_key_down('A'))
    {
        camera_yaw(state, camera_sensitivity * delta_time);
    }
    if (input_was_key_down('D'))
    {
        camera_yaw(state, -camera_sensitivity * delta_time);
    }

    if (input_was_key_down('T') && input_is_key_up('T'))
    {
        event_context context = {};
        event_fire(EVENT_CODE_DEBUG0, 0, context);
        DDEBUG("Swapping textures.");
    }

    // INFO: up and down
    if (input_is_key_down(KEY_UP))
    {
        camera_pitch(state, camera_sensitivity * delta_time);
    }
    if (input_is_key_down(KEY_DOWN))
    {
        camera_pitch(state, -camera_sensitivity * delta_time);
    }

    f32  temp_move_speed = 10.0f;
    vec3 velocity        = vec3_zero();

    if (input_is_key_down('W'))
    {
        vec3 forward = mat4_forward(state->view);
        velocity     = vec3_add(velocity, forward);
    }
    if (input_is_key_down('S'))
    {
        vec3 backward = mat4_backward(state->view);
        velocity      = vec3_add(velocity, backward);
    }

    if (input_is_key_down('Q'))
    {
        vec3 left = mat4_left(state->view);
        velocity  = vec3_add(velocity, left);
    }
    if (input_is_key_down('E'))
    {
        vec3 right = mat4_right(state->view);
        velocity   = vec3_add(velocity, right);
    }

    if (input_is_key_down(KEY_SPACE))
    {
        velocity.y += 1.0f;
    }
    if (input_is_key_down('X'))
    {
        velocity.y -= 1.0f;
    }

    vec3 zero = vec3_zero();

    if (!vec3_compare(velocity, zero, 0.0002f))
    {
        vec3_normalize(&velocity);
        state->camera_position.x += velocity.x * temp_move_speed * delta_time;
        state->camera_position.y += velocity.y * temp_move_speed * delta_time;
        state->camera_position.z += velocity.z * temp_move_speed * delta_time;

        state->camera_view_dirty = true;
    }

    recalculate_view_matrix(state);

    // HACK: this should not be available outside the engine
    renderer_set_view(state->view);

    return true;
}

b8 game_render(game *game_inst, f32 delta_time)
{
    render_packet packet = {delta_time};
    b8            result = renderer_draw_frame(&packet);
    return result;
}

void game_on_resize(game *game_inst, u32 width, u32 height)
{
}
