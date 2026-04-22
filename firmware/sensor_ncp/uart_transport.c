#include "host/host_link.h"
#include "uart_transport.h"

void uartTransportRun(void)
{
    host_link_init();

    while (1)
    {
        host_link_poll();
    }
}
