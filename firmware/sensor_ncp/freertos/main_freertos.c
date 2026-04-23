/*
 * Copyright (c) 2016-2020, Texas Instruments Incorporated
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
 *  ======== main_freertos.c ========
 */
#include <stdint.h>

#ifdef __ICCARM__
    #include <DLib_Threads.h>
#endif

/* POSIX Header files */
#include <pthread.h>

/* RTOS header files */
#include <FreeRTOS.h>
#include <task.h>

#include <ti/drivers/Board.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>

#include "mesh_system.h"
#include "ns_trace.h"

#ifdef NV_RESTORE
#include "macconfig.h"
#include "nvocmp.h"
#else
#include "nvintf.h"
#endif

#ifdef FEATURE_TIMAC_SUPPORT
#include "macTask.h"

#ifndef USE_DEFAULT_USER_CFG
#include "mac_user_config.h"
macUserCfg_t macUser0Cfg[] = MAC_USER_CFG;
#else
extern macUserCfg_t macUser0Cfg[];
#endif

static uint8_t timacTaskId;
extern void startRfCbThread(void);

#define MAIN_ASSERT_MAC 3
#endif

#ifdef WISUN_RCP_ENABLE
#include "rcp_host.h"
#endif

extern void *mainThread(void *arg0);

#ifdef NV_RESTORE
mac_Config_t Main_user1Cfg = {0};
#endif

NVINTF_nvFuncts_t *pNV = NULL;

/* Stack size in bytes */
#define THREADSTACKSIZE 2048

#ifdef FEATURE_TIMAC_SUPPORT
void Main_assertHandler(uint8_t assertReason)
{
    (void)assertReason;
    taskDISABLE_INTERRUPTS();

    while (1) {}
}

void assertHandler(void)
{
    Main_assertHandler(MAIN_ASSERT_MAC);
}
#endif

/*
 *  ======== main ========
 */
int main(void)
{
    pthread_t thread;
    pthread_attr_t attrs;
    struct sched_param priParam;
    int retc;

    /* initialize the system locks */
#ifdef __ICCARM__
    __iar_Initlocks();
#endif

    Power_setConstraint(PowerCC26XX_IDLE_PD_DISALLOW);
    Power_setConstraint(PowerCC26XX_SB_DISALLOW);

    Board_init();

#ifdef FEATURE_TIMAC_SUPPORT
    macUser0Cfg[0].pAssertFP = assertHandler;
    timacTaskId = macTaskInit(macUser0Cfg);
#endif

#ifdef WISUN_RCP_ENABLE
    rcp_init();
#endif

    /* Initialize the attributes structure with default values */
    pthread_attr_init(&attrs);

    /* Set priority, detach state, and stack size attributes */
    priParam.sched_priority = 1;
    retc                    = pthread_attr_setschedparam(&attrs, &priParam);
    retc |= pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED);
    retc |= pthread_attr_setstacksize(&attrs, THREADSTACKSIZE);
    if (retc != 0)
    {
        /* failed to set attributes */
        while (1) {}
    }

    retc = pthread_create(&thread, &attrs, mainThread, NULL);
    if (retc != 0)
    {
        /* pthread_create() failed */
        while (1) {}
    }

    ns_trace_init();
    mesh_system_init();

#ifdef NV_RESTORE
    NVOCMP_loadApiPtrs(&Main_user1Cfg.nvFps);
    if (Main_user1Cfg.nvFps.initNV != NULL)
    {
        Main_user1Cfg.nvFps.initNV(NULL);
    }
    pNV = &Main_user1Cfg.nvFps;
#endif

#ifdef FEATURE_TIMAC_SUPPORT
    startRfCbThread();
#endif

    /* Start the FreeRTOS scheduler */
    vTaskStartScheduler();

    return (0);
}
