#ifndef INPUT_CAPTURE_H
#define INPUT_CAPTURE_H

#include <stdint.h>

typedef struct {
    uint16_t type;
    uint16_t code;
    int32_t value;
} InputEvent;

int init_input_capture(void);
int capture_input(InputEvent *event);
int get_device_fds(int *fds, int max_fds);
void set_device_grab(int grab);
void cleanup_input_capture(void);

// Get hardware keyboard state (which keys are physically pressed)
// key_states: array of 256 bits (32 bytes) representing key states
// Returns 0 on success, -1 on failure
int get_hardware_keyboard_state(uint8_t key_states[32]);

#endif // INPUT_CAPTURE_H