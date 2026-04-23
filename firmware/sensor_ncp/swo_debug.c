#include "swo_debug.h"

#include <stdarg.h>
#include <string.h>

#include <ti/drivers/ITM.h>
#include <ti/drivers/dpl/SystemP.h>

#define SWO_TRACE_PORT 0U
#define SWO_RESET_PORT 31U
#define SWO_RESET_FRAME 0xBBBBBBBBUL
#define SWO_SELF_TEST_TOKEN "SWO_SELF_TEST: CC1352P7_DIO16_ITM_CH0"
#define SWO_DEBUG_LINE_MAX 160U

static bool swoDebugInitialized = false;

static bool swoDebugWriteRaw(const char *data, size_t length)
{
    if ((!swoDebugInitialized) || (data == NULL) || (length == 0U))
    {
        return false;
    }

    ITM_sendBufferAtomic(SWO_TRACE_PORT, data, length);
    return true;
}

bool swoDebugInit(void)
{
    if (swoDebugInitialized)
    {
        return true;
    }

    if (false == ITM_open())
    {
        return false;
    }

    ITM_disableExceptionTrace();
    ITM_disablePCAndEventSampling();

    swoDebugInitialized = true;
    return true;
}

void swoDebugInitOrDie(void)
{
    if (!swoDebugInit())
    {
        /*
         * DIO_16 is reserved for the debug header SWO/TDO path on the custom
         * BDE-MB1352P71 carrier board.
         */
        while (1) {}
    }
}

bool swoDebugWrite(const char *data, size_t length)
{
    return swoDebugWriteRaw(data, length);
}

bool swoDebugWriteLine(const char *line)
{
    if (line == NULL)
    {
        return false;
    }

    if (!swoDebugWriteRaw(line, strlen(line)))
    {
        return false;
    }

    return swoDebugWriteRaw("\r\n", 2U);
}

bool swoDebugPrintf(const char *fmt, ...)
{
    va_list ap;
    int length;
    char line[SWO_DEBUG_LINE_MAX];

    if ((!swoDebugInitialized) || (fmt == NULL))
    {
        return false;
    }

    va_start(ap, fmt);
    length = SystemP_vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);

    if (length < 0)
    {
        return false;
    }

    if ((size_t)length >= sizeof(line))
    {
        length = (int)(sizeof(line) - 1U);
    }

    return swoDebugWriteLine(line);
}

void swoDebugBootSelfTest(const char *phase, const char *detail)
{
    const char *phaseText = ((phase != NULL) && (phase[0] != '\0')) ? phase : "BOOT";

    if (!swoDebugInit())
    {
        return;
    }

    /*
     * Host tooling waits for this parser reset token before consuming the
     * software stimulus stream.
     */
    ITM_send32Atomic(SWO_RESET_PORT, SWO_RESET_FRAME);

    if ((detail != NULL) && (detail[0] != '\0'))
    {
        (void)swoDebugPrintf("%s phase=%s %s", SWO_SELF_TEST_TOKEN, phaseText, detail);
    }
    else
    {
        (void)swoDebugPrintf("%s phase=%s", SWO_SELF_TEST_TOKEN, phaseText);
    }
}
