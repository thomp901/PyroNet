#ifndef UART_TRANSPORT_H
#define UART_TRANSPORT_H

typedef void (*UartTransportLogFn)(const char *line);

void uartTransportRun(UartTransportLogFn logFn);

#endif
