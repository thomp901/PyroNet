#ifndef TRANSPORT_HOST_UART_H
#define TRANSPORT_HOST_UART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HOST_UART_BAUDRATE 115200U

bool host_uart_init(void);
bool host_uart_try_read_byte(uint8_t *out_byte);
void host_uart_write(const uint8_t *data, size_t length);

#endif
