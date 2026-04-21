#include <string.h>

#include <ti/drivers/ITM.h>

#include "trace_port.h"

#define TRACE_PORT_STIMULUS 0U
#define TRACE_PORT_RESET_STIMULUS 31U
#define TRACE_PORT_RESET_FRAME 0xBBBBBBBBUL

bool tracePortInit(void)
{
    if (false == ITM_open())
    {
        return false;
    }

    ITM_disableExceptionTrace();
    ITM_disablePCAndEventSampling();

    /*
     * Host tooling waits for this parser reset token before consuming the
     * software stimulus stream.
     */
    ITM_send32Atomic(TRACE_PORT_RESET_STIMULUS, TRACE_PORT_RESET_FRAME);

    return true;
}

void tracePortWriteLine(const char *line)
{
    ITM_sendBufferAtomic(TRACE_PORT_STIMULUS, line, strlen(line));
    ITM_send8Atomic(TRACE_PORT_STIMULUS, '\r');
    ITM_send8Atomic(TRACE_PORT_STIMULUS, '\n');
}
