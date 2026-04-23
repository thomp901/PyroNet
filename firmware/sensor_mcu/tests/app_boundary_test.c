#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app.h"
#include "app/app_state.h"
#include "transport/host_link.h"

static int64_t test_now_ns;
static bool test_host_ready;
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

void pyronet_risk_service_init(pyronet_risk_service_t *service,
                               int64_t now_ns,
                               void *time_context,
                               pyronet_risk_service_resolve_unix_time_fn_t resolve_unix_time)
{
  (void)service;
  (void)now_ns;
  (void)time_context;
  (void)resolve_unix_time;
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

void pyronet_risk_service_tick(pyronet_risk_service_t *service, int64_t now_ns)
{
  (void)service;
  (void)now_ns;
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
  (void)payload;
  return true;
}

bool host_link_send_sensor_report(
  const pyronet_host_send_sensor_report_v1_t *payload)
{
  (void)payload;
  return true;
}

bool host_link_send_sensor_alert(
  const pyronet_host_send_sensor_alert_v1_t *payload)
{
  (void)payload;
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

static void test_parent_update_uses_original_event_time(void)
{
  pyronet_host_parent_changed_v1_t parent_changed = {
    .change_reason = PYRONET_PARENT_CHANGE_PARENT_LOST,
  };
  pyronet_host_time_sync_update_v1_t time_sync = {
    .unix_time_s = 1000U,
  };

  memset(&test_host_handlers, 0, sizeof(test_host_handlers));
  memset(&test_last_parent_update, 0, sizeof(test_last_parent_update));
  test_parent_update_count = 0U;
  test_risk_retry_count = 0U;
  test_host_ready = true;
  test_now_ns = 0LL;

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
  test_parent_update_uses_original_event_time();
  test_inbound_neighbor_alert_and_config_update_reach_risk_service();
  return 0;
}
