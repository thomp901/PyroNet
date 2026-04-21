#ifndef BUS_BRINGUP_H
#define BUS_BRINGUP_H

#include <stdint.h>

#define BOARD_I2C_BITRATE_HZ 100000U
#define BOARD_UART_BAUDRATE 115200U

void board_i2c_init(void);
void board_uart_init(void);
void board_uart_write(const char *text);

#endif
