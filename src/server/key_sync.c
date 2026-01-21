#include "key_sync.h"
#include "input_capture.h"
#include "keyboard_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <linux/input.h>
#include <linux/uinput.h>

static int uinput_fd = -1;

int key_sync_init(void) {
    // Open uinput device
    uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (uinput_fd < 0) {
        perror("Failed to open /dev/uinput");
        return -1;
    }

    // Enable key events
    if (ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY) < 0) {
        perror("Failed to enable EV_KEY");
        close(uinput_fd);
        uinput_fd = -1;
        return -1;
    }

    // Enable all key codes we might need to inject
    for (int i = 0; i < 256; i++) {
        ioctl(uinput_fd, UI_SET_KEYBIT, i);
    }

    // Setup uinput device
    struct uinput_setup usetup = {0};
    snprintf(usetup.name, UINPUT_MAX_NAME_SIZE, "OneKM Key Sync Device");
    usetup.id.bustype = BUS_VIRTUAL;
    usetup.id.vendor = 0x1234;
    usetup.id.product = 0x5678;
    usetup.id.version = 1;

    if (ioctl(uinput_fd, UI_DEV_SETUP, &usetup) < 0) {
        perror("Failed to setup uinput device");
        close(uinput_fd);
        uinput_fd = -1;
        return -1;
    }

    // Create the device
    if (ioctl(uinput_fd, UI_DEV_CREATE) < 0) {
        perror("Failed to create uinput device");
        close(uinput_fd);
        uinput_fd = -1;
        return -1;
    }

    printf("Key sync module initialized with uinput device\n");
    return 0;
}

static void emit_key_event(int fd, uint16_t keycode, int value) {
    struct input_event ev = {0};
    
    // Key event
    ev.type = EV_KEY;
    ev.code = keycode;
    ev.value = value;
    gettimeofday(&ev.time, NULL);
    
    if (write(fd, &ev, sizeof(ev)) < 0) {
        perror("Failed to write key event");
        return;
    }
    
    // Sync event
    ev.type = EV_SYN;
    ev.code = SYN_REPORT;
    ev.value = 0;
    gettimeofday(&ev.time, NULL);
    
    if (write(fd, &ev, sizeof(ev)) < 0) {
        perror("Failed to write sync event");
    }
}

int key_sync_on_mode_switch(void) {
    if (uinput_fd < 0) {
        fprintf(stderr, "Key sync module not initialized\n");
        return -1;
    }

    // Get hardware keyboard state
    uint8_t hw_key_states[32] = {0};
    if (get_hardware_keyboard_state(hw_key_states) < 0) {
        fprintf(stderr, "Failed to get hardware keyboard state\n");
        return -1;
    }

    int keys_synced = 0;

    // Check all possible Linux keycodes (0-255)
    for (int keycode = 0; keycode < 256; keycode++) {
        // Check if software thinks this key is pressed
        int sw_pressed = keyboard_state_is_key_pressed(keycode);
        
        // Check if hardware reports this key as pressed
        int hw_pressed = (hw_key_states[keycode / 8] >> (keycode % 8)) & 1;

        // If software thinks key is pressed but hardware says it's not,
        // we need to inject a release event
        if (sw_pressed && !hw_pressed) {
            printf("[KEY_SYNC] Injecting release for keycode %d (0x%02X)\n", 
                   keycode, keycode);
            
            // Inject key release event
            emit_key_event(uinput_fd, keycode, 0);
            keys_synced++;
            
            // Small delay to ensure event is processed
            usleep(1000);
        }
    }

    if (keys_synced > 0) {
        printf("[KEY_SYNC] Synchronized %d key(s)\n", keys_synced);
    } else {
        printf("[KEY_SYNC] No keys needed synchronization\n");
    }

    return keys_synced;
}

void key_sync_cleanup(void) {
    if (uinput_fd >= 0) {
        ioctl(uinput_fd, UI_DEV_DESTROY);
        close(uinput_fd);
        uinput_fd = -1;
        printf("Key sync module cleaned up\n");
    }
}
