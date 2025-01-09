#include "containers/darray.h"
#include "core/asserts.h"
#include "core/dstring.h"
#include "core/event.h"
#include "core/input.h"
#include "core/logger.h"
#include "platform.h"

// Linux platform layer.
#ifdef DPLATFORM_LINUX

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#if _POSIX_C_SOURCE >= 199309L
#include <time.h> // nanosleep
#else
#include <unistd.h> // usleep
#endif

#if DPLATFORM_LINUX_X11

#include <X11/XKBlib.h>   // sudo apt-get install libx11-dev
#include <X11/Xlib-xcb.h> // sudo apt-get install libxkbcommon-x11-dev
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <xcb/xcb.h>

#define VK_USE_PLATFORM_XCB_KHR
#include "renderer/vulkan/vulkan_platform.h"
#include "renderer/vulkan/vulkan_types.inl"
#include <vulkan/vulkan.h>

typedef struct internal_state
{
    Display          *display;
    xcb_connection_t *connection;
    xcb_window_t      window;
    xcb_screen_t     *screen;
    xcb_atom_t        wm_protocols;
    xcb_atom_t        wm_delete_win;
} internal_state;

// Key translation
keys translate_keycode(u32 wl_keycode);

b8 platform_startup(platform_state *plat_state, const char *application_name, i32 x, i32 y, i32 width, i32 height)
{
    // Create the internal state.
    plat_state->internal_state = malloc(sizeof(internal_state));
    internal_state *state = (internal_state *)plat_state->internal_state;

    // Connect to X
    state->display = XOpenDisplay(NULL);

    // Turn off key repeats.
    XAutoRepeatOff(state->display);

    // Retrieve the connection from the display.
    state->connection = XGetXCBConnection(state->display);

    if (xcb_connection_has_error(state->connection))
    {
        DFATAL("Failed to connect to X server via XCB.");
        return false;
    }

    // Get data from the X server
    const struct xcb_setup_t *setup = xcb_get_setup(state->connection);

    // Loop through screens using iterator
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
    i32                   screen_p = 0;
    for (i32 s = screen_p; s > 0; s--)
    {
        xcb_screen_next(&it);
    }

    // After screens have been looped through, assign it.
    state->screen = it.data;

    // Allocate a XID for the window to be created.
    state->window = xcb_generate_id(state->connection);

    // Register event types.
    // XCB_CW_BACK_PIXEL = filling then window bg with a single colour
    // XCB_CW_EVENT_MASK is required.
    u32 event_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;

    // Listen for keyboard and mouse buttons
    u32 event_values = XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_KEY_PRESS |
                       XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_POINTER_MOTION |
                       XCB_EVENT_MASK_STRUCTURE_NOTIFY;

    // Values to be sent over XCB (bg colour, events)
    u32 value_list[] = {state->screen->black_pixel, event_values};

    // Create the window
    xcb_void_cookie_t cookie = xcb_create_window(state->connection,
                                                 XCB_COPY_FROM_PARENT, // depth
                                                 state->window,
                                                 state->screen->root,           // parent
                                                 x,                             // x
                                                 y,                             // y
                                                 width,                         // width
                                                 height,                        // height
                                                 0,                             // No border
                                                 XCB_WINDOW_CLASS_INPUT_OUTPUT, // class
                                                 state->screen->root_visual, event_mask, value_list);

    // Change the title
    xcb_change_property(state->connection, XCB_PROP_MODE_REPLACE, state->window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING,
                        8, // data should be viewed 8 bits at a time
                        strlen(application_name), application_name);

    // Tell the server to notify when the window manager
    // attempts to destroy the window.
    xcb_intern_atom_cookie_t wm_delete_cookie =
        xcb_intern_atom(state->connection, 0, strlen("WM_DELETE_WINDOW"), "WM_DELETE_WINDOW");
    xcb_intern_atom_cookie_t wm_protocols_cookie =
        xcb_intern_atom(state->connection, 0, strlen("WM_PROTOCOLS"), "WM_PROTOCOLS");
    xcb_intern_atom_reply_t *wm_delete_reply = xcb_intern_atom_reply(state->connection, wm_delete_cookie, NULL);
    xcb_intern_atom_reply_t *wm_protocols_reply = xcb_intern_atom_reply(state->connection, wm_protocols_cookie, NULL);
    state->wm_delete_win = wm_delete_reply->atom;
    state->wm_protocols = wm_protocols_reply->atom;

    xcb_change_property(state->connection, XCB_PROP_MODE_REPLACE, state->window, wm_protocols_reply->atom, 4, 32, 1,
                        &wm_delete_reply->atom);

    // Map the window to the screen
    xcb_map_window(state->connection, state->window);

    // Flush the stream
    i32 stream_result = xcb_flush(state->connection);
    if (stream_result <= 0)
    {
        DFATAL("An error occurred when flusing the stream: %d", stream_result);
        return false;
    }

    return true;
}

