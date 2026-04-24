#ifndef DEBUG_CONSOLE_H
#define DEBUG_CONSOLE_H

#include <stdbool.h>
#include <stdint.h>

#define DEBUG_CONSOLE_BACKEND_NAME         "SWO"
#define DEBUG_CONSOLE_BOOT_BANNER_MARKER   "SWO_BOOT"
#define DEBUG_CONSOLE_BOOT_READY_MARKER    "SWO_BOOT_READY"
#define DEBUG_CONSOLE_ITM_PORT             0U
#define DEBUG_CONSOLE_SYNC_ITM_PORT        8U
#define DEBUG_CONSOLE_TARGET_SWO_SPEED_HZ  875000U

void debug_console_init(void);
bool debug_console_is_initialized(void);
const char *debug_console_backend_name(void);
uint32_t debug_console_swo_speed_hz(void);
void debug_console_emit_boot_markers(void);
void debug_console_panic_write(const char *text);

#endif
