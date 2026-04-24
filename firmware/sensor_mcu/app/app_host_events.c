#include "app/app_host_events.h"

#include <stdio.h>
#include <stdint.h>

#include "app/app_provisioning.h"
#include "platform/monotonic_time.h"

#define APP_EDT_UTC_OFFSET_S  (4L * 60L * 60L)

typedef struct {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
} app_edt_time_t;

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

static long app_abs_fraction(long scaled, unsigned int decimals)
{
  long divisor = 1L;
  unsigned int index;

  for (index = 0U; index < decimals; index++) {
    divisor *= 10L;
  }

  scaled %= divisor;
  return (scaled < 0L) ? -scaled : scaled;
}

static void app_log_config_thresholds(
  const pyronet_host_config_update_received_v1_t *event)
{
  if (event == NULL) {
    return;
  }

  printf("==== CONFIG_THRESHOLDS BEGIN config_id=%lu ====\r\n",
         (unsigned long)event->config_id);
  printf("CONFIG_THRESHOLD level=2 temp_min_c=%ld.%02ld rh_max_pct=%lu.%02lu voc_min_ppm=%lu\r\n",
         (long)event->l2_temp_thresh / 100L,
         app_abs_fraction((long)event->l2_temp_thresh, 2U),
         (unsigned long)event->l2_humidity_thresh / 100UL,
         (unsigned long)app_abs_fraction((long)event->l2_humidity_thresh, 2U),
         (unsigned long)event->l2_bvoc_ppm_thresh);
  printf("CONFIG_THRESHOLD level=3 temp_min_c=%ld.%02ld rh_max_pct=%lu.%02lu voc_min_ppm=%lu\r\n",
         (long)event->l3_temp_thresh / 100L,
         app_abs_fraction((long)event->l3_temp_thresh, 2U),
         (unsigned long)event->l3_humidity_thresh / 100UL,
         (unsigned long)app_abs_fraction((long)event->l3_humidity_thresh, 2U),
         (unsigned long)event->l3_bvoc_ppm_thresh);
  printf("CONFIG_THRESHOLD level=4 voc_min_ppm=%lu\r\n",
         (unsigned long)event->l4_bvoc_ppm_thresh);
  printf("CONFIG_THRESHOLD level=5 voc_min_ppm=%lu pm25_min_ug_m3=%lu.%01lu\r\n",
         (unsigned long)event->l5_bvoc_ppm_thresh,
         (unsigned long)event->l5_pm25_thresh / 10UL,
         (unsigned long)app_abs_fraction((long)event->l5_pm25_thresh, 1U));
  printf("==== CONFIG_THRESHOLDS END config_id=%lu ====\r\n",
         (unsigned long)event->config_id);
}

static void app_days_since_epoch_to_ymd(int64_t days,
                                        uint16_t *year,
                                        uint8_t *month,
                                        uint8_t *day)
{
  int64_t era;
  uint32_t day_of_era;
  uint32_t year_of_era;
  uint32_t day_of_year;
  uint32_t month_part;
  int32_t computed_year;
  uint32_t computed_month;

  days += 719468LL;
  era = (days >= 0LL ? days : days - 146096LL) / 146097LL;
  day_of_era = (uint32_t)(days - (era * 146097LL));
  year_of_era = (day_of_era - (day_of_era / 1460U) + (day_of_era / 36524U)
                 - (day_of_era / 146096U)) / 365U;
  computed_year = (int32_t)year_of_era + (int32_t)(era * 400LL);
  day_of_year = day_of_era
                - ((365U * year_of_era) + (year_of_era / 4U)
                   - (year_of_era / 100U));
  month_part = ((5U * day_of_year) + 2U) / 153U;
  *day = (uint8_t)(day_of_year - (((153U * month_part) + 2U) / 5U) + 1U);
  computed_month = (month_part < 10U) ? (month_part + 3U) : (month_part - 9U);
  computed_year += (computed_month <= 2U) ? 1 : 0;

  *year = (uint16_t)computed_year;
  *month = (uint8_t)computed_month;
}

static app_edt_time_t app_unix_time_to_edt(uint32_t unix_time_s)
{
  app_edt_time_t edt = { 0 };
  int64_t local_time_s = (int64_t)unix_time_s - APP_EDT_UTC_OFFSET_S;
  int64_t days;
  int64_t seconds_of_day;

  days = local_time_s / 86400LL;
  seconds_of_day = local_time_s % 86400LL;
  if (seconds_of_day < 0LL) {
    seconds_of_day += 86400LL;
    days--;
  }

  app_days_since_epoch_to_ymd(days, &edt.year, &edt.month, &edt.day);
  edt.hour = (uint8_t)(seconds_of_day / 3600LL);
  edt.minute = (uint8_t)((seconds_of_day % 3600LL) / 60LL);
  edt.second = (uint8_t)(seconds_of_day % 60LL);
  return edt;
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
  app_edt_time_t edt;

  if ((app == NULL) || (event == NULL)) {
    return;
  }

  app_time_anchor_set(&app->time_anchor,
                      monotonic_time_now_ns(),
                      event->unix_time_s);
  app->csp_time_sync_received = true;
  edt = app_unix_time_to_edt(event->unix_time_s);
  printf("TIME_SYNC_UPDATE accepted=1 unix_time_s=%lu edt=%04u-%02u-%02uT%02u:%02u:%02uEDT\r\n",
         (unsigned long)event->unix_time_s,
         (unsigned int)edt.year,
         (unsigned int)edt.month,
         (unsigned int)edt.day,
         (unsigned int)edt.hour,
         (unsigned int)edt.minute,
         (unsigned int)edt.second);
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
  if (accepted) {
    app_log_config_thresholds(event);
  }
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
