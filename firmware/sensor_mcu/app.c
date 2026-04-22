/***************************************************************************//**
 * @file
 * @brief Top level application functions
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#include "app.h"

#include "debug_console.h"
#include "platform/monotonic_time.h"
#include <stdbool.h>
#include <stdio.h>

#include "transport/host_link.h"

typedef enum {
  APP_PHASE_WAIT_LINK = 0,
  APP_PHASE_RUN_PING,
  APP_PHASE_RUN_STATUS,
  APP_PHASE_IDLE,
} app_phase_t;

static app_phase_t app_phase = APP_PHASE_WAIT_LINK;

void app_init_early(void)
{
  debug_console_init();
}

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/
void app_init(void)
{
  monotonic_time_init();
  app_phase = APP_PHASE_WAIT_LINK;

  if (!host_link_init()) {
    printf("HOST_INIT_FAILED\r\n");
  }

  debug_console_emit_boot_markers();
}

/***************************************************************************//**
 * App ticking function.
 ******************************************************************************/
void app_process_action(void)
{
  host_status_v1_t status;

  host_link_poll();

  if (!host_link_is_ready()) {
    if (app_phase != APP_PHASE_WAIT_LINK) {
      app_phase = APP_PHASE_WAIT_LINK;
    }
    return;
  }

  if (app_phase == APP_PHASE_WAIT_LINK) {
    app_phase = APP_PHASE_RUN_PING;
  }

  if (app_phase == APP_PHASE_RUN_PING) {
    if (!host_link_ping(0x12345678UL)) {
      return;
    }
    app_phase = APP_PHASE_RUN_STATUS;
  }

  if (app_phase == APP_PHASE_RUN_STATUS) {
    if (!host_link_get_status(&status)) {
      return;
    }

    printf("HOST_STATUS link_state=%u network_state=%u uptime_s=%lu\r\n",
           status.link_state,
           status.network_state,
           (unsigned long)status.uptime_s);
    app_phase = APP_PHASE_IDLE;
  }
}
