#include "services/risk/pyronet_risk_service.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "services/air_quality/bsec_service.h"
#include "transport/host_link.h"

#define PYRONET_RISK_SERVICE_BATTERY_UNAVAILABLE 0xFFU
#define PYRONET_RISK_SERVICE_NODE_ID_INVALID     0U

static int32_t pyronet_risk_service_scale_float_signed(float value, float scale)
{
  float scaled_value = value * scale;

  if (scaled_value >= 0.0f) {
    scaled_value += 0.5f;
  } else {
    scaled_value -= 0.5f;
  }

  if (scaled_value > (float)INT32_MAX) {
    return INT32_MAX;
  }

  if (scaled_value < (float)INT32_MIN) {
    return INT32_MIN;
  }

  return (int32_t)scaled_value;
}

static long pyronet_risk_service_scale_float(float value, float scale)
{
  float scaled_value = value * scale;

  if (scaled_value >= 0.0f) {
    scaled_value += 0.5f;
  } else {
    scaled_value -= 0.5f;
  }

  return (long)scaled_value;
}

static void pyronet_risk_service_log_decimal_triplet(const char *label,
                                                     float value,
                                                     float scale,
                                                     unsigned int decimals)
{
  long scaled = pyronet_risk_service_scale_float(value, scale);
  long divisor = 1L;
  unsigned int index;

  for (index = 0U; index < decimals; index++) {
    divisor *= 10L;
  }

  printf(" %s=%ld.%0*ld",
         label,
         scaled / divisor,
         (int)decimals,
         labs(scaled % divisor));
}

static void pyronet_risk_service_log_snapshot(const char *label,
                                              const pyronet_risk_snapshot_t *snapshot)
{
  printf("%s level=%u previous=%u reason=%s override=%u config_id=%lu",
         label,
         (unsigned int)snapshot->current_level,
         (unsigned int)snapshot->previous_level,
         pyronet_risk_reason_name(snapshot->reason),
         snapshot->override_active ? 1U : 0U,
         (unsigned long)snapshot->config_id);

  if (snapshot->has_air_quality) {
    pyronet_risk_service_log_decimal_triplet("temp_c",
                                             snapshot->temperature_c,
                                             100.0f,
                                             2U);
    pyronet_risk_service_log_decimal_triplet("rh_pct",
                                             snapshot->humidity_percent,
                                             100.0f,
                                             2U);
    pyronet_risk_service_log_decimal_triplet("voc",
                                             snapshot->voc,
                                             1000.0f,
                                             3U);
  }

  if (snapshot->has_pm25) {
    pyronet_risk_service_log_decimal_triplet("pm25_ug_m3",
                                             snapshot->pm25_ug_m3,
                                             1000.0f,
                                             3U);
  }

  printf("\r\n");
}

static const char *pyronet_risk_service_pending_reason_name(uint8_t pending_flags)
{
  if ((pending_flags & PYRONET_RISK_PENDING_NODE_ID) != 0U) {
    return "node_id";
  }

  if ((pending_flags & PYRONET_RISK_PENDING_PM25) != 0U) {
    return "pm25";
  }

  if ((pending_flags & PYRONET_RISK_PENDING_UNIX_TIME) != 0U) {
    return "unix-time";
  }

  if ((pending_flags & PYRONET_RISK_PENDING_HOST_LINK) != 0U) {
    return "host-link";
  }

  return "unknown";
}

static void pyronet_risk_service_log_pending_snapshot(
  const char *label,
  const pyronet_risk_snapshot_t *snapshot,
  uint8_t pending_flags)
{
  char log_label[96];

  if ((label == NULL) || (snapshot == NULL)) {
    return;
  }

  (void)snprintf(log_label,
                 sizeof(log_label),
                 "%s missing=%s",
                 label,
                 pyronet_risk_service_pending_reason_name(pending_flags));
  pyronet_risk_service_log_snapshot(log_label, snapshot);
}

static void pyronet_risk_service_store_pending_request(
  pyronet_risk_service_pending_request_t *pending,
  const pyronet_risk_snapshot_t *snapshot,
  uint8_t pending_flags)
{
  if ((pending == NULL) || (snapshot == NULL) || (pending_flags == 0U)) {
    return;
  }

  pending->valid = true;
  pending->pending_flags = pending_flags;
  pending->snapshot = *snapshot;
}

