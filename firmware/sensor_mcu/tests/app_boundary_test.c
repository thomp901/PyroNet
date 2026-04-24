#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app.h"
#include "app/app_provisioning.h"
#include "app/app_state.h"
#include "sensor_bus.h"
#include "transport/host_link.h"

static int64_t test_now_ns;
static bool test_host_ready;
static unsigned int test_registration_count;
static pyronet_host_send_registration_v1_t test_last_registration;
static unsigned int test_sensor_report_count;
static pyronet_host_send_sensor_report_v1_t test_last_sensor_report;
static unsigned int test_sensor_alert_count;
static pyronet_host_send_sensor_alert_v1_t test_last_sensor_alert;
static unsigned int test_parent_update_count;
static pyronet_host_request_parent_update_v1_t test_last_parent_update;
static host_link_event_handlers_t test_host_handlers;
static unsigned int test_risk_retry_count;
static unsigned int test_neighbor_alert_receive_count;
static int64_t test_last_neighbor_alert_receive_ns;
static unsigned int test_config_apply_count;
static int64_t test_last_config_apply_ns;
static bool test_last_config_apply_accepted;
static pyronet_risk_config_t test_current_config;
static pyronet_risk_config_t test_last_applied_config;
static bool test_snapshot_valid;
static pyronet_risk_snapshot_t test_snapshot;
static app_registration_identity_t test_identity;
static bool test_boot_unix_time_valid;
static uint32_t test_boot_unix_time_s;

void debug_console_init(void)
{
}

void debug_console_emit_boot_markers(void)
{
}

void monotonic_time_init(void)
{
}

int64_t monotonic_time_now_ns(void)
{
  return test_now_ns;
}

uint64_t monotonic_time_now_us(void)
{
  return (uint64_t)(test_now_ns / 1000LL);
}

const app_registration_identity_t *app_provisioning_identity(void)
{
  return &test_identity;
}

bool app_provisioning_identity_valid(const app_registration_identity_t *identity)
{
  return (identity != NULL)
         && identity->configured
         && (identity->node_id != APP_PROVISIONING_NODE_ID_INVALID)
         && (identity->battery_pct <= 100U);
}

uint8_t app_provisioning_battery_pct(const app_registration_identity_t *identity)
{
  if (!app_provisioning_identity_valid(identity)) {
    return APP_PROVISIONING_BATTERY_UNAVAILABLE;
  }

  return identity->battery_pct;
}

bool app_provisioning_boot_unix_time_valid(void)
{
  return test_boot_unix_time_valid;
}

uint32_t app_provisioning_boot_unix_time_s(void)
{
  return test_boot_unix_time_s;
}

void pyronet_risk_service_init(pyronet_risk_service_t *service,
                               int64_t now_ns,
                               void *time_context,
                               pyronet_risk_service_resolve_unix_time_fn_t resolve_unix_time,
                               const pyronet_risk_service_sensor_ops_t *sensor_ops)
{
  (void)service;
  (void)now_ns;
  (void)time_context;
  (void)resolve_unix_time;
  (void)sensor_ops;
}

void pyronet_risk_service_set_node_id(pyronet_risk_service_t *service,
                                      uint16_t node_id)
{
  (void)service;
  (void)node_id;
}

void pyronet_risk_service_set_battery_pct(pyronet_risk_service_t *service,
                                          uint8_t battery_pct)
{
  (void)service;
  (void)battery_pct;
}

bool pyronet_risk_service_current_snapshot(const pyronet_risk_service_t *service,
                                           pyronet_risk_snapshot_t *out_snapshot)
{
  (void)service;

  if ((out_snapshot == NULL) || !test_snapshot_valid) {
    return false;
  }

  *out_snapshot = test_snapshot;
  return true;
}

void pyronet_risk_service_tick(pyronet_risk_service_t *service, int64_t now_ns)
{
  (void)service;
  (void)now_ns;
}

