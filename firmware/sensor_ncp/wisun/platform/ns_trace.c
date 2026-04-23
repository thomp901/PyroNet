/******************************************************************************
 * Local ns_trace override for the custom BDE-MB1352P71 carrier board.
 *
 * The TI Wi-SUN example routes non-NCP SWO to a LaunchPad pin that does not
 * exist on this hardware. This copy keeps the TI trace implementation but
 * forces SWO onto DIO_16, which is wired to the XDS110 SWO/TDO pin on the
 * custom board.
 *****************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <semaphore.h>

#include <ti/drivers/dpl/SystemP.h>

#include <ioc.h>

#include "itm_private.h"
#include "mbed-client-libservice/ip6string.h"
#include "ns_trace.h"
#include "ti_radio_config.h"

#define ITM_STIM_PORT_8(x) (*(volatile uint8_t *)ITM_STIM_PORT((x)))

#define ITM_PORT 0

#define VT100_COLOR_ERROR "\x1b[31m"
#define VT100_COLOR_WARN  "\x1b[33m"
#define VT100_COLOR_INFO  "\x1b[39m"
#define VT100_COLOR_DEBUG "\x1b[90m"
#define VT100_RESET_TERM  "\x1b[0m\n\r"

#define DEFAULT_TRACE_TMP_LINE_LEN 128
#define TRACE_SYSTEM_CLOCK_HZ      48000000U
#define TRACE_BAUD_RATE            3000000U

typedef enum
{
    ITM_TS_DIV_NONE = 0,
    ITM_TS_DIV_4 = 1,
    ITM_TS_DIV_16 = 2,
    ITM_TS_DIV_64 = 3
} ITM_tsPrescale;

typedef enum
{
    ITM_SYNC_NONE = 0,
    ITM_SYNC_16M_CYCLES = 1,
    ITM_SYNC_64M_CYCLES = 2,
    ITM_SYNC_256M_CYCLES = 3
} ITM_syncPacketRate;

char tmpStr[DEFAULT_TRACE_TMP_LINE_LEN];

static sem_t ns_trace_mutex_handle;
static char ns_buf[256];

void ns_trace_init(void)
{
    int retc;

    retc = sem_init(&ns_trace_mutex_handle, 0, 1);
    if (retc != 0)
    {
        while (1) {}
    }

    SCS_DEMCR &= (~SCS_DEMCR_TRCEN);
    ITM_TCR = 0x00000000;

    SCS_DEMCR |= SCS_DEMCR_TRCEN;

    TPIU_LAR = CS_LAR_UNLOCK;
    TPIU_SPPR = TPIU_SPPR_SWO_UART;
    TPIU_CSPSR = TPIU_CSPSR_PIN_1;

    ITM_LAR = CS_LAR_UNLOCK;
    ITM_TER = ITM_TER_ENABLE_ALL;
    ITM_TPR = ITM_TPR_ENABLE_USER_ALL;

    {
        uint32_t prescalar = TRACE_SYSTEM_CLOCK_HZ / TRACE_BAUD_RATE;
        uint32_t diff1 = TRACE_SYSTEM_CLOCK_HZ - (prescalar * TRACE_BAUD_RATE);
        uint32_t diff2 = ((prescalar + 1U) * TRACE_BAUD_RATE) - TRACE_SYSTEM_CLOCK_HZ;

        if (diff2 < diff1)
        {
            prescalar++;
        }

        TPIU_ACPR = (prescalar - 1U);
    }

    TPIU_FFCR = 0;
    DWT_LAR = CS_LAR_UNLOCK;

    /*
     * Custom carrier-board routing:
     * XDS110 SWO/TDO -> module JTAG_TDO -> DIO_16
     */
    IOCPortConfigureSet(IOID_16, IOC_PORT_MCU_SWV, IOC_STD_OUTPUT);

    ns_enable_module();
}