static void pyronet_risk_service_clear_pending_request(
  pyronet_risk_service_pending_request_t *pending)
{
  if (pending == NULL) {
    return;
  }

  pending->valid = false;
  pending->pending_flags = 0U;
}

static int16_t pyronet_risk_service_temperature_x100(
  const pyronet_risk_snapshot_t *snapshot)
{
  int32_t scaled;

  if ((snapshot == NULL) || !snapshot->has_air_quality) {
    return 0;
  }

  scaled = pyronet_risk_service_scale_float_signed(snapshot->temperature_c, 100.0f);
  if (scaled > INT16_MAX) {
    return INT16_MAX;
  }

  if (scaled < INT16_MIN) {
    return INT16_MIN;
  }

  return (int16_t)scaled;
}

static uint16_t pyronet_risk_service_humidity_x100(
  const pyronet_risk_snapshot_t *snapshot)
{
  int32_t scaled;

  if ((snapshot == NULL) || !snapshot->has_air_quality) {
    return 0U;
  }

  scaled = pyronet_risk_service_scale_float_signed(snapshot->humidity_percent, 100.0f);
  if (scaled <= 0) {
    return 0U;
  }

  if (scaled > UINT16_MAX) {
    return UINT16_MAX;
  }

  return (uint16_t)scaled;
}

static uint16_t pyronet_risk_service_bvoc_ppm(
  const pyronet_risk_snapshot_t *snapshot)
{
  int32_t scaled;

  if ((snapshot == NULL) || !snapshot->has_air_quality) {
    return 0U;
  }

  /*
   * TODO: The wiki still leaves the exact integer on-wire bvoc scaling open.
   * Keep the semantic boundary in ppm and round into uint16_t without any
   * extra scale factor until that contract is finalized.
   */
  scaled = pyronet_risk_service_scale_float_signed(snapshot->voc, 1.0f);
  if (scaled <= 0) {
    return 0U;
  }

  if (scaled > UINT16_MAX) {
    return UINT16_MAX;
  }

  return (uint16_t)scaled;
}

static uint16_t pyronet_risk_service_pm25_x10(
  const pyronet_risk_snapshot_t *snapshot)
{
  int32_t scaled;

  if ((snapshot == NULL) || !snapshot->has_pm25) {
    return 0U;
  }

  scaled = pyronet_risk_service_scale_float_signed(snapshot->pm25_ug_m3, 10.0f);
  if (scaled <= 0) {
    return 0U;
  }

  if (scaled > UINT16_MAX) {
    return UINT16_MAX;
  }

  return (uint16_t)scaled;
}

static bool pyronet_risk_service_try_resolve_timestamp(
  const pyronet_risk_service_t *service,
  int64_t monotonic_timestamp_ns,
  uint32_t *out_timestamp_s)
{
  if ((service == NULL)
      || (service->resolve_unix_time == NULL)
      || (out_timestamp_s == NULL)) {
    return false;
  }

  return service->resolve_unix_time(service->time_context,
                                    monotonic_timestamp_ns,
                                    out_timestamp_s);
}