void platform_shutdown(platform_state *plat_state)
{
    // Simply cold-cast to the known type.
    internal_state *state = (internal_state *)plat_state->internal_state;

    // Turn key repeats back on since this is global for the OS... just... wow.
    XAutoRepeatOn(state->display);

    xcb_destroy_window(state->connection, state->window);
}

b8 platform_pump_messages(platform_state *plat_state)
{
    // Simply cold-cast to the known type.
    internal_state *state = (internal_state *)plat_state->internal_state;

    xcb_generic_event_t        *event;
    xcb_client_message_event_t *cm;

    b8 quit_flagged = false;

    // Poll for events until null is returned.
    while (event != 0)
    {
        event = xcb_poll_for_event(state->connection);
        if (event == 0)
        {
            break;
        }

        // Input events
        switch (event->response_type & ~0x80)
        {
            case XCB_KEY_PRESS:
            case XCB_KEY_RELEASE: {
                // Key press event - xcb_key_press_event_t and xcb_key_release_event_t are the same
                xcb_key_press_event_t *kb_event = (xcb_key_press_event_t *)event;
                b8                     pressed = event->response_type == XCB_KEY_PRESS;
                xcb_keycode_t          code = kb_event->detail;
                KeySym                 key_sym = XkbKeycodeToKeysym(state->display,
                                                                    (KeyCode)code, // event.xkey.keycode,
                                                                    0, code & ShiftMask ? 1 : 0);

                keys key = translate_keycode(key_sym);

                // Pass to the input subsystem for processing.
                input_process_key(key, pressed);
            }
            break;
            case XCB_BUTTON_PRESS:
            case XCB_BUTTON_RELEASE: {
                xcb_button_press_event_t *mouse_event = (xcb_button_press_event_t *)event;
                b8                        pressed = event->response_type == XCB_BUTTON_PRESS;
                buttons                   mouse_button = BUTTON_MAX_BUTTONS;
                switch (mouse_event->detail)
                {
                    case XCB_BUTTON_INDEX_1:
                        mouse_button = BUTTON_LEFT;
                        break;
                    case XCB_BUTTON_INDEX_2:
                        mouse_button = BUTTON_MIDDLE;
                        break;
                    case XCB_BUTTON_INDEX_3:
                        mouse_button = BUTTON_RIGHT;
                        break;
                }

                // Pass over to the input subsystem.
                if (mouse_button != BUTTON_MAX_BUTTONS)
                {
                    input_process_button(mouse_button, pressed);
                }
            }
            break;
            case XCB_MOTION_NOTIFY: {
                // Mouse move
                xcb_motion_notify_event_t *move_event = (xcb_motion_notify_event_t *)event;

                // Pass over to the input subsystem.
                input_process_mouse_move(move_event->event_x, move_event->event_y);
            }
            break;
            case XCB_CONFIGURE_NOTIFY: {
                // TODO: Resizing
            }
            break;

            case XCB_CLIENT_MESSAGE: {
                cm = (xcb_client_message_event_t *)event;

                // Window close
                if (cm->data.data32[0] == state->wm_delete_win)
                {
                    quit_flagged = true;
                }
            }
            break;
            default:
                // Something else
                break;
        }

        free(event);
    }
    return !quit_flagged;
}