void pyronet_risk_service_submit_air_quality(pyronet_risk_service_t *service,
                                             const air_quality_reading_t *reading)
{
  (void)service;
  (void)reading;
}

void pyronet_risk_service_submit_pm25(pyronet_risk_service_t *service,
                                      int64_t timestamp_ns,
                                      float pm25_ug_m3)
{
  (void)service;
  (void)timestamp_ns;
  (void)pm25_ug_m3;
}

void pyronet_risk_service_retry_pending(pyronet_risk_service_t *service)
{
  (void)service;
  test_risk_retry_count++;
}

bool pyronet_risk_service_apply_config_update(pyronet_risk_service_t *service,
                                              const pyronet_risk_config_t *config,
                                              int64_t now_ns)
{
  (void)service;

  test_config_apply_count++;
  test_last_config_apply_ns = now_ns;
  test_last_applied_config = *config;
  test_last_config_apply_accepted = (config != NULL) && (config->config_id > test_current_config.config_id);
  if (test_last_config_apply_accepted) {
    test_current_config = *config;
  }

  return test_last_config_apply_accepted;
}

void pyronet_risk_service_receive_neighbor_alert(pyronet_risk_service_t *service,
                                                 int64_t now_ns)
{
  (void)service;
  test_neighbor_alert_receive_count++;
  test_last_neighbor_alert_receive_ns = now_ns;
}

const pyronet_risk_config_t *pyronet_risk_service_config(
  const pyronet_risk_service_t *service)
{
  (void)service;
  return &test_current_config;
}

pyronet_risk_config_t pyronet_risk_config_default(void)
{
  pyronet_risk_config_t config = { 0 };

  config.normal_pm25_samples_per_day = 3U;
  return config;
}

bool host_link_init(const host_link_event_handlers_t *event_handlers)
{
  if (event_handlers != NULL) {
    test_host_handlers = *event_handlers;
  }

  return true;
}

void host_link_poll(void)
{
}

bool host_link_is_ready(void)
{
  return test_host_ready;
}

bool host_link_ping(uint32_t token)
{
  (void)token;
  return true;
}

bool host_link_get_status(host_status_v1_t *out_status)
{
  memset(out_status, 0, sizeof(*out_status));
  return true;
}

bool host_link_send_registration(
  const pyronet_host_send_registration_v1_t *payload)
{
  if (payload == NULL) {
    return false;
  }

  test_registration_count++;
  test_last_registration = *payload;
  return true;
}

bool host_link_send_sensor_report(
  const pyronet_host_send_sensor_report_v1_t *payload)
{
  if (payload == NULL) {
    return false;
  }

  test_sensor_report_count++;
  test_last_sensor_report = *payload;
  return true;
}

bool host_link_send_sensor_alert(
  const pyronet_host_send_sensor_alert_v1_t *payload)
{
  if (payload == NULL) {
    return false;
  }

  test_sensor_alert_count++;
  test_last_sensor_alert = *payload;
  return true;
}

bool host_link_send_neighbor_alert(
  const pyronet_host_send_neighbor_alert_v1_t *payload)
{
  (void)payload;
  return true;
}

bool host_link_request_parent_update(
  const pyronet_host_request_parent_update_v1_t *payload)
{
  if (payload == NULL) {
    return false;
  }

  test_parent_update_count++;
  test_last_parent_update = *payload;
  return true;
}

const char *host_proto_type_name(uint8_t type)
{
  (void)type;
  return "TEST";
}

const char *host_proto_tx_status_name(uint8_t status)
{
  (void)status;
  return "test";
}

sensor_bus_state_t sensor_bus_init(void)
{
  sensor_bus_state_t state = { 0 };

  return state;
}

bool bsec_service_init(bsec_service_t *service, uint8_t i2c_address)
{
  (void)service;
  (void)i2c_address;
  return false;
}

bool bsec_service_read(bsec_service_t *service, air_quality_reading_t *reading)
{
  (void)service;
  (void)reading;
  return false;
}

