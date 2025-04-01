#include "core/input.h"
#include "core/dmemory.h"
#include "core/event.h"
#include "core/logger.h"

typedef struct keyboard_state
{
    b8 keys[256];
} keyboard_state;

typedef struct mouse_state
{
    s16 x;
    s16 y;
    u8  buttons[BUTTON_MAX_BUTTONS];
} mouse_state;

typedef struct input_state
{
    keyboard_state keyboard_current;
    keyboard_state keyboard_previous;
    mouse_state    mouse_current;
    mouse_state    mouse_previous;
} input_state;

// Internal input state
static input_state *input_state_ptr = {};

b8 input_system_initialize(u64 *input_state_mem_requirements, void *input_state)
{
    *input_state_mem_requirements = sizeof(struct input_state);
    if (input_state == 0)
    {
        return 0;
    }

    input_state_ptr = input_state;

    DINFO("Input subsystem Initalized.");
    return true;
}

void input_system_shutdown(void *state)
{
    // TODO: Add shutdown routines when needed.
    input_state_ptr = 0;
}

void input_update(f64 delta_time)
{
    if (!input_state_ptr)
    {
        return;
    }

    // Copy current input_state_ptr-> to previous input_state_ptr->.
    dcopy_memory(&input_state_ptr->keyboard_previous, &input_state_ptr->keyboard_current, sizeof(keyboard_state));
    dcopy_memory(&input_state_ptr->mouse_previous, &input_state_ptr->mouse_current, sizeof(mouse_state));
}

void input_process_key(keys key, b8 pressed)
{
    // Only handle this if the input_state_ptr->actually changed.
    if (!input_state_ptr)
    {
        return;
    }

    if (input_state_ptr->keyboard_current.keys[key] != pressed)
    {
        // Update internal input_state_ptr->
        input_state_ptr->keyboard_current.keys[key] = pressed;

        // Fire off an event for immediate processing.
        event_context context;
        context.data.u16[0] = key;
        event_fire(pressed ? EVENT_CODE_KEY_PRESSED : EVENT_CODE_KEY_RELEASED, 0, context);
    }
}

void input_process_button(buttons button, b8 pressed)
{
    if (!input_state_ptr)
    {
        return;
    }
    // If the input_state_ptr->changed, fire an event.
    if (input_state_ptr->mouse_current.buttons[button] != pressed)
    {
        input_state_ptr->mouse_current.buttons[button] = pressed;

        // Fire the event.
        event_context context;
        context.data.u16[0] = button;
        event_fire(pressed ? EVENT_CODE_BUTTON_PRESSED : EVENT_CODE_BUTTON_RELEASED, 0, context);
    }
}

void input_process_mouse_move(s16 x, s16 y)
{
    if (!input_state_ptr)
    {
        return;
    }
    if (input_state_ptr->mouse_current.x != x || input_state_ptr->mouse_current.y != y)
    {
        // NOTE: Enable this if debugging.
        // DDEBUG("Mouse pos: %i, %i!", x, y);

        // Update internal input_state_ptr->
        input_state_ptr->mouse_current.x = x;
        input_state_ptr->mouse_current.y = y;

        // Fire the event.
        event_context context;
        context.data.u16[0] = x;
        context.data.u16[1] = y;
        event_fire(EVENT_CODE_MOUSE_MOVED, 0, context);
    }
}

void input_process_mouse_wheel(s8 z_delta)
{
    // NOTE: no internal input_state_ptr->to update.

    // Fire the event.
    event_context context;
    context.data.u8[0] = z_delta;
    event_fire(EVENT_CODE_MOUSE_WHEEL, 0, context);
}

b8 input_is_key_down(keys key)
{
    if (!input_state_ptr)
    {
        return false;
    }
    return input_state_ptr->keyboard_current.keys[key] == true;
}

b8 input_is_key_up(keys key)
{
    if (!input_state_ptr)
    {
        return true;
    }
    return input_state_ptr->keyboard_current.keys[key] == false;
}

b8 input_was_key_down(keys key)
{
    if (!input_state_ptr)
    {
        return false;
    }
    return input_state_ptr->keyboard_previous.keys[key] == true;
}

b8 input_was_key_up(keys key)
{
    if (!input_state_ptr)
    {
        return true;
    }
    return input_state_ptr->keyboard_previous.keys[key] == false;
}

// mouse input
b8 input_is_button_down(buttons button)
{
    if (!input_state_ptr)
    {
        return false;
    }
    return input_state_ptr->mouse_current.buttons[button] == true;
}

b8 input_is_button_up(buttons button)
{
    if (!input_state_ptr)
    {
        return true;
    }
    return input_state_ptr->mouse_current.buttons[button] == false;
}

b8 input_was_button_down(buttons button)
{
    if (!input_state_ptr)
    {
        return false;
    }
    return input_state_ptr->mouse_previous.buttons[button] == true;
}

b8 input_was_button_up(buttons button)
{
    if (!input_state_ptr)
    {
        return true;
    }
    return input_state_ptr->mouse_previous.buttons[button] == false;
}

void input_get_mouse_position(s32 *x, s32 *y)
{
    if (!input_state_ptr)
    {
        *x = 0;
        *y = 0;
        return;
    }
    *x = input_state_ptr->mouse_current.x;
    *y = input_state_ptr->mouse_current.y;
}

void input_get_previous_mouse_position(s32 *x, s32 *y)
{
    if (!input_state_ptr)
    {
        *x = 0;
        *y = 0;
        return;
    }
    *x = input_state_ptr->mouse_previous.x;
    *y = input_state_ptr->mouse_previous.y;
}