void platform_get_required_extension_names(const char ***array)
{
    darray_push(*array, &"VK_KHR_xcb_surface");
}

b8 platform_create_vulkan_surface(platform_state *plat_state, struct vulkan_context *context)
{

    DTRACE("Creating X11 surface...");

    internal_state *state = (internal_state *)plat_state->internal_state;

    VkXcbSurfaceCreateInfoKHR surface_info = {};
    surface_info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    surface_info.pNext = 0;
    surface_info.flags = 0;
    surface_info.connection = state->connection;
    surface_info.window = state->window;

    VK_CHECK(vkCreateXcbSurfaceKHR(context->instance, &surface_info, 0, &context->surface));

    DINFO("Created x11 surface");

    return true;
}

// Key translation
keys translate_keycode(u32 wl_keycode)
{
    switch (wl_keycode)
    {
        case XK_BackSpace:
            return KEY_BACKSPACE;
        case XK_Return:
            return KEY_ENTER;
        case XK_Tab:
            return KEY_TAB;
            // case XK_Shift: return KEY_SHIFT;
            // case XK_Control: return KEY_CONTROL;

        case XK_Pause:
            return KEY_PAUSE;
        case XK_Caps_Lock:
            return KEY_CAPITAL;

        case XK_Escape:
            return KEY_ESCAPE;
            // Not supported
            // case : return KEY_CONVERT;
            // case : return KEY_NONCONVERT;
            // case : return KEY_ACCEPT;

        case XK_Mode_switch:
            return KEY_MODECHANGE;

        case XK_space:
            return KEY_SPACE;
        case XK_Prior:
            return KEY_PRIOR;
        case XK_Next:
            return KEY_NEXT;
        case XK_End:
            return KEY_END;
        case XK_Home:
            return KEY_HOME;
        case XK_Left:
            return KEY_LEFT;
        case XK_Up:
            return KEY_UP;
        case XK_Right:
            return KEY_RIGHT;
        case XK_Down:
            return KEY_DOWN;
        case XK_Select:
            return KEY_SELECT;
        case XK_Print:
            return KEY_PRINT;
        case XK_Execute:
            return KEY_EXECUTE;
        // case XK_snapshot: return KEY_SNAPSHOT; // not supported
        case XK_Insert:
            return KEY_INSERT;
        case XK_Delete:
            return KEY_DELETE;
        case XK_Help:
            return KEY_HELP;

        case XK_Meta_L:
            return KEY_LWIN; // TODO: not sure this is right
        case XK_Meta_R:
            return KEY_RWIN;
            // case XK_apps: return KEY_APPS; // not supported

            // case XK_sleep: return KEY_SLEEP; //not supported

        case XK_KP_0:
            return KEY_NUMPAD0;
        case XK_KP_1:
            return KEY_NUMPAD1;
        case XK_KP_2:
            return KEY_NUMPAD2;
        case XK_KP_3:
            return KEY_NUMPAD3;
        case XK_KP_4:
            return KEY_NUMPAD4;
        case XK_KP_5:
            return KEY_NUMPAD5;
        case XK_KP_6:
            return KEY_NUMPAD6;
        case XK_KP_7:
            return KEY_NUMPAD7;
        case XK_KP_8:
            return KEY_NUMPAD8;
        case XK_KP_9:
            return KEY_NUMPAD9;
        case XK_multiply:
            return KEY_MULTIPLY;
        case XK_KP_Add:
            return KEY_ADD;
        case XK_KP_Separator:
            return KEY_SEPARATOR;
        case XK_KP_Subtract:
            return KEY_SUBTRACT;
        case XK_KP_Decimal:
            return KEY_DECIMAL;
        case XK_KP_Divide:
            return KEY_DIVIDE;
        case XK_F1:
            return KEY_F1;
        case XK_F2:
            return KEY_F2;
        case XK_F3:
            return KEY_F3;
        case XK_F4:
            return KEY_F4;
        case XK_F5:
            return KEY_F5;
        case XK_F6:
            return KEY_F6;
        case XK_F7:
            return KEY_F7;
        case XK_F8:
            return KEY_F8;
        case XK_F9:
            return KEY_F9;
        case XK_F10:
            return KEY_F10;
        case XK_F11:
            return KEY_F11;
        case XK_F12:
            return KEY_F12;
        case XK_F13:
            return KEY_F13;
        case XK_F14:
            return KEY_F14;
        case XK_F15:
            return KEY_F15;
        case XK_F16:
            return KEY_F16;
        case XK_F17:
            return KEY_F17;
        case XK_F18:
            return KEY_F18;
        case XK_F19:
            return KEY_F19;
        case XK_F20:
            return KEY_F20;
        case XK_F21:
            return KEY_F21;
        case XK_F22:
            return KEY_F22;
        case XK_F23:
            return KEY_F23;
        case XK_F24:
            return KEY_F24;

        case XK_Num_Lock:
            return KEY_NUMLOCK;
        case XK_Scroll_Lock:
            return KEY_SCROLL;

        case XK_KP_Equal:
            return KEY_NUMPAD_EQUAL;

        case XK_Shift_L:
            return KEY_LSHIFT;
        case XK_Shift_R:
            return KEY_RSHIFT;
        case XK_Control_L:
            return KEY_LCONTROL;
        case XK_Control_R:
            return KEY_RCONTROL;
        // case XK_Menu: return KEY_LMENU;
        case XK_Menu:
            return KEY_RMENU;

        case XK_semicolon:
            return KEY_SEMICOLON;
        case XK_plus:
            return KEY_PLUS;
        case XK_comma:
            return KEY_COMMA;
        case XK_minus:
            return KEY_MINUS;
        case XK_period:
            return KEY_PERIOD;
        case XK_slash:
            return KEY_SLASH;
        case XK_grave:
            return KEY_GRAVE;

        case XK_a:
        case XK_A:
            return KEY_A;
        case XK_b:
        case XK_B:
            return KEY_B;
        case XK_c:
        case XK_C:
            return KEY_C;
        case XK_d:
        case XK_D:
            return KEY_D;
        case XK_e:
        case XK_E:
            return KEY_E;
        case XK_f:
        case XK_F:
            return KEY_F;
        case XK_g:
        case XK_G:
            return KEY_G;
        case XK_h:
        case XK_H:
            return KEY_H;
        case XK_i:
        case XK_I:
            return KEY_I;
        case XK_j:
        case XK_J:
            return KEY_J;
        case XK_k:
        case XK_K:
            return KEY_K;
        case XK_l:
        case XK_L:
            return KEY_L;
        case XK_m:
        case XK_M:
            return KEY_M;
        case XK_n:
        case XK_N:
            return KEY_N;
        case XK_o:
        case XK_O:
            return KEY_O;
        case XK_p:
        case XK_P:
            return KEY_P;
        case XK_q:
        case XK_Q:
            return KEY_Q;
        case XK_r:
        case XK_R:
            return KEY_R;
        case XK_s:
        case XK_S:
            return KEY_S;
        case XK_t:
        case XK_T:
            return KEY_T;
        case XK_u:
        case XK_U:
            return KEY_U;
        case XK_v:
        case XK_V:
            return KEY_V;
        case XK_w:
        case XK_W:
            return KEY_W;
        case XK_x:
        case XK_X:
            return KEY_X;
        case XK_y:
        case XK_Y:
            return KEY_Y;
        case XK_z:
        case XK_Z:
            return KEY_Z;

        default:
            return 0;
    }
}