static bool pyronet_risk_service_try_send_report_payload(
  pyronet_risk_service_t *service,
  const pyronet_risk_snapshot_t *snapshot,
  uint8_t *out_pending_flags)
{
  pyronet_host_send_sensor_report_v1_t payload;
  uint8_t pending_flags = 0U;
  uint32_t timestamp_s;

  if ((service == NULL) || (snapshot == NULL)) {
    return false;
  }

  if (out_pending_flags != NULL) {
    *out_pending_flags = 0U;
  }

  if (!snapshot->has_air_quality) {
    pyronet_risk_service_log_snapshot("SEND_SENSOR_REPORT_DEFERRED missing=air-quality",
                                      snapshot);
    return false;
  }

  if (!service->node_id_valid) {
    pending_flags |= PYRONET_RISK_PENDING_NODE_ID;
  }

  if (!snapshot->has_pm25) {
    pending_flags |= PYRONET_RISK_PENDING_PM25;
  }

  if (!pyronet_risk_service_try_resolve_timestamp(service,
                                                  snapshot->timestamp_ns,
                                                  &timestamp_s)) {
    pending_flags |= PYRONET_RISK_PENDING_UNIX_TIME;
  }

  if (!host_link_is_ready()) {
    pending_flags |= PYRONET_RISK_PENDING_HOST_LINK;
  }

  if (pending_flags != 0U) {
    pyronet_risk_service_log_pending_snapshot("SEND_SENSOR_REPORT_DEFERRED",
                                              snapshot,
                                              pending_flags);
    if (out_pending_flags != NULL) {
      *out_pending_flags = pending_flags;
    }
    return false;
  }

  payload.node_id = service->node_id;
  payload.timestamp = timestamp_s;
  payload.risk_level = (uint8_t)snapshot->current_level;
  payload.temperature_c_x100 = pyronet_risk_service_temperature_x100(snapshot);
  payload.humidity_pct_x100 = pyronet_risk_service_humidity_x100(snapshot);
  payload.bvoc_ppm = pyronet_risk_service_bvoc_ppm(snapshot);
  payload.pm25_ug_m3_x10 = pyronet_risk_service_pm25_x10(snapshot);
  payload.battery_pct = service->battery_pct;

  if (!host_link_send_sensor_report(&payload)) {
    pyronet_risk_service_log_pending_snapshot("SEND_SENSOR_REPORT_DEFERRED",
                                              snapshot,
                                              PYRONET_RISK_PENDING_HOST_LINK);
    if (out_pending_flags != NULL) {
      *out_pending_flags = PYRONET_RISK_PENDING_HOST_LINK;
    }
    return false;
  }

  return true;
}

static void pyronet_risk_service_try_flush_pending_report(
  pyronet_risk_service_t *service)
{
  uint8_t pending_flags;
  pyronet_risk_snapshot_t snapshot;

  if ((service == NULL) || !service->pending_report.valid) {
    return;
  }

  snapshot = service->pending_report.snapshot;
  pending_flags = service->pending_report.pending_flags;

  if ((pending_flags & PYRONET_RISK_PENDING_PM25) != 0U) {
    if (!service->engine.has_pm25) {
      return;
    }

    snapshot.has_pm25 = true;
    snapshot.pm25_ug_m3 = service->engine.latest_pm25_ug_m3;
    pending_flags &= (uint8_t)~PYRONET_RISK_PENDING_PM25;
  }

  if (pyronet_risk_service_try_send_report_payload(service,
                                                   &snapshot,
                                                   &pending_flags)) {
    pyronet_risk_service_clear_pending_request(&service->pending_report);
    return;
  }

  if (pending_flags != 0U) {
    pyronet_risk_service_store_pending_request(&service->pending_report,
                                               &snapshot,
                                               pending_flags);
  }
}

static bool pyronet_risk_service_try_send_sensor_alert_payload(
  pyronet_risk_service_t *service,
  const pyronet_risk_snapshot_t *snapshot,
  uint8_t *out_pending_flags)
{
  pyronet_host_send_sensor_alert_v1_t payload;
  uint8_t pending_flags = 0U;
  uint32_t timestamp_s;

  if ((service == NULL) || (snapshot == NULL)) {
    return false;
  }

  if (out_pending_flags != NULL) {
    *out_pending_flags = 0U;
  }

  if (!snapshot->has_air_quality || !snapshot->has_pm25) {
    pyronet_risk_service_log_snapshot("SEND_SENSOR_ALERT_DEFERRED missing=telemetry",
                                      snapshot);
    return false;
  }

  if (!service->node_id_valid) {
    pending_flags |= PYRONET_RISK_PENDING_NODE_ID;
  }

  if (!pyronet_risk_service_try_resolve_timestamp(service,
                                                  snapshot->timestamp_ns,
                                                  &timestamp_s)) {
    pending_flags |= PYRONET_RISK_PENDING_UNIX_TIME;
  }

  if (!host_link_is_ready()) {
    pending_flags |= PYRONET_RISK_PENDING_HOST_LINK;
  }

  if (pending_flags != 0U) {
    pyronet_risk_service_log_pending_snapshot("SEND_SENSOR_ALERT_DEFERRED",
                                              snapshot,
                                              pending_flags);
    if (out_pending_flags != NULL) {
      *out_pending_flags = pending_flags;
    }
    return false;
  }

  payload.node_id = service->node_id;
  payload.timestamp = timestamp_s;
  payload.risk_level = (uint8_t)snapshot->current_level;
  payload.temperature_c_x100 = pyronet_risk_service_temperature_x100(snapshot);
  payload.humidity_pct_x100 = pyronet_risk_service_humidity_x100(snapshot);
  payload.bvoc_ppm = pyronet_risk_service_bvoc_ppm(snapshot);
  payload.pm25_ug_m3_x10 = pyronet_risk_service_pm25_x10(snapshot);
  payload.battery_pct = service->battery_pct;

  if (!host_link_send_sensor_alert(&payload)) {
    pyronet_risk_service_log_pending_snapshot("SEND_SENSOR_ALERT_DEFERRED",
                                              snapshot,
                                              PYRONET_RISK_PENDING_HOST_LINK);
    if (out_pending_flags != NULL) {
      *out_pending_flags = PYRONET_RISK_PENDING_HOST_LINK;
    }
    return false;
  }

  return true;
}

