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

#include "app/app_host_events.h"
#include "app/app_provisioning.h"
#include "app/app_state.h"
#include "app/app_time_anchor.h"
#include "debug_console.h"
#include "platform/monotonic_time.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "transport/host_link.h"

typedef enum {
  APP_PHASE_WAIT_LINK = 0,
  APP_PHASE_RUN_PING,
  APP_PHASE_RUN_STATUS,
  APP_PHASE_IDLE,
} app_phase_t;

static app_phase_t app_phase = APP_PHASE_WAIT_LINK;

static app_context_t app_context;

static bool app_resolve_unix_time_s(void *context,
                                    int64_t monotonic_timestamp_ns,
                                    uint32_t *out_unix_time_s)
{
  const app_context_t *app = (const app_context_t *)context;
  if (app == NULL) {
    return false;
  }

  return app_time_anchor_resolve(&app->time_anchor,
                                 monotonic_timestamp_ns,
                                 out_unix_time_s);
}

app_context_t *app_context_get(void)
{
  return &app_context;
}

static void app_retry_ready_work(app_context_t *app)
{
  if (app == NULL) {
    return;
  }

  app_boundary_tx_flush(&app->boundary_tx,
                        app_provisioning_identity(),
                        &app->time_anchor);
  pyronet_risk_service_retry_pending(&app->risk_service);
}

void app_init_early(void)
{
  debug_console_init();
}

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/
void app_init(void)
{
  const app_registration_identity_t *identity = app_provisioning_identity();
  host_link_event_handlers_t handlers;

  monotonic_time_init();
  memset(&app_context, 0, sizeof(app_context));
  app_phase = APP_PHASE_WAIT_LINK;
  app_time_anchor_init(&app_context.time_anchor);
  app_boundary_tx_init(&app_context.boundary_tx);

  if (app_provisioning_boot_unix_time_valid()) {
    app_time_anchor_set(&app_context.time_anchor,
                        monotonic_time_now_ns(),
                        app_provisioning_boot_unix_time_s());
  }

  pyronet_risk_service_init(&app_context.risk_service,
                            monotonic_time_now_ns(),
                            &app_context,
                            app_resolve_unix_time_s);

  if (app_provisioning_identity_valid(identity)) {
    pyronet_risk_service_set_node_id(&app_context.risk_service,
                                     identity->node_id);
  }

  pyronet_risk_service_set_battery_pct(&app_context.risk_service,
                                       app_provisioning_battery_pct(identity));

  app_host_events_init_handlers(&handlers, &app_context);

  if (!host_link_init(&handlers)) {
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
  int64_t now_ns;

  host_link_poll();
  now_ns = monotonic_time_now_ns();
  pyronet_risk_service_tick(&app_context.risk_service, now_ns);

  if (!host_link_is_ready()) {
    if (app_phase != APP_PHASE_WAIT_LINK) {
      app_phase = APP_PHASE_WAIT_LINK;
    }
    return;
  }

  app_retry_ready_work(&app_context);

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
