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
#include "app/app_sensor_runtime.h"
#include "app/app_state.h"
#include "app/app_time_anchor.h"
#include "debug_console.h"
#include "platform/monotonic_time.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "transport/host_link.h"

// Debug packet schedule after boot.
#define APP_TIMED_REPORT_DELAY_NS   (10LL * 1000000000LL)
#define APP_TIMED_ALERT_DELAY_NS    (20LL * 1000000000LL)

typedef enum {
  APP_PHASE_WAIT_LINK = 0,
  APP_PHASE_RUN_PING,
  APP_PHASE_RUN_STATUS,
  APP_PHASE_IDLE,
} app_phase_t;

static app_phase_t app_phase = APP_PHASE_WAIT_LINK;

static app_context_t app_context;
static int64_t app_startup_ns;
static bool app_timed_report_sent;
static bool app_timed_alert_sent;

static int16_t app_scale_float_signed(float value, float scale)
{
  float scaled_value = value * scale;

  if (scaled_value >= 0.0f) {
    scaled_value += 0.5f;
  } else {
    scaled_value -= 0.5f;
  }

  if (scaled_value > 32767.0f) {
    return 32767;
  }

  if (scaled_value < -32768.0f) {
    return -32768;
  }

  return (int16_t)scaled_value;
}

static uint16_t app_scale_float_unsigned(float value, float scale)
{
  float scaled_value = value * scale;

  if (scaled_value <= 0.0f) {
    return 0U;
  }

  scaled_value += 0.5f;
  if (scaled_value > 65535.0f) {
    return 65535U;
  }

  return (uint16_t)scaled_value;
}

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

static uint32_t app_resolve_packet_timestamp_s(int64_t monotonic_timestamp_ns)
{
  uint32_t timestamp_s = 0U;

  if (app_time_anchor_resolve(&app_context.time_anchor,
                              monotonic_timestamp_ns,
                              &timestamp_s)) {
    return timestamp_s;
  }

  if (monotonic_timestamp_ns <= app_startup_ns) {
    return 0U;
  }

  return (uint32_t)((uint64_t)(monotonic_timestamp_ns - app_startup_ns) / 1000000000ULL);
}

static bool app_build_timed_sensor_payload(int64_t now_ns,
                                           pyronet_host_send_sensor_report_v1_t *payload)
{
  const app_registration_identity_t *identity = app_provisioning_identity();
  pyronet_risk_snapshot_t snapshot;

  if ((payload == NULL) || !app_provisioning_identity_valid(identity)) {
    return false;
  }

  if (!pyronet_risk_service_current_snapshot(&app_context.risk_service, &snapshot)) {
    return false;
  }

  payload->node_id = identity->node_id;
  payload->timestamp = app_resolve_packet_timestamp_s(now_ns);
  payload->risk_level = (uint8_t)snapshot.current_level;
  payload->temperature_c_x100 = app_scale_float_signed(snapshot.temperature_c, 100.0f);
  payload->humidity_pct_x100 = app_scale_float_unsigned(snapshot.humidity_percent, 100.0f);
  payload->bvoc_ppm = app_scale_float_unsigned(snapshot.voc, 1.0f);
  payload->pm25_ug_m3_x10 = app_scale_float_unsigned(snapshot.pm25_ug_m3, 10.0f);
  payload->battery_pct = app_provisioning_battery_pct(identity);
  return true;
}

static bool app_try_send_timed_sensor_report(int64_t now_ns)
{
  pyronet_host_send_sensor_report_v1_t payload;

  if (!app_build_timed_sensor_payload(now_ns, &payload)) {
    return false;
  }

  if (!host_link_send_sensor_report(&payload)) {
    return false;
  }

  printf("TIMED_PACKET_SENT type=0x%02X timestamp=%lu uptime_s=%lu\r\n",
         PYRONET_PKT_SENSOR_REPORT,
         (unsigned long)payload.timestamp,
         (unsigned long)((uint64_t)(now_ns - app_startup_ns) / 1000000000ULL));
  return true;
}

static bool app_try_send_timed_sensor_alert(int64_t now_ns)
{
  pyronet_host_send_sensor_alert_v1_t payload;

  if (!app_build_timed_sensor_payload(now_ns, &payload)) {
    return false;
  }

  if (!host_link_send_sensor_alert(&payload)) {
    return false;
  }

  printf("TIMED_PACKET_SENT type=0x%02X timestamp=%lu uptime_s=%lu\r\n",
         PYRONET_PKT_SENSOR_ALERT,
         (unsigned long)payload.timestamp,
         (unsigned long)((uint64_t)(now_ns - app_startup_ns) / 1000000000ULL));
  return true;
}

static void app_process_timed_packets(int64_t now_ns)
{
  if (!host_link_is_ready()) {
    return;
  }

  if (!app_timed_report_sent && ((now_ns - app_startup_ns) >= APP_TIMED_REPORT_DELAY_NS)) {
    app_timed_report_sent = app_try_send_timed_sensor_report(now_ns);
  }

  if (!app_timed_alert_sent && ((now_ns - app_startup_ns) >= APP_TIMED_ALERT_DELAY_NS)) {
    app_timed_alert_sent = app_try_send_timed_sensor_alert(now_ns);
  }
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
  pyronet_risk_service_sensor_ops_t sensor_ops = {
    .context = &app_context.sensors,
    .request_pm25_sample = app_sensor_runtime_request_pm25_sample,
    .set_pm25_schedule = app_sensor_runtime_set_pm25_schedule,
  };

  monotonic_time_init();
  memset(&app_context, 0, sizeof(app_context));
  app_phase = APP_PHASE_WAIT_LINK;
  app_startup_ns = monotonic_time_now_ns();
  app_timed_report_sent = false;
  app_timed_alert_sent = false;
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
                            app_resolve_unix_time_s,
                            &sensor_ops);

  if (app_provisioning_identity_valid(identity)) {
    pyronet_risk_service_set_node_id(&app_context.risk_service,
                                     identity->node_id);
  }

  pyronet_risk_service_set_battery_pct(&app_context.risk_service,
                                       app_provisioning_battery_pct(identity));
  app_sensor_runtime_init(&app_context.sensors, &app_context.risk_service);

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
  app_sensor_runtime_process(&app_context.sensors);
  now_ns = monotonic_time_now_ns();
  pyronet_risk_service_tick(&app_context.risk_service, now_ns);
  app_process_timed_packets(now_ns);

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