bool bsec_service_process(bsec_service_t *service)
{
  (void)service;
  return false;
}

bool bsec_service_get_reading(const bsec_service_t *service, air_quality_reading_t *reading)
{
  (void)service;
  (void)reading;
  return false;
}

const bsec_service_output_t *bsec_service_latest_output(const bsec_service_t *service)
{
  (void)service;
  return NULL;
}

const char *bsec_service_status_name(bsec_library_return_t status)
{
  (void)status;
  return "test";
}

bool sps30_pm25_init(sps30_pm25_t *sensor, uint8_t i2c_address)
{
  (void)sensor;
  (void)i2c_address;
  return false;
}

bool sps30_pm25_read(sps30_pm25_t *sensor, float *pm25_ug_m3)
{
  (void)sensor;
  (void)pm25_ug_m3;
  return false;
}

int16_t sps30_pm25_last_status(const sps30_pm25_t *sensor)
{
  (void)sensor;
  return SPS30_BASE_STATUS_INVALID_STATE;
}

void sps30_pm25_get_firmware_version(const sps30_pm25_t *sensor,
                                     uint8_t *major,
                                     uint8_t *minor)
{
  (void)sensor;
  if (major != NULL) {
    *major = 0U;
  }
  if (minor != NULL) {
    *minor = 0U;
  }
}

const char *sps30_base_status_name(int16_t status)
{
  (void)status;
  return "test";
}

static void test_parent_update_uses_original_event_time(void)
{
  pyronet_host_parent_changed_v1_t parent_changed = {
    .change_reason = PYRONET_PARENT_CHANGE_PARENT_LOST,
  };
  pyronet_host_time_sync_update_v1_t time_sync = {
    .unix_time_s = 1000U,
  };

  memset(&test_host_handlers, 0, sizeof(test_host_handlers));
  memset(&test_last_registration, 0, sizeof(test_last_registration));
  memset(&test_last_parent_update, 0, sizeof(test_last_parent_update));
  test_registration_count = 0U;
  test_parent_update_count = 0U;
  test_risk_retry_count = 0U;
  test_host_ready = true;
  test_now_ns = 0LL;
  test_identity = (app_registration_identity_t) {
    .configured = true,
    .node_id = 321U,
    .latitude = 39.1234f,
    .longitude = -86.5432f,
    .fw_version_8_8 = 0x0102U,
    .battery_pct = 77U,
  };
  test_boot_unix_time_valid = false;
  test_boot_unix_time_s = 0U;

  app_init();
  assert(test_host_handlers.on_parent_changed != NULL);
  assert(test_host_handlers.on_time_sync_update != NULL);

  test_now_ns = 15000000000LL;
  test_host_handlers.on_parent_changed(test_host_handlers.context, &parent_changed);

  assert(test_parent_update_count == 0U);
  assert(app_context_get()->boundary_tx.pending_parent_update.valid);
  assert(app_context_get()->boundary_tx.pending_parent_update.event_monotonic_ns == 15000000000LL);

  test_now_ns = 20000000000LL;
  test_host_handlers.on_time_sync_update(test_host_handlers.context, &time_sync);

  assert(test_parent_update_count == 1U);
  assert(test_last_parent_update.node_id == 321U);
  assert(test_last_parent_update.timestamp == 995U);
  assert(!app_context_get()->boundary_tx.pending_parent_update.valid);
  assert(test_risk_retry_count > 0U);
}

