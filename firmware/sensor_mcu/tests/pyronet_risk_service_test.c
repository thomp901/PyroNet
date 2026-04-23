#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "services/risk/pyronet_risk_service.h"
#include "transport/host_proto.h"

typedef struct {
  bool valid;
  int64_t anchor_monotonic_ns;
  uint32_t anchor_unix_s;
} test_time_anchor_t;

typedef struct {
  bool ready;
  unsigned int report_count;
  unsigned int sensor_alert_count;
  unsigned int neighbor_alert_count;
  pyronet_host_send_sensor_report_v1_t last_report;
  pyronet_host_send_sensor_alert_v1_t last_sensor_alert;
  pyronet_host_send_neighbor_alert_v1_t last_neighbor_alert;
} test_host_link_state_t;

static test_host_link_state_t test_host_link_state;

static bool test_resolve_unix_time(void *context,
                                   int64_t monotonic_timestamp_ns,
                                   uint32_t *out_unix_time_s)
{
  test_time_anchor_t *anchor = (test_time_anchor_t *)context;
  int64_t delta_ns;
  uint64_t delta_s;

  if ((anchor == NULL) || (out_unix_time_s == NULL) || !anchor->valid) {
    return false;
  }

  delta_ns = monotonic_timestamp_ns - anchor->anchor_monotonic_ns;
  if (delta_ns >= 0) {
    *out_unix_time_s = anchor->anchor_unix_s
                       + (uint32_t)((uint64_t)delta_ns / 1000000000ULL);
    return true;
  }

  delta_s = ((uint64_t)(-delta_ns)) / 1000000000ULL;
  if (delta_s > anchor->anchor_unix_s) {
    return false;
  }

  *out_unix_time_s = anchor->anchor_unix_s - (uint32_t)delta_s;
  return true;
}

bool host_link_is_ready(void)
{
  return test_host_link_state.ready;
}

bool host_link_send_sensor_report(
  const pyronet_host_send_sensor_report_v1_t *payload)
{
  if ((payload == NULL) || !test_host_link_state.ready) {
    return false;
  }

  test_host_link_state.report_count++;
  test_host_link_state.last_report = *payload;
  return true;
}

bool host_link_send_sensor_alert(
  const pyronet_host_send_sensor_alert_v1_t *payload)
{
  if ((payload == NULL) || !test_host_link_state.ready) {
    return false;
  }

  test_host_link_state.sensor_alert_count++;
  test_host_link_state.last_sensor_alert = *payload;
  return true;
}

bool host_link_send_neighbor_alert(
  const pyronet_host_send_neighbor_alert_v1_t *payload)
{
  if ((payload == NULL) || !test_host_link_state.ready) {
    return false;
  }

  test_host_link_state.neighbor_alert_count++;
  test_host_link_state.last_neighbor_alert = *payload;
  return true;
}

static void test_reset_host_link_state(void)
{
  memset(&test_host_link_state, 0, sizeof(test_host_link_state));
  test_host_link_state.ready = true;
}

static void test_level5_alerts_flush_after_time_sync(void)
{
  pyronet_risk_service_t service;
  test_time_anchor_t anchor = { 0 };
  air_quality_reading_t reading = {
    .timestamp_ns = 11000000000LL,
    .temperature_c = 50.0f,
    .humidity_percent = 20.0f,
    .breath_voc_equivalent_ppm = 600.0f,
  };

  test_reset_host_link_state();
  pyronet_risk_service_init(&service, 0LL, &anchor, test_resolve_unix_time);
  pyronet_risk_service_set_node_id(&service, 77U);
  pyronet_risk_service_set_battery_pct(&service, 66U);

  pyronet_risk_service_submit_air_quality(&service, &reading);
  pyronet_risk_service_submit_pm25(&service, 12000000000LL, 50.0f);

  assert(test_host_link_state.report_count == 0U);
  assert(test_host_link_state.sensor_alert_count == 0U);
  assert(test_host_link_state.neighbor_alert_count == 0U);
  assert(service.pending_report.valid);
  assert(service.pending_sensor_alert.valid);
  assert(service.pending_neighbor_alert.valid);

  anchor.valid = true;
  anchor.anchor_monotonic_ns = 20000000000LL;
  anchor.anchor_unix_s = 1000U;
  pyronet_risk_service_retry_pending(&service);

  assert(test_host_link_state.report_count == 1U);
  assert(test_host_link_state.sensor_alert_count == 1U);
  assert(test_host_link_state.neighbor_alert_count == 1U);
  assert(test_host_link_state.last_report.timestamp == 992U);
  assert(test_host_link_state.last_sensor_alert.timestamp == 992U);
  assert(test_host_link_state.last_neighbor_alert.timestamp == 992U);
  assert(test_host_link_state.last_report.risk_level == PYRONET_RISK_LEVEL_5);
  assert(test_host_link_state.last_sensor_alert.risk_level == PYRONET_RISK_LEVEL_5);
  assert(test_host_link_state.last_neighbor_alert.risk_level == PYRONET_RISK_LEVEL_5);
  assert(!service.pending_report.valid);
  assert(!service.pending_sensor_alert.valid);
  assert(!service.pending_neighbor_alert.valid);
}

static void test_report_flush_after_node_id_becomes_valid(void)
{
  pyronet_risk_service_t service;
  test_time_anchor_t anchor = {
    .valid = true,
    .anchor_monotonic_ns = 20000000000LL,
    .anchor_unix_s = 1000U,
  };
  air_quality_reading_t reading = {
    .timestamp_ns = 10000000000LL,
    .temperature_c = 46.0f,
    .humidity_percent = 20.0f,
    .breath_voc_equivalent_ppm = 250.0f,
  };

  test_reset_host_link_state();
  pyronet_risk_service_init(&service, 0LL, &anchor, test_resolve_unix_time);
  pyronet_risk_service_set_battery_pct(&service, 55U);

  pyronet_risk_service_submit_pm25(&service, 9000000000LL, 10.0f);
  pyronet_risk_service_submit_air_quality(&service, &reading);

  assert(test_host_link_state.report_count == 0U);
  assert(service.pending_report.valid);
  assert((service.pending_report.pending_flags & PYRONET_RISK_PENDING_NODE_ID) != 0U);

  pyronet_risk_service_set_node_id(&service, 88U);

  assert(test_host_link_state.report_count == 1U);
  assert(test_host_link_state.last_report.node_id == 88U);
  assert(test_host_link_state.last_report.timestamp == 990U);
  assert(test_host_link_state.last_report.risk_level == PYRONET_RISK_LEVEL_3);
  assert(!service.pending_report.valid);
}

int main(void)
{
  test_level5_alerts_flush_after_time_sync();
  test_report_flush_after_node_id_becomes_valid();
  return 0;
}
