#ifdef DPLATFORM_WINDOWS
#include "platform/platform.h"

#include "containers/darray.h"
#include "core/event.h"
#include "core/input.h"
#include "core/logger.h"

#include <stdlib.h>
#include <windows.h>
#include <windowsx.h> // param input extraction

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#include "renderer/vulkan/vulkan_platform.h"
#include "renderer/vulkan/vulkan_types.h"

typedef struct platform_state
{
    HINSTANCE h_instance;
    HWND      hwnd;

    u32 width;
    u32 height;
} platform_state;

// Clock
static f64           clock_frequency;
static LARGE_INTEGER start_time;

static platform_state *platform_state_ptr;

LRESULT CALLBACK win32_process_message(HWND hwnd, u32 msg, WPARAM w_param, LPARAM l_param);

b8 platform_startup(u64 *platform_mem_requirements, void *plat_state, const char *application_name, s32 x, s32 y, s32 width, s32 height)
{
    *platform_mem_requirements = sizeof(platform_state);
    if (plat_state == 0)
    {
        return true;
    }
    platform_state_ptr = plat_state;

    platform_state_ptr->h_instance = GetModuleHandleA(0);

    // Setup and register window class.
    HICON     icon = LoadIcon(platform_state_ptr->h_instance, IDI_APPLICATION);
    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.style         = CS_DBLCLKS; // Get double-clicks
    wc.lpfnWndProc   = win32_process_message;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = platform_state_ptr->h_instance;
    wc.hIcon         = icon;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW); // NULL; // Manage the cursor manually
    wc.hbrBackground = NULL;                        // Transparent
    wc.lpszClassName = "kohi_window_class";

    if (!RegisterClassA(&wc))
    {
        MessageBoxA(0, "Window registration failed", "Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    // Create window
    u32 client_x      = x;
    u32 client_y      = y;
    u32 client_width  = width;
    u32 client_height = height;

    u32 window_x      = client_x;
    u32 window_y      = client_y;
    u32 window_width  = client_width;
    u32 window_height = client_height;

    u32 window_style    = WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION;
    u32 window_ex_style = WS_EX_APPWINDOW;

    window_style |= WS_MAXIMIZEBOX;
    window_style |= WS_MINIMIZEBOX;
    window_style |= WS_THICKFRAME;

    // Obtain the size of the border.
    RECT border_rect = {0, 0, 0, 0};
    AdjustWindowRectEx(&border_rect, window_style, 0, window_ex_style);

    // In this case, the border rectangle is negative.
    window_x += border_rect.left;
    window_y += border_rect.top;

    // Grow by the size of the OS border.
    window_width += border_rect.right - border_rect.left;
    window_height += border_rect.bottom - border_rect.top;

    HWND handle = CreateWindowExA(window_ex_style, "kohi_window_class", application_name, window_style, window_x, window_y, window_width, window_height, 0, 0, platform_state_ptr->h_instance, 0);

    if (handle == 0)
    {
        MessageBoxA(NULL, "Window creation failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);

        DFATAL("Window creation failed!");
        return false;
    }
    else
    {
        platform_state_ptr->hwnd = handle;
    }

    // Show the window
    b32 should_activate           = 1; // TODO: if the window should not accept input, this should be false.
    s32 show_window_command_flags = should_activate ? SW_SHOW : SW_SHOWNOACTIVATE;
    // If initially minimized, use SW_MINIMIZE : SW_SHOWMINNOACTIVE;
    // If initially maximized, use SW_SHOWMAXIMIZED : SW_MAXIMIZE
    ShowWindow(platform_state_ptr->hwnd, show_window_command_flags);

    // Clock setup
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    clock_frequency = 1.0 / (f64)frequency.QuadPart;
    QueryPerformanceCounter(&start_time);

    return true;
}

void platform_shutdown()
{
    // Simply cold-cast to the known type.

    if (platform_state_ptr->hwnd)
    {
        DestroyWindow(platform_state_ptr->hwnd);
        platform_state_ptr->hwnd = 0;
    }
}

b8 platform_pump_messages()
{
    MSG message;
    while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    return true;
}

void platform_get_required_extension_names(const char ***names_darray)
{
    darray_push(*names_darray, &"VK_KHR_win32_surface");
}

b8 platform_create_vulkan_surface(struct vulkan_context *context)
{
    if (!platform_state_ptr)
    {
        return true;
    }
    DTRACE("Creating win32 surface...");

    VkWin32SurfaceCreateInfoKHR surface_info = {};

    surface_info.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_info.pNext     = 0;
    surface_info.flags     = 0;
    surface_info.hinstance = platform_state_ptr->h_instance;
    surface_info.hwnd      = platform_state_ptr->hwnd;

    VK_CHECK(vkCreateWin32SurfaceKHR(context->instance, &surface_info, 0, &context->surface));

    DINFO("Created win32 surface");

    return true;
}

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

void *platform_set_memory(void *dest, s32 value, u64 size)
{
    return memset(dest, value, size);
}

void platform_console_write(const char *message, u8 colour)
{
    HANDLE console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    // FATAL,ERROR,WARN,INFO,DEBUG,TRACE
    static u8 levels[6] = {64, 4, 6, 2, 1, 8};
    SetConsoleTextAttribute(console_handle, levels[colour]);
    OutputDebugStringA(message);
    u64     length         = strlen(message);
    LPDWORD number_written = 0;
    WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), message, (DWORD)length, number_written, 0);
}