static void pyronet_risk_service_try_flush_pending_sensor_alert(
  pyronet_risk_service_t *service)
{
  uint8_t pending_flags;

  if ((service == NULL) || !service->pending_sensor_alert.valid) {
    return;
  }

  pending_flags = service->pending_sensor_alert.pending_flags;
  if (pyronet_risk_service_try_send_sensor_alert_payload(
        service,
        &service->pending_sensor_alert.snapshot,
        &pending_flags)) {
    pyronet_risk_service_clear_pending_request(&service->pending_sensor_alert);
    return;
  }

  if (pending_flags != 0U) {
    service->pending_sensor_alert.pending_flags = pending_flags;
  }
}

static bool pyronet_risk_service_try_send_neighbor_alert_payload(
  pyronet_risk_service_t *service,
  const pyronet_risk_snapshot_t *snapshot,
  uint8_t *out_pending_flags)
{
  pyronet_host_send_neighbor_alert_v1_t payload;
  uint8_t pending_flags = 0U;
  uint32_t timestamp_s;

  if ((service == NULL) || (snapshot == NULL)) {
    return false;
  }

  if (out_pending_flags != NULL) {
    *out_pending_flags = 0U;
  }

  if (!service->node_id_valid) {
    pending_flags |= PYRONET_RISK_PENDING_NODE_ID;
  }

  if (!pyronet_risk_service_try_resolve_timestamp(service,
                                                  snapshot->timestamp_ns,
                                                  &timestamp_s)) {
    pending_flags |= PYRONET_RISK_PENDING_UNIX_TIME;
  }

  if (!host_link_is_ready()) {
    pending_flags |= PYRONET_RISK_PENDING_HOST_LINK;
  }

  if (pending_flags != 0U) {
    pyronet_risk_service_log_pending_snapshot("SEND_NEIGHBOR_ALERT_DEFERRED",
                                              snapshot,
                                              pending_flags);
    if (out_pending_flags != NULL) {
      *out_pending_flags = pending_flags;
    }
    return false;
  }

  payload.node_id = service->node_id;
  payload.risk_level = (uint8_t)snapshot->current_level;
  payload.timestamp = timestamp_s;

  if (!host_link_send_neighbor_alert(&payload)) {
    pyronet_risk_service_log_pending_snapshot("SEND_NEIGHBOR_ALERT_DEFERRED",
                                              snapshot,
                                              PYRONET_RISK_PENDING_HOST_LINK);
    if (out_pending_flags != NULL) {
      *out_pending_flags = PYRONET_RISK_PENDING_HOST_LINK;
    }
    return false;
  }

  return true;
}

static void pyronet_risk_service_try_flush_pending_neighbor_alert(
  pyronet_risk_service_t *service)
{
  uint8_t pending_flags;

  if ((service == NULL) || !service->pending_neighbor_alert.valid) {
    return;
  }

  pending_flags = service->pending_neighbor_alert.pending_flags;
  if (pyronet_risk_service_try_send_neighbor_alert_payload(
        service,
        &service->pending_neighbor_alert.snapshot,
        &pending_flags)) {
    pyronet_risk_service_clear_pending_request(&service->pending_neighbor_alert);
    return;
  }

  if (pending_flags != 0U) {
    service->pending_neighbor_alert.pending_flags = pending_flags;
  }
}

