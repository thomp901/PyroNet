#include "app/app_host_events.h"

#include <stdio.h>

#include "app/app_provisioning.h"
#include "platform/monotonic_time.h"

static float app_decode_fixed_x100_i16(int16_t value)
{
  return ((float)value) / 100.0f;
}

static float app_decode_fixed_x100_u16(uint16_t value)
{
  return ((float)value) / 100.0f;
}

static float app_decode_fixed_x10_u16(uint16_t value)
{
  return ((float)value) / 10.0f;
}

static pyronet_risk_config_t app_translate_config_update(
  const app_context_t *app,
  const pyronet_host_config_update_received_v1_t *event)
{
  const pyronet_risk_config_t *current_config;
  pyronet_risk_config_t config;

  current_config = (app != NULL)
                     ? pyronet_risk_service_config(&app->risk_service)
                     : NULL;
  config = (current_config != NULL) ? *current_config : pyronet_risk_config_default();

  if (event == NULL) {
    return config;
  }

  /*
   * CONFIG_UPDATE_RX carries packet-schema fixed-point threshold fields, so
   * convert them back into real risk-engine units before applying the config.
   */
  config.config_id = event->config_id;
  config.l2_temp_c_min = app_decode_fixed_x100_i16(event->l2_temp_thresh);
  config.l2_rh_percent_max = app_decode_fixed_x100_u16(event->l2_humidity_thresh);
  config.l2_voc_min = (float)event->l2_bvoc_ppm_thresh;
  config.l3_temp_c_min = app_decode_fixed_x100_i16(event->l3_temp_thresh);
  config.l3_rh_percent_max = app_decode_fixed_x100_u16(event->l3_humidity_thresh);
  config.l3_voc_min = (float)event->l3_bvoc_ppm_thresh;
  config.l4_voc_min = (float)event->l4_bvoc_ppm_thresh;
  config.l5_voc_min = (float)event->l5_bvoc_ppm_thresh;
  config.l5_pm25_ug_m3_min = app_decode_fixed_x10_u16(event->l5_pm25_thresh);
  return config;
}

static void app_handle_registration_needed(
  void *context,
  const pyronet_host_registration_needed_v1_t *event)
{
  app_context_t *app = (app_context_t *)context;

  if ((app == NULL) || (event == NULL)) {
    return;
  }

  app_boundary_tx_handle_registration_needed(&app->boundary_tx,
                                             app_provisioning_identity(),
                                             event->reason);
}

static void app_handle_parent_changed(
  void *context,
  const pyronet_host_parent_changed_v1_t *event)
{
  app_context_t *app = (app_context_t *)context;

  if ((app == NULL) || (event == NULL)) {
    return;
  }

  app_boundary_tx_handle_parent_changed(&app->boundary_tx,
                                        app_provisioning_identity(),
                                        &app->time_anchor,
                                        event->change_reason,
                                        monotonic_time_now_ns());
}

static void app_handle_tx_result(void *context,
                                 const pyronet_host_tx_result_v1_t *event)
{
  (void)context;

  if (event == NULL) {
    return;
  }

  printf("TX_RESULT request=%s status=%s detail=%u\r\n",
         host_proto_type_name(event->request_type),
         host_proto_tx_status_name(event->status),
         event->detail);
}

static void app_handle_time_sync_update(
  void *context,
  const pyronet_host_time_sync_update_v1_t *event)
{
  app_context_t *app = (app_context_t *)context;

  if ((app == NULL) || (event == NULL)) {
    return;
  }

  app_time_anchor_set(&app->time_anchor,
                      monotonic_time_now_ns(),
                      event->unix_time_s);
  printf("TIME_SYNC_UPDATE unix_time_s=%lu\r\n", (unsigned long)event->unix_time_s);
  app_boundary_tx_flush(&app->boundary_tx,
                        app_provisioning_identity(),
                        &app->time_anchor);
  pyronet_risk_service_retry_pending(&app->risk_service);
}

static void app_handle_neighbor_alert_rx(
  void *context,
  const pyronet_host_neighbor_alert_received_v1_t *event)
{
  app_context_t *app = (app_context_t *)context;
  int64_t receive_time_ns;

  if ((app == NULL) || (event == NULL)) {
    return;
  }

  receive_time_ns = monotonic_time_now_ns();
  printf("NEIGHBOR_ALERT_RX node=%u risk=%u timestamp=%lu\r\n",
         (unsigned int)event->node_id,
         (unsigned int)event->risk_level,
         (unsigned long)event->timestamp);
  pyronet_risk_service_receive_neighbor_alert(&app->risk_service, receive_time_ns);
}

static void app_handle_config_update_rx(
  void *context,
  const pyronet_host_config_update_received_v1_t *event)
{
  app_context_t *app = (app_context_t *)context;
  pyronet_risk_config_t config;
  bool accepted;

  if ((app == NULL) || (event == NULL)) {
    return;
  }

  config = app_translate_config_update(app, event);
  accepted = pyronet_risk_service_apply_config_update(&app->risk_service,
                                                      &config,
                                                      monotonic_time_now_ns());
  printf("CONFIG_UPDATE_RX accepted=%u config_id=%lu\r\n",
         accepted ? 1U : 0U,
         (unsigned long)event->config_id);
}

void app_host_events_init_handlers(host_link_event_handlers_t *handlers,
                                   app_context_t *context)
{
  if (handlers == NULL) {
    return;
  }

  *handlers = (host_link_event_handlers_t) {
    .context = context,
    .on_registration_needed = app_handle_registration_needed,
    .on_parent_changed = app_handle_parent_changed,
    .on_tx_result = app_handle_tx_result,
    .on_time_sync_update = app_handle_time_sync_update,
    .on_neighbor_alert_rx = app_handle_neighbor_alert_rx,
    .on_config_update_rx = app_handle_config_update_rx,
  };
}