void platform_console_write_error(const char *message, u8 colour)
{
    HANDLE console_handle = GetStdHandle(STD_ERROR_HANDLE);
    // FATAL,ERROR,WARN,INFO,DEBUG,TRACE
    static u8 levels[6] = {64, 4, 6, 2, 1, 8};
    SetConsoleTextAttribute(console_handle, levels[colour]);
    OutputDebugStringA(message);
    u64     length         = strlen(message);
    LPDWORD number_written = 0;
    WriteConsoleA(GetStdHandle(STD_ERROR_HANDLE), message, (DWORD)length, number_written, 0);
}

f64 platform_get_absolute_time()
{
    LARGE_INTEGER now_time;
    QueryPerformanceCounter(&now_time);
    return (f64)now_time.QuadPart * clock_frequency;
}

void platform_sleep(u64 ms)
{
    Sleep(ms);
}

void platform_get_window_dimensions(u32 *width, u32 *height)
{
    if (platform_state_ptr)
    {
        *width  = platform_state_ptr->width;
        *height = platform_state_ptr->height;
    }
}

LRESULT CALLBACK win32_process_message(HWND hwnd, u32 msg, WPARAM w_param, LPARAM l_param)
{
    switch (msg)
    {
        case WM_ERASEBKGND:
            // Notify the OS that erasing will be handled by the application to prevent flicker.
            return 1;
        case WM_CLOSE: {
            event_context context = {};
            event_fire(EVENT_CODE_APPLICATION_QUIT, 0, context);
            return 0;
        }
        break;
        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
        break;
        case WM_SIZE: {
            // Get the updated size.
            RECT          r;
            event_context context = {};

            GetClientRect(hwnd, &r);
            u32 width  = r.right - r.left;
            u32 height = r.bottom - r.top;

            context.data.u32[0] = width;
            context.data.u32[1] = height;

            platform_state_ptr->width  = width;
            platform_state_ptr->height = height;

            event_fire(EVENT_CODE_RESIZED, 0, context);
        }
        break;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP: {
            // Key pressed/released
            b8   pressed = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
            keys key     = (u16)w_param;

            // Pass to the input subsystem for processing.
            input_process_key(key, pressed);
        }
        break;
        case WM_MOUSEMOVE: {
            // Mouse move
            s32 x_position = GET_X_LPARAM(l_param);
            s32 y_position = GET_Y_LPARAM(l_param);

            // Pass over to the input subsystem.
            input_process_mouse_move(x_position, y_position);
        }
        break;
        case WM_MOUSEWHEEL: {
            s32 z_delta = GET_WHEEL_DELTA_WPARAM(w_param);
            if (z_delta != 0)
            {
                // Flatten the input to an OS-independent (-1, 1)
                z_delta = (z_delta < 0) ? -1 : 1;
                input_process_mouse_wheel(z_delta);
            }
        }
        break;
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP: {
            b8      pressed      = msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN;
            buttons mouse_button = BUTTON_MAX_BUTTONS;
            switch (msg)
            {
                case WM_LBUTTONDOWN:
                case WM_LBUTTONUP:
                    mouse_button = BUTTON_LEFT;
                    break;
                case WM_MBUTTONDOWN:
                case WM_MBUTTONUP:
                    mouse_button = BUTTON_MIDDLE;
                    break;
                case WM_RBUTTONDOWN:
                case WM_RBUTTONUP:
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
    }

    return DefWindowProcA(hwnd, msg, w_param, l_param);
}

#endif // DPLATFORM_WINDOWS