static void pyronet_risk_service_send_report(void *context,
                                             const pyronet_risk_snapshot_t *snapshot,
                                             bool level_changed)
{
  pyronet_risk_service_t *service = (pyronet_risk_service_t *)context;
  uint8_t pending_flags = 0U;

  (void)level_changed;

  if ((service == NULL) || (snapshot == NULL)) {
    return;
  }

  if (pyronet_risk_service_try_send_report_payload(service,
                                                   snapshot,
                                                   &pending_flags)) {
    pyronet_risk_service_clear_pending_request(&service->pending_report);
    return;
  }

  if (pending_flags != 0U) {
    pyronet_risk_service_store_pending_request(&service->pending_report,
                                               snapshot,
                                               pending_flags);
  }
}

static void pyronet_risk_service_send_sensor_alert(
  void *context,
  const pyronet_risk_snapshot_t *snapshot)
{
  pyronet_risk_service_t *service = (pyronet_risk_service_t *)context;
  uint8_t pending_flags = 0U;

  if ((service == NULL) || (snapshot == NULL)) {
    return;
  }

  if (pyronet_risk_service_try_send_sensor_alert_payload(service,
                                                         snapshot,
                                                         &pending_flags)) {
    pyronet_risk_service_clear_pending_request(&service->pending_sensor_alert);
    return;
  }

  if (pending_flags != 0U) {
    pyronet_risk_service_store_pending_request(&service->pending_sensor_alert,
                                               snapshot,
                                               pending_flags);
  }
}

static void pyronet_risk_service_broadcast_neighbor_alert(
  void *context,
  const pyronet_risk_snapshot_t *snapshot)
{
  pyronet_risk_service_t *service = (pyronet_risk_service_t *)context;
  uint8_t pending_flags = 0U;

  if ((service == NULL) || (snapshot == NULL)) {
    return;
  }

  if (pyronet_risk_service_try_send_neighbor_alert_payload(service,
                                                           snapshot,
                                                           &pending_flags)) {
    pyronet_risk_service_clear_pending_request(&service->pending_neighbor_alert);
    return;
  }

  if (pending_flags != 0U) {
    pyronet_risk_service_store_pending_request(&service->pending_neighbor_alert,
                                               snapshot,
                                               pending_flags);
  }
}

static void pyronet_risk_service_request_pm25_sample(void *context)
{
  (void)context;
  printf("PM25_SAMPLE_REQUEST immediate=1\r\n");
}

static void pyronet_risk_service_set_pm25_schedule(void *context,
                                                   uint8_t samples_per_day)
{
  (void)context;

  if (samples_per_day == 0U) {
    printf("PM25_SCHEDULE samples_per_day=default\r\n");
    return;
  }

  printf("PM25_SCHEDULE samples_per_day=%u\r\n", samples_per_day);
}

static void pyronet_risk_service_set_report_interval(void *context,
                                                     int64_t interval_ns)
{
  long minutes = (long)(interval_ns / (60LL * 1000000000LL));

  (void)context;
  printf("REPORT_INTERVAL minutes=%ld\r\n", minutes);
}

void pyronet_risk_service_init(pyronet_risk_service_t *service,
                               int64_t now_ns,
                               void *time_context,
                               pyronet_risk_service_resolve_unix_time_fn_t resolve_unix_time)
{
  pyronet_risk_engine_ops_t ops = {
    .context = service,
    .send_report = pyronet_risk_service_send_report,
    .send_sensor_alert = pyronet_risk_service_send_sensor_alert,
    .broadcast_neighbor_alert = pyronet_risk_service_broadcast_neighbor_alert,
    .request_pm25_sample = pyronet_risk_service_request_pm25_sample,
    .set_pm25_schedule = pyronet_risk_service_set_pm25_schedule,
    .set_report_interval = pyronet_risk_service_set_report_interval,
  };

  if (service == NULL) {
    return;
  }

  memset(service, 0, sizeof(*service));
  service->time_context = time_context;
  service->resolve_unix_time = resolve_unix_time;
  service->node_id = PYRONET_RISK_SERVICE_NODE_ID_INVALID;
  service->node_id_valid = false;
  service->battery_pct = PYRONET_RISK_SERVICE_BATTERY_UNAVAILABLE;
  pyronet_risk_service_clear_pending_request(&service->pending_report);
  pyronet_risk_service_clear_pending_request(&service->pending_sensor_alert);
  pyronet_risk_service_clear_pending_request(&service->pending_neighbor_alert);
  pyronet_risk_engine_init(&service->engine, &ops, now_ns);
}