#endif

#if DPLATFORM_LINUX_WAYLAND

#define CHECK_WL_RESULT(expr)                                                                                          \
    {                                                                                                                  \
        DASSERT(expr != 0);                                                                                            \
    }
//
#include "wayland/xdg-shell-client-protocol.h"
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

// TODO: this is temporary migrate the shared memory to use vulkan instead of mannually allocating buffers
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
//
// VULKAN
#define VK_USE_PLATFORM_WAYLAND_KHR
#include "renderer/vulkan/vulkan_platform.h"
#include "renderer/vulkan/vulkan_types.inl"
#include <vulkan/vulkan.h>

///* Shared memory support code */
// static void randname(char *buf)
//{
//     struct timespec ts;
//     clock_gettime(CLOCK_REALTIME, &ts);
//     long r = ts.tv_nsec;
//     for (int i = 0; i < 6; ++i)
//     {
//         buf[i] = 'A' + (r & 15) + (r & 16) * 2;
//         r >>= 5;
//     }
// }
//
// static int create_shm_file(void)
//{
//     int retries = 100;
//     do
//     {
//         char name[] = "/wl_shm-XXXXXX";
//         randname(name + sizeof(name) - 7);
//         --retries;
//         int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
//         if (fd >= 0)
//         {
//             shm_unlink(name);
//             return fd;
//         }
//     } while (retries > 0 && errno == EEXIST);
//     return -1;
// }
//
// static int allocate_shm_file(size_t size)
//{
//     int fd = create_shm_file();
//     if (fd < 0)
//         return -1;
//     int ret;
//     do
//     {
//         ret = ftruncate(fd, size);
//     } while (ret < 0 && errno == EINTR);
//     if (ret < 0)
//     {
//         close(fd);
//         return -1;
//     }
//     return fd;
// }

