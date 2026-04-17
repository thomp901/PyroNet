#ifndef DEBUG_CONSOLE_H
#define DEBUG_CONSOLE_H

#include <stdint.h>

#define SWO_SELF_TEST_TOKEN "SWO_SELF_TEST: FG28_PA03_ITM_CH0"

void debug_console_init(void);
const char *debug_console_backend_name(void);
uint32_t debug_console_swo_speed_hz(void);

#endif
