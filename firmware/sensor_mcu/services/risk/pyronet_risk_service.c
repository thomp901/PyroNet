#include "services/risk/pyronet_risk_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "services/air_quality/bsec_service.h"

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

static void pyronet_risk_service_send_report(void *context,
                                             const pyronet_risk_snapshot_t *snapshot,
                                             bool level_changed)
{
  (void)context;
  printf("REPORT_TX kind=%s",
         level_changed ? "level-change" : "periodic");
  pyronet_risk_service_log_snapshot("", snapshot);
}

static void pyronet_risk_service_send_sensor_alert(
  void *context,
  const pyronet_risk_snapshot_t *snapshot)
{
  (void)context;
  pyronet_risk_service_log_snapshot("SENSOR_ALERT_TX", snapshot);
}

static void pyronet_risk_service_broadcast_neighbor_alert(
  void *context,
  const pyronet_risk_snapshot_t *snapshot)
{
  (void)context;
  pyronet_risk_service_log_snapshot("NEIGHBOR_ALERT_TX", snapshot);
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

void pyronet_risk_service_init(pyronet_risk_service_t *service, int64_t now_ns)
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
  pyronet_risk_engine_init(&service->engine, &ops, now_ns);
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
}

void pyronet_risk_service_tick(pyronet_risk_service_t *service, int64_t now_ns)
{
  if (service == NULL) {
    return;
  }

  pyronet_risk_engine_tick(&service->engine, now_ns);
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