static void test_registration_waits_for_configured_identity(void)
{
  pyronet_host_registration_needed_v1_t registration_needed = {
    .reason = 2U,
  };

  memset(&test_host_handlers, 0, sizeof(test_host_handlers));
  memset(&test_last_registration, 0, sizeof(test_last_registration));
  memset(&test_last_sensor_report, 0, sizeof(test_last_sensor_report));
  memset(&test_last_sensor_alert, 0, sizeof(test_last_sensor_alert));
  test_registration_count = 0U;
  test_sensor_report_count = 0U;
  test_sensor_alert_count = 0U;
  test_host_ready = true;
  test_now_ns = 1000000000LL;
  test_identity = (app_registration_identity_t) {
    .configured = false,
    .node_id = APP_PROVISIONING_NODE_ID_INVALID,
    .latitude = 0.0f,
    .longitude = 0.0f,
    .fw_version_8_8 = 0U,
    .battery_pct = 100U,
  };
  test_boot_unix_time_valid = false;
  test_boot_unix_time_s = 0U;

  app_init();

  assert(test_host_handlers.on_registration_needed != NULL);

  test_host_handlers.on_registration_needed(test_host_handlers.context,
                                            &registration_needed);
  assert(test_registration_count == 0U);
  assert(app_context_get()->boundary_tx.pending_registration.valid);
  assert(app_context_get()->boundary_tx.pending_registration.reason == 2U);

  test_identity = (app_registration_identity_t) {
    .configured = true,
    .node_id = 321U,
    .latitude = 39.1234f,
    .longitude = -86.5432f,
    .fw_version_8_8 = 0x0102U,
    .battery_pct = 77U,
  };

  app_process_action();

  assert(test_registration_count == 1U);
  assert(test_last_registration.node_id == 321U);
  assert(test_last_registration.latitude == 39.1234f);
  assert(test_last_registration.longitude == -86.5432f);
  assert(test_last_registration.fw_version == 0x0102U);
  assert(test_last_registration.battery_pct == 77U);
  assert(!app_context_get()->boundary_tx.pending_registration.valid);
}

static void test_timed_sensor_packets_send_after_startup_delays(void)
{
  memset(&test_host_handlers, 0, sizeof(test_host_handlers));
  memset(&test_last_sensor_report, 0, sizeof(test_last_sensor_report));
  memset(&test_last_sensor_alert, 0, sizeof(test_last_sensor_alert));
  test_registration_count = 0U;
  test_sensor_report_count = 0U;
  test_sensor_alert_count = 0U;
  test_host_ready = true;
  test_now_ns = 0LL;
  test_snapshot_valid = true;
  memset(&test_snapshot, 0, sizeof(test_snapshot));
  test_snapshot.timestamp_ns = 8000000000LL;
  test_snapshot.current_level = PYRONET_RISK_LEVEL_3;
  test_snapshot.previous_level = PYRONET_RISK_LEVEL_2;
  test_snapshot.has_air_quality = true;
  test_snapshot.temperature_c = 21.5f;
  test_snapshot.humidity_percent = 45.0f;
  test_snapshot.voc = 10.0f;
  test_snapshot.has_pm25 = true;
  test_snapshot.pm25_ug_m3 = 1.5f;
  test_identity = (app_registration_identity_t) {
    .configured = true,
    .node_id = 321U,
    .latitude = 39.1234f,
    .longitude = -86.5432f,
    .fw_version_8_8 = 0x0102U,
    .battery_pct = 77U,
  };
  test_boot_unix_time_valid = false;
  test_boot_unix_time_s = 0U;

  app_init();

  test_now_ns = 9000000000LL;
  app_process_action();
  assert(test_sensor_report_count == 0U);
  assert(test_sensor_alert_count == 0U);

  test_now_ns = 10000000000LL;
  app_process_action();
  assert(test_sensor_report_count == 1U);
  assert(test_last_sensor_report.node_id == 321U);
  assert(test_last_sensor_report.timestamp == 10U);
  assert(test_last_sensor_report.risk_level == 3U);
  assert(test_last_sensor_report.temperature_c_x100 == 2150);
  assert(test_last_sensor_report.humidity_pct_x100 == 4500U);
  assert(test_last_sensor_report.bvoc_ppm == 10U);
  assert(test_last_sensor_report.pm25_ug_m3_x10 == 15U);
  assert(test_sensor_alert_count == 0U);

  test_now_ns = 19000000000LL;
  app_process_action();
  assert(test_sensor_report_count == 1U);
  assert(test_sensor_alert_count == 0U);

  test_now_ns = 20000000000LL;
  app_process_action();
  assert(test_sensor_report_count == 1U);
  assert(test_sensor_alert_count == 1U);
  assert(test_last_sensor_alert.node_id == 321U);
  assert(test_last_sensor_alert.timestamp == 20U);
  assert(test_last_sensor_alert.risk_level == 3U);
  assert(test_last_sensor_alert.temperature_c_x100 == 2150);
  assert(test_last_sensor_alert.humidity_pct_x100 == 4500U);
  assert(test_last_sensor_alert.bvoc_ppm == 10U);
  assert(test_last_sensor_alert.pm25_ug_m3_x10 == 15U);
}