/* Wayland code */
typedef struct internal_state
{
    /* Globals */
    struct wl_display    *wl_display;
    struct wl_registry   *wl_registry;
    struct wl_shm        *wl_shm;
    struct wl_seat       *wl_seat;
    struct wl_compositor *wl_compositor;
    struct xdg_wm_base   *xdg_wm_base;

    /* Objects */
    struct wl_surface   *wl_surface;
    struct xdg_surface  *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;

    /* input */
    struct wl_keyboard *wl_keyboard;
    struct wl_mouse    *wl_mouse;

} internal_state;

u32 translate_keycode(u32 key);

// static void wl_buffer_release(void *data, struct wl_buffer *wl_buffer)
//{
//     /* Sent by the compositor when it's no longer using this buffer */
//     wl_buffer_destroy(wl_buffer);
// }
//
// static const struct wl_buffer_listener wl_buffer_listener = {
//     .release = wl_buffer_release,
// };
//
//  static struct wl_buffer *draw_frame(struct internal_state *state)
//{
//      const int width = 1200, height = 720;
//      int       stride = width * 4;
//      int       size = stride * height;
//
//      int fd = allocate_shm_file(size);
//      if (fd == -1)
//      {
//          return NULL;
//      }
//
//      u32 *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
//      if (data == MAP_FAILED)
//      {
//          close(fd);
//          return NULL;
//      }
//
//      struct wl_shm_pool *pool = wl_shm_create_pool(state->wl_shm, fd, size);
//      struct wl_buffer   *buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
//      wl_shm_pool_destroy(pool);
//      close(fd);
//
//      // black
//      for (int y = 0; y < height; ++y)
//      {
//          for (int x = 0; x < width; ++x)
//          {
//              data[y * width + x] = 0x00000000;
//          }
//      }
//
//      munmap(data, size);
//      wl_buffer_add_listener(buffer, &wl_buffer_listener, NULL);
//      return buffer;
//  }
//
//   seat

// keyboard

static void wl_keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard, u32 format, i32 fd, u32 size)
{
    // struct internal_state *state = data;
}

static void wl_keyboard_enter(void *data, struct wl_keyboard *wl_keyboard, u32 serial, struct wl_surface *surface,
                              struct wl_array *keys)
{
    DDEBUG("Keyboard in scope");
}

static void wl_keyboard_key(void *data, struct wl_keyboard *wl_keyboard, u32 serial, u32 time, u32 key, u32 state)
{
    // struct internal_state *internal_state = data;

    keys code = translate_keycode(key);

    input_process_key(code, (b8)state);
}

static void wl_keyboard_leave(void *data, struct wl_keyboard *wl_keyboard, u32 serial, struct wl_surface *surface)
{
    DDEBUG("Mouse not in scope");
}

static void wl_keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard, u32 serial, u32 mods_depressed,
                                  u32 mods_latched, u32 mods_locked, u32 group)
{
}

static void wl_keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard, i32 rate, i32 delay)
{
}