void ns_trace_printf(uint8_t dlevel, const char *grp, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    ns_trace_vprintf(dlevel, grp, fmt, ap);
    va_end(ap);
}

void ns_trace_vprintf(uint8_t dlevel, const char *grp, const char *fmt, va_list ap)
{
    int len_written = 0;
    int total_len = 0;
    int remaining_len;
    char *pBuf;

    sem_wait(&ns_trace_mutex_handle);

    pBuf = ns_buf;

    switch (dlevel)
    {
        case TRACE_LEVEL_ERROR:
            len_written = SystemP_snprintf(ns_buf, sizeof(ns_buf), "%s[ERR ][%-4s]: ", VT100_COLOR_ERROR, grp);
            break;
        case TRACE_LEVEL_WARN:
            len_written = SystemP_snprintf(ns_buf, sizeof(ns_buf), "%s[WARN][%-4s]: ", VT100_COLOR_WARN, grp);
            break;
        case TRACE_LEVEL_INFO:
            len_written = SystemP_snprintf(ns_buf, sizeof(ns_buf), "%s[INFO][%-4s]: ", VT100_COLOR_INFO, grp);
            break;
        case TRACE_LEVEL_DEBUG:
            len_written = SystemP_snprintf(ns_buf, sizeof(ns_buf), "%s[DBG ][%-4s]: ", VT100_COLOR_DEBUG, grp);
            break;
        default:
            len_written = SystemP_snprintf(ns_buf, sizeof(ns_buf), "                ");
            break;
    }

    total_len += len_written;
    remaining_len = (int)sizeof(ns_buf) - total_len;

    pBuf += len_written;
    len_written = SystemP_vsnprintf(pBuf, remaining_len, fmt, ap);
    if (len_written > (remaining_len - (int)sizeof(VT100_RESET_TERM)))
    {
        len_written = remaining_len - (int)sizeof(VT100_RESET_TERM);
    }

    total_len += len_written;
    remaining_len = (int)sizeof(ns_buf) - total_len;

    pBuf += len_written;
    len_written = SystemP_snprintf(pBuf, remaining_len, VT100_RESET_TERM);
    total_len += len_written;

    for (int x = 0; x < total_len; x++)
    {
        ns_put_char_blocking(ns_buf[x]);
    }

    sem_post(&ns_trace_mutex_handle);
}

void ns_enable_module(void)
{
    ITM_TCR |= ITM_TCR_ENABLE_ITM;
}

void ns_disable_module(void)
{
    ITM_TCR &= ~ITM_TCR_ENABLE_ITM;
}

void ns_put_char_blocking(const char ch)
{
    while (0 == ITM_STIM_PORT_8(ITM_PORT)) {}
    ITM_STIM_PORT_8(ITM_PORT) = ch;
}

void ns_enable_exception_trace(void)
{
    DWT_CTRL |= DWT_CTRL_ENABLE_EXC_TRC;
    ITM_TCR |= ITM_TCR_ENABLE_DWT_TX;
}

bool ns_enable_data_trace(const uint32_t *variable)
{
    uint_least8_t numDwtComp = (DWT_CTRL & DWT_CTRL_MASK_NUM_COMP) >> DWT_CTRL_SHIFT_NUM_COMP;
    bool dwtAvailable = false;

    for (uint_least8_t dwtIndex = 0; dwtIndex < numDwtComp; dwtIndex++)
    {
        if (0 == DWT_FUNC(dwtIndex))
        {
            DWT_COMP(dwtIndex) = (uint32_t)variable;
            DWT_MASK(dwtIndex) = 0x0;
            DWT_FUNC(dwtIndex) = (DWT_FUNC_DATA_SIZE_32 | DWT_FUNC_ENABLE_ADDR_OFFSET | DWT_FUNC_ENABLE_COMP_RW);
            dwtAvailable = true;
        }
    }

    return dwtAvailable;
}

