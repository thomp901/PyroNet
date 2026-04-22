#ifndef HOST_HOST_UART_H
#define HOST_HOST_UART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HOST_UART_BAUD_RATE 115200U

bool host_uart_init(void);
bool host_uart_read_byte(uint8_t *out_byte);
bool host_uart_write(const uint8_t *data, size_t length);

#endif /* HOST_HOST_UART_H */