static const struct wl_keyboard_listener wl_keyboard_listener = {
    .keymap = wl_keyboard_keymap,
    .enter = wl_keyboard_enter,
    .leave = wl_keyboard_leave,
    .key = wl_keyboard_key,
    .modifiers = wl_keyboard_modifiers,
    .repeat_info = wl_keyboard_repeat_info,
};
//

static void wl_seat_capabilites(void *data, struct wl_seat *wl_seat, u32 capabilities)
{
    internal_state *state = data;

    // TODO: mouse events
    //

    b8 have_keyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;

    if (have_keyboard && state->wl_keyboard == NULL)
    {
        state->wl_keyboard = wl_seat_get_keyboard(state->wl_seat);
        wl_keyboard_add_listener(state->wl_keyboard, &wl_keyboard_listener, state);
    }
    else if (!have_keyboard && state->wl_keyboard != NULL)
    {
        wl_keyboard_release(state->wl_keyboard);
        state->wl_keyboard = NULL;
    }
}
static void wl_seat_name(void *data, struct wl_seat *wl_seat, const char *name)
{
}

struct wl_seat_listener wl_seat_listener = {.capabilities = wl_seat_capabilites, .name = wl_seat_name};

// actual surface
static void xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel, i32 width, i32 height,
                                   struct wl_array *states)
{
    if (width == 0 || height == 0)
    {
        return;
    }
    event_context context = {};
    context.data.u32[0] = width;
    context.data.u32[1] = height;
    event_fire(EVENT_CODE_RESIZED, 0, context);
}

static void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
}
struct xdg_toplevel_listener xdg_toplevel_listener = {.configure = xdg_toplevel_configure, .close = xdg_toplevel_close};
//

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, u32 serial)
{
    internal_state *state = (internal_state *)data;

    xdg_surface_ack_configure(xdg_surface, serial);
    wl_surface_commit(state->wl_surface);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

// shell
static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, u32 serial)
{
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};
//

