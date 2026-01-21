#ifndef KEY_SYNC_H
#define KEY_SYNC_H

#include <stdint.h>

// Initialize key synchronization module (creates uinput device)
// Returns 0 on success, -1 on failure
int key_sync_init(void);

// Synchronize keyboard state between hardware and software
// Injects release events for keys that software thinks are pressed
// but hardware reports as released
// Returns number of keys synchronized, -1 on failure
int key_sync_on_mode_switch(void);

// Cleanup key synchronization module
void key_sync_cleanup(void);

#endif // KEY_SYNC_H