static void test_inbound_neighbor_alert_and_config_update_reach_risk_service(void)
{
  pyronet_host_neighbor_alert_received_v1_t neighbor_alert = {
    .node_id = 88U,
    .risk_level = 5U,
    .timestamp = 777U,
  };
  pyronet_host_config_update_received_v1_t config_update = {
    .config_id = 10U,
    .l2_temp_thresh = 3600,
    .l2_humidity_thresh = 4100U,
    .l2_bvoc_ppm_thresh = 101U,
    .l3_temp_thresh = 4600,
    .l3_humidity_thresh = 2600U,
    .l3_bvoc_ppm_thresh = 201U,
    .l4_bvoc_ppm_thresh = 301U,
    .l5_bvoc_ppm_thresh = 501U,
    .l5_pm25_thresh = 360U,
  };

  memset(&test_host_handlers, 0, sizeof(test_host_handlers));
  test_neighbor_alert_receive_count = 0U;
  test_last_neighbor_alert_receive_ns = 0LL;
  test_config_apply_count = 0U;
  test_last_config_apply_ns = 0LL;
  test_last_config_apply_accepted = false;
  memset(&test_last_applied_config, 0, sizeof(test_last_applied_config));
  test_current_config = pyronet_risk_config_default();
  test_current_config.config_id = 4U;
  test_current_config.normal_pm25_samples_per_day = 6U;
  test_host_ready = true;
  test_now_ns = 1000000000LL;

  app_init();

  assert(test_host_handlers.on_neighbor_alert_rx != NULL);
  assert(test_host_handlers.on_config_update_rx != NULL);

  test_now_ns = 23000000000LL;
  test_host_handlers.on_neighbor_alert_rx(test_host_handlers.context, &neighbor_alert);
  assert(test_neighbor_alert_receive_count == 1U);
  assert(test_last_neighbor_alert_receive_ns == 23000000000LL);

  test_now_ns = 24000000000LL;
  test_host_handlers.on_config_update_rx(test_host_handlers.context, &config_update);
  assert(test_config_apply_count == 1U);
  assert(test_last_config_apply_ns == 24000000000LL);
  assert(test_last_config_apply_accepted);
  assert(test_last_applied_config.config_id == config_update.config_id);
  assert(test_last_applied_config.l2_temp_c_min == 36.0f);
  assert(test_last_applied_config.l2_rh_percent_max == 41.0f);
  assert(test_last_applied_config.l2_voc_min == 101.0f);
  assert(test_last_applied_config.l3_temp_c_min == 46.0f);
  assert(test_last_applied_config.l3_rh_percent_max == 26.0f);
  assert(test_last_applied_config.l3_voc_min == 201.0f);
  assert(test_last_applied_config.l4_voc_min == 301.0f);
  assert(test_last_applied_config.l5_voc_min == 501.0f);
  assert(test_last_applied_config.l5_pm25_ug_m3_min == 36.0f);
  assert(test_last_applied_config.normal_pm25_samples_per_day == 6U);
}

int main(void)
{
  test_registration_waits_for_configured_identity();
  test_parent_update_uses_original_event_time();
  test_timed_sensor_packets_send_after_startup_delays();
  test_inbound_neighbor_alert_and_config_update_reach_risk_service();
  return 0;
}