static void registry_global(void *data, struct wl_registry *wl_registry, u32 name, const char *interface, u32 version)
{
    internal_state *state = data;

    if (string_compare(interface, wl_compositor_interface.name))
    {
        state->wl_compositor = wl_registry_bind(wl_registry, name, &wl_compositor_interface, 4);
    }
    else if (string_compare(interface, xdg_wm_base_interface.name))
    {
        state->xdg_wm_base = wl_registry_bind(wl_registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(state->xdg_wm_base, &xdg_wm_base_listener, state);
    }
    else if (string_compare(interface, wl_seat_interface.name))
    {
        state->wl_seat = wl_registry_bind(wl_registry, name, &wl_seat_interface, 1);
        wl_seat_add_listener(state->wl_seat, &wl_seat_listener, state);
    }
}

static void registry_global_remove(void *data, struct wl_registry *wl_registry, u32 name)
{
    /* This space deliberately left blank */
}

static const struct wl_registry_listener wl_registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

b8 platform_startup(platform_state *plat_state, const char *application_name, i32 x, i32 y, i32 width, i32 height)
{
    DINFO("Initializing linux-Wayland platform...");
    plat_state->internal_state = malloc(sizeof(internal_state));
    internal_state *state = (internal_state *)plat_state->internal_state;

    state->wl_display = wl_display_connect(NULL);
    if (!state->wl_display)
    {
        return false;
    }

    state->wl_registry = wl_display_get_registry(state->wl_display);
    if (!state->wl_registry)
    {
        return false;
    }
    wl_registry_add_listener(state->wl_registry, &wl_registry_listener, state);

    wl_display_roundtrip(state->wl_display);

    state->wl_surface = wl_compositor_create_surface(state->wl_compositor);
    if (!state->wl_surface)
    {
        return false;
    }

    state->xdg_surface = xdg_wm_base_get_xdg_surface(state->xdg_wm_base, state->wl_surface);
    if (!state->xdg_surface)
    {
        return false;
    }

    xdg_surface_add_listener(state->xdg_surface, &xdg_surface_listener, state);

    state->xdg_toplevel = xdg_surface_get_toplevel(state->xdg_surface);

    if (!state->xdg_toplevel)
    {
        return false;
    }
    xdg_toplevel_add_listener(state->xdg_toplevel, &xdg_toplevel_listener, state);

    xdg_toplevel_set_title(state->xdg_toplevel, application_name);
    xdg_toplevel_set_app_id(state->xdg_toplevel, application_name);

    wl_surface_commit(state->wl_surface);

    DINFO("Linux-Wayland platform initialized");

    return true;
}

b8 platform_pump_messages(platform_state *plat_state)
{
    internal_state *state = (internal_state *)plat_state->internal_state;

    i32 result = wl_display_dispatch(state->wl_display);

    return result == -1 ? false : true;
}

void platform_shutdown(platform_state *plat_state)
{
    internal_state *state = (internal_state *)plat_state->internal_state;

    xdg_toplevel_destroy(state->xdg_toplevel);
    xdg_surface_destroy(state->xdg_surface);
    wl_surface_destroy(state->wl_surface);
    wl_display_disconnect(state->wl_display);
}

void platform_get_required_extension_names(const char ***names_darray)
{
    char *surface_name = VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
    darray_push(*names_darray, surface_name);
}

b8 platform_create_vulkan_surface(platform_state *plat_state, vulkan_context *context)
{
    internal_state *state = (internal_state *)plat_state->internal_state;

    VkWaylandSurfaceCreateInfoKHR create_surface_info = {};
    create_surface_info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    create_surface_info.pNext = 0;
    create_surface_info.flags = 0;
    create_surface_info.display = state->wl_display;
    create_surface_info.surface = state->wl_surface;

    VK_CHECK(vkCreateWaylandSurfaceKHR(context->instance, &create_surface_info, 0, &context->surface));

    return true;
}

u32 translate_keycode(u32 wl_keycode)
{
    switch (wl_keycode)
    {
        case 1: {
            return KEY_ESCAPE;
        }
        case 2: {
            return 0;
            // return KEY_NUMPAD1;
        }
        case 3: {
            return 0;
            // return KEY_NUMPAD2;
        }
        case 4: {
            return 0;
            // return KEY_NUMPAD3;
        }
        case 5: {
            return 0;
            // return KEY_NUMPAD4;
        }
        case 6: {
            return 0;
            // return KEY_NUMPAD5;
        }
        case 7: {
            return 0;
            // return KEY_NUMPAD6;
        }
        case 8: {
            return 0;
            // return KEY_NUMPAD7;
        }
        case 9: {
            return 0;
            // return KEY_NUMPAD8;
        }
        case 10: {
            return 0;
            // return KEY_NUMPAD9;
        }
        case 11: {
            return 0;
            // return KEY_NUMPAD0;
        }
        case 12: {
            return 0;
        }
        case 13: {
            return 0;
        }
        case 14: {
            return 0;
        }
        case 15: {
            return 0;
        }
        case 16: {
            return KEY_Q;
        }
        case 17: {
            return KEY_W;
        }
        case 18: {
            return KEY_E;
        }
        case 19: {
            return KEY_R;
        }
        case 20: {
            return KEY_T;
        }
        case 21: {
            return KEY_Y;
        }
        case 22: {
            return KEY_U;
        }
        case 23: {
            return KEY_I;
        }
        case 24: {
            return KEY_O;
        }
        case 25: {
            return KEY_P;
        }
        case 26: {
            return 0;
        }
        case 27: {
            return 0;
        }
        case 28: {
            return 0;
        }
        case 29: {
            return 0;
        }
        case 30: {
            return KEY_A;
        }
        case 31: {
            return KEY_S;
        }
        case 32: {
            return KEY_D;
        }
        case 33: {
            return KEY_F;
        }
        case 34: {
            return KEY_G;
        }
        case 35: {
            return KEY_H;
        }
        case 36: {
            return KEY_J;
        }
        case 37: {
            return KEY_K;
        }
        case 38: {
            return KEY_L;
        }
        case 39: {
            return 0;
        }
        case 40: {
            return 0;
        }
        case 41: {
            return 0;
        }
        case 42: {
            return 0;
        }
        case 43: {
            return 0;
        }
        case 44: {
            return KEY_Z;
        }
        case 45: {
            return KEY_X;
        }
        case 46: {
            return KEY_C;
        }
        case 47: {
            return KEY_V;
        }
        case 48: {
            return KEY_B;
        }
        case 49: {
            return KEY_N;
        }
        case 50: {
            return KEY_M;
        }
        case 51: {
            return 0;
        }
        case 52: {
            return 0;
        }
        case 53: {
            return 0;
        }
        case 54: {
            return 0;
        }
        case 55: {
            return 0;
        }
        case 56: {
            return 0;
        }
        case 57: {
            return 0;
        }
        case 58: {
            return 0;
        }
        case 59: {
            return 0;
        }
        case 60: {
            return 0;
        }
        case 61: {
            return 0;
        }
        case 62: {
            return 0;
        }
        case 63: {
            return 0;
        }
        case 64: {
            return 0;
        }
        case 65: {
            return 0;
        }
        case 66: {
            return 0;
        }
        case 67: {
            return 0;
        }
        case 68: {
            return 0;
        }
        case 69: {
            return 0;
        }
        case 70: {
            return 0;
        }
        case 71: {
            return 0;
        }
        case 72: {
            return 0;
        }
        case 73: {
            return 0;
        }
        case 74: {
            return 0;
        }
        case 75: {
            return 0;
        }
        case 76: {
            return 0;
        }
        case 77: {
            return 0;
        }
        case 78: {
            return 0;
        }
        case 79: {
            return 0;
        }
        case 80: {
            return 0;
        }
        case 81: {
            return 0;
        }
        case 82: {
            return 0;
        }
        case 83: {
            return 0;
        }
        case 84: {
            return 0;
        }
        case 85: {
            return 0;
        }
        case 86: {
            return 0;
        }
        case 87: {
            return 0;
        }
        case 88: {
            return 0;
        }
        case 89: {
            return 0;
        }
        case 90: {
            return 0;
        }
        case 91: {
            return 0;
        }
        case 92: {
            return 0;
        }
        case 93: {
            return 0;
        }
        case 94: {
            return 0;
        }
        case 95: {
            return 0;
        }
        case 96: {
            return 0;
        }
        case 97: {
            return 0;
        }
        case 98: {
            return 0;
        }
        case 99: {
            return 0;
        }
        case 100: {
            return 0;
        }
        case 101: {
            return 0;
        }
        case 102: {
            return 0;
        }
        defualt:
            return 0;
    }
    return 0;
}

#endif

void *platform_allocate(u64 size, b8 aligned)
{
    return malloc(size);
}
void platform_free(void *block, b8 aligned)
{
    free(block);
}
void *platform_zero_memory(void *block, u64 size)
{
    return memset(block, 0, size);
}
void *platform_copy_memory(void *dest, const void *source, u64 size)
{
    return memcpy(dest, source, size);
}
void *platform_set_memory(void *dest, i32 value, u64 size)
{
    return memset(dest, value, size);
}

void platform_console_write(const char *message, u8 colour)
{
    // FATAL,ERROR,WARN,INFO,DDEBUG,TRACE
    const char *colour_strings[] = {"0;41", "1;31", "1;33", "1;32", "1;34", "1;30"};
    printf("\033[%sm%s\033[0m", colour_strings[colour], message);
}
void platform_console_write_error(const char *message, u8 colour)
{
    // FATAL,ERROR,WARN,INFO,DEBUG,TRACE
    const char *colour_strings[] = {"0;41", "1;31", "1;33", "1;32", "1;34", "1;30"};
    printf("\033[%sm%s\033[0m", colour_strings[colour], message);
}

f64 platform_get_absolute_time()
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec + now.tv_nsec * 0.000000001;
}

void platform_sleep(u64 ms)
{
#if _POSIX_C_SOURCE >= 199309L
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000 * 1000;
    nanosleep(&ts, 0);
#else
    if (ms >= 1000)
    {
        sleep(ms / 1000);
    }
    usleep((ms % 1000) * 1000);
#endif
}

#endif