void pyronet_risk_service_set_node_id(pyronet_risk_service_t *service,
                                      uint16_t node_id)
{
  if (service == NULL) {
    return;
  }

  service->node_id = node_id;
  service->node_id_valid = (node_id != PYRONET_RISK_SERVICE_NODE_ID_INVALID);
  if (service->node_id_valid) {
    pyronet_risk_service_retry_pending(service);
  }
}

void pyronet_risk_service_set_battery_pct(pyronet_risk_service_t *service,
                                          uint8_t battery_pct)
{
  if (service == NULL) {
    return;
  }

  service->battery_pct = battery_pct;
}

void pyronet_risk_service_submit_air_quality(pyronet_risk_service_t *service,
                                             const air_quality_reading_t *reading)
{
  if ((service == NULL) || (reading == NULL)) {
    return;
  }

  pyronet_risk_engine_submit_air_quality(&service->engine,
                                         reading->timestamp_ns,
                                         reading->temperature_c,
                                         reading->humidity_percent,
                                         reading->breath_voc_equivalent_ppm);
}

void pyronet_risk_service_submit_pm25(pyronet_risk_service_t *service,
                                      int64_t timestamp_ns,
                                      float pm25_ug_m3)
{
  if (service == NULL) {
    return;
  }

  pyronet_risk_engine_submit_pm25(&service->engine, timestamp_ns, pm25_ug_m3);
  pyronet_risk_service_retry_pending(service);
}

void pyronet_risk_service_tick(pyronet_risk_service_t *service, int64_t now_ns)
{
  if (service == NULL) {
    return;
  }

  pyronet_risk_engine_tick(&service->engine, now_ns);
  pyronet_risk_service_retry_pending(service);
}

void pyronet_risk_service_retry_pending(pyronet_risk_service_t *service)
{
  if (service == NULL) {
    return;
  }

  pyronet_risk_service_try_flush_pending_report(service);
  pyronet_risk_service_try_flush_pending_sensor_alert(service);
  pyronet_risk_service_try_flush_pending_neighbor_alert(service);
}

bool pyronet_risk_service_apply_config_update(pyronet_risk_service_t *service,
                                              const pyronet_risk_config_t *config,
                                              int64_t now_ns)
{
  int applied;

  if (service == NULL) {
    return false;
  }

  applied = pyronet_risk_engine_apply_config_update(&service->engine,
                                                    config,
                                                    now_ns);
  printf("CONFIG_UPDATE accepted=%u config_id=%lu\r\n",
         applied ? 1U : 0U,
         (config != NULL) ? (unsigned long)config->config_id : 0UL);
  return applied != 0;
}

void pyronet_risk_service_receive_neighbor_alert(pyronet_risk_service_t *service,
                                                 int64_t now_ns)
{
  if (service == NULL) {
    return;
  }

  pyronet_risk_engine_receive_neighbor_alert(&service->engine, now_ns);
}

void pyronet_risk_service_set_override(pyronet_risk_service_t *service,
                                       bool active,
                                       pyronet_risk_level_t level,
                                       int64_t now_ns)
{
  if (service == NULL) {
    return;
  }

  pyronet_risk_engine_set_override(&service->engine, active, level, now_ns);
}

pyronet_risk_level_t pyronet_risk_service_current_level(
  const pyronet_risk_service_t *service)
{
  return (service != NULL)
           ? pyronet_risk_engine_current_level(&service->engine)
           : PYRONET_RISK_LEVEL_1;
}

const pyronet_risk_config_t *pyronet_risk_service_config(
  const pyronet_risk_service_t *service)
{
  return (service != NULL) ? pyronet_risk_engine_config(&service->engine) : NULL;
}
