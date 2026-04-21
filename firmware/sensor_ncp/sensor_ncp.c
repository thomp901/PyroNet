/*
 * Copyright (c) 2015-2019, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== sensor_ncp.c ========
 */

/* For usleep() */
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/ITM.h>
// #include <ti/drivers/I2C.h>
// #include <ti/drivers/SPI.h>
// #include <ti/drivers/Watchdog.h>

/* Driver configuration */
#include "ti_drivers_config.h"

/*
 * Use the raw GPIO index so this stays tied to the custom-board DIO mapping
 * instead of LaunchPad aliases.
 */
#define TEST_PULSE_DIO 28U
#define SWO_TRACE_PORT 0U
#define SWO_RESET_PORT 31U
#define SWO_RESET_FRAME 0xBBBBBBBBUL
#define SWO_LINE_BUFFER_SIZE 96U
#define SWO_SELF_TEST_TOKEN "SWO_SELF_TEST: CC1352P7_DIO16_ITM_CH0"

static void swoWriteLine(const char *line)
{
    ITM_sendBufferAtomic(SWO_TRACE_PORT, line, strlen(line));
    ITM_send8Atomic(SWO_TRACE_PORT, '\r');
    ITM_send8Atomic(SWO_TRACE_PORT, '\n');
}

static void swoInit(void)
{
    if (false == ITM_open())
    {
        /* DIO_16 is reserved for the debug header SWO/TDO path. */
        while (1) {}
    }

    ITM_disableExceptionTrace();
    ITM_disablePCAndEventSampling();

    /*
     * Host tooling waits for this parser reset token before consuming the
     * software stimulus stream.
     */
    ITM_send32Atomic(SWO_RESET_PORT, SWO_RESET_FRAME);
    swoWriteLine(SWO_SELF_TEST_TOKEN " phase=BOOT pulse_dio=28");
}

/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0)
{
    const uint32_t halfPeriodUs = 500000;
    bool pulseLevel = false;
    uint32_t sequence = 0;
    char swoLine[SWO_LINE_BUFFER_SIZE];

    (void)arg0;

    /* Call driver init functions */
    GPIO_init();
    swoInit();
    // I2C_init();
    // SPI_init();
    // Watchdog_init();

    /* Drive DIO_28 as a push-pull output for the pulse test. */
    GPIO_setConfig(TEST_PULSE_DIO, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_write(TEST_PULSE_DIO, 0);

    while (1)
    {
        usleep(halfPeriodUs);
        GPIO_toggle(TEST_PULSE_DIO);
        pulseLevel = !pulseLevel;

        sequence++;
        snprintf(swoLine,
                 sizeof(swoLine),
                 SWO_SELF_TEST_TOKEN " phase=HEARTBEAT seq=%lu level=%u",
                 (unsigned long)sequence,
                 pulseLevel ? 1U : 0U);
        swoWriteLine(swoLine);
    }
}
