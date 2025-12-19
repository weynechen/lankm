#include "input_inject.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>

static INPUT input_buffer[2];
static int input_count = 0;

// External logging function from main.c
extern void log_message(const char *format, ...);

// Screen dimensions for absolute positioning
static int screen_width = 0;
static int screen_height = 0;
static int current_x = 0;
static int current_y = 0;

// Helper function to get screen dimensions
void update_screen_dimensions(void) {
    screen_width = GetSystemMetrics(SM_CXSCREEN);
    screen_height = GetSystemMetrics(SM_CYSCREEN);

    // Initialize cursor position to center if not set
    if (current_x == 0 && current_y == 0) {
        current_x = screen_width / 2;
        current_y = screen_height / 2;
    }
}

// Helper function to set absolute mouse position
void set_mouse_position_absolute(int x, int y) {
    // Normalize to 0-65535 range for absolute coordinates
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dx = (x * 65535) / screen_width;
    input.mi.dy = (y * 65535) / screen_height;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    input.mi.mouseData = 0;
    input.mi.time = 0;
    input.mi.dwExtraInfo = 0;

    SendInput(1, &input, sizeof(INPUT));
}

int init_input_inject(void) {
    // Initialize input buffer
    memset(input_buffer, 0, sizeof(input_buffer));
    input_count = 0;

    // No need for complex initialization with relative positioning
    return 0;
}

void inject_mouse_move(int dx, int dy) {
    // Use relative positioning for better performance
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dx = dx;
    input.mi.dy = dy;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;  // Use relative move
    input.mi.mouseData = 0;
    input.mi.time = 0;
    input.mi.dwExtraInfo = 0;

    UINT result = SendInput(1, &input, sizeof(INPUT));
    if (result != 1) {
        DWORD error = GetLastError();
        fprintf(stderr, "SendInput failed for mouse move: %lu (error %lu)\n", result, error);
    }
}

void inject_mouse_button(uint8_t button, uint8_t state) {
    // Simple approach: just send the button event directly
    // The cursor position is controlled by inject_mouse_move which updates continuously

    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dx = 0;
    input.mi.dy = 0;
    input.mi.mouseData = 0;
    input.mi.time = 0;
    input.mi.dwExtraInfo = 0;

    if (state == 1) {
        // Button DOWN
        switch (button) {
            case 1: input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN; break;
            case 2: input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN; break;
            case 3: input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN; break;
        }
    } else {
        // Button UP
        switch (button) {
            case 1: input.mi.dwFlags = MOUSEEVENTF_LEFTUP; break;
            case 2: input.mi.dwFlags = MOUSEEVENTF_RIGHTUP; break;
            case 3: input.mi.dwFlags = MOUSEEVENTF_MIDDLEUP; break;
        }
    }

    UINT result = SendInput(1, &input, sizeof(INPUT));
    if (result != 1) {
        // Log error if SendInput failed
        DWORD error = GetLastError();
        fprintf(stderr, "SendInput failed for mouse button: %lu (error %lu)\n", result, error);
    }
}

void inject_key_event(uint16_t vk_code, uint8_t state) {
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk_code;
    input.ki.wScan = 0;
    input.ki.dwFlags = (state == 1) ? 0 : KEYEVENTF_KEYUP;
    input.ki.time = 0;
    input.ki.dwExtraInfo = 0;

    // Handle special keys that need extended flag
    switch (vk_code) {
        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_MENU:
        case VK_LMENU:
        case VK_RMENU:
        case VK_INSERT:
        case VK_DELETE:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_NUMLOCK:
        case VK_CANCEL:
        case VK_SNAPSHOT:
        case VK_DIVIDE:
            input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
            break;
    }

    UINT result = SendInput(1, &input, sizeof(INPUT));
    if (result != 1) {
        DWORD error = GetLastError();
        fprintf(stderr, "SendInput failed for key event (vk=%d, state=%d): %lu (error %lu)\n", vk_code, state, result, error);
    }
}

void cleanup_input_inject(void) {
    // Release any potentially stuck modifier keys
    // This helps prevent sticky key issues if program exits abnormally

    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.dwFlags = KEYEVENTF_KEYUP;

    // Release common modifier keys that might be stuck
    input.ki.wVk = VK_MENU;        // Alt
    SendInput(1, &input, sizeof(INPUT));

    input.ki.wVk = VK_CONTROL;     // Ctrl
    SendInput(1, &input, sizeof(INPUT));

    input.ki.wVk = VK_SHIFT;       // Shift
    SendInput(1, &input, sizeof(INPUT));

    input.ki.wVk = VK_LWIN;        // Windows key
    SendInput(1, &input, sizeof(INPUT));
}