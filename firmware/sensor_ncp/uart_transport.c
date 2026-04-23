#include "host/host_link.h"
#include "pyronet_ncp.h"
#include "uart_transport.h"

void uartTransportRun(void)
{
    (void)pyronet_ncp_init();
    host_link_init();

    while (1)
    {
        pyronet_ncp_poll();
        host_link_poll();
    }
}