void ns_enable_cycle_counter(void)
{
    DWT_CTRL &= ~(DWT_CTRL_ENABLE_PC_SAMP | DWT_CTRL_ENABLE_CYC_EVT);
    DWT_CTRL |= DWT_CTRL_CYC_CNT_1024;
    ITM_TCR |= ITM_TCR_ENABLE_DWT_TX;
    DWT_CTRL |= DWT_CTRL_ENABLE_CYC_CNT;
    DWT_CTRL |= DWT_CTRL_ENABLE_CYC_EVT;
}

static void ns_enable_timing(ITM_tsPrescale tsPrescale)
{
    ITM_TCR |= ((tsPrescale << ITM_TCR_TS_PRESCALE_SHIFT) & ITM_TCR_TS_PRESCALE_MASK);
    ITM_TCR |= ITM_TCR_ENABLE_TS;
}

static void ns_enable_sync_packets(ITM_syncPacketRate syncPacketRate)
{
    DWT_CTRL &= ~(DWT_CTRL_MASK_SYNCTAP);
    DWT_CTRL |= ((syncPacketRate << DWT_CTRL_SHIFT_SYNCTAP) & DWT_CTRL_MASK_SYNCTAP);
    DWT_CTRL |= DWT_CTRL_ENABLE_CYC_CNT;
    ITM_TCR |= ITM_TCR_ENABLE_SYNC;
}

void ns_flush_module(void)
{
    while (ITM_TCR & ITM_TCR_BUSY)
    {
        asm(" NOP");
    }
}

char *ns_trace_ipv6(const void *addr_ptr)
{
    if (addr_ptr == NULL)
    {
        return "<null>";
    }

    tmpStr[0] = 0;
    ip6tos(addr_ptr, tmpStr);
    return tmpStr;
}

char *ns_trace_ipv6_prefix(const uint8_t *prefix, uint8_t prefix_len)
{
    if ((prefix_len != 0U && prefix == NULL) || prefix_len > 128U)
    {
        return "<err>";
    }

    ip6_prefix_tos(prefix, prefix_len, tmpStr);
    return tmpStr;
}

char *ns_trace_array(const uint8_t *buf, uint16_t len)
{
    if (len == 0U)
    {
        return "";
    }
    if (buf == NULL)
    {
        return "<null>";
    }

    const uint8_t *ptr = buf;
    char *pOutput = tmpStr;
    char overflow = 0;

    memset(pOutput, 0x0, DEFAULT_TRACE_TMP_LINE_LEN);

    for (int i = 0; i < len; i++)
    {
        int retval = SystemP_snprintf(pOutput, DEFAULT_TRACE_TMP_LINE_LEN, "%02x:", *ptr++);
        if (retval <= 0 || retval > DEFAULT_TRACE_TMP_LINE_LEN)
        {
            overflow = 1;
            break;
        }
        pOutput += retval;
    }

    tmpStr[DEFAULT_TRACE_TMP_LINE_LEN - 1] = overflow ? '*' : 0;
    return tmpStr;
}

char *ns_trace_array16(const uint16_t *buf, uint16_t len)
{
    if (len == 0U)
    {
        return "";
    }
    if (buf == NULL)
    {
        return "<null>";
    }

    const uint16_t *ptr = buf;
    char *pOutput = tmpStr;
    char overflow = 0;

    memset(pOutput, 0x0, DEFAULT_TRACE_TMP_LINE_LEN);

    for (int i = 0; i < len; i++)
    {
        int retval = SystemP_snprintf(pOutput, DEFAULT_TRACE_TMP_LINE_LEN, "%04x: ", *ptr++);
        if (retval <= 0 || retval > DEFAULT_TRACE_TMP_LINE_LEN)
        {
            overflow = 1;
            break;
        }
        pOutput += retval;
    }

    tmpStr[DEFAULT_TRACE_TMP_LINE_LEN - 1] = overflow ? '*' : 0;
    return tmpStr;
}
