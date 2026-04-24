#ifndef SERVICES_RISK_PYRONET_RISK_SERVICE_H
#define SERVICES_RISK_PYRONET_RISK_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "services/air_quality/bsec_service.h"
#include "services/risk/pyronet_risk_engine.h"

typedef bool (*pyronet_risk_service_resolve_unix_time_fn_t)(
  void *context,
  int64_t monotonic_timestamp_ns,
  uint32_t *out_unix_time_s);

typedef struct {
  void *context;
  void (*request_pm25_sample)(void *context);
  void (*set_pm25_schedule)(void *context, uint8_t samples_per_day);
} pyronet_risk_service_sensor_ops_t;

enum pyronet_risk_service_pending_flag {
  PYRONET_RISK_PENDING_NODE_ID = 1 << 0,
  PYRONET_RISK_PENDING_UNIX_TIME = 1 << 1,
  PYRONET_RISK_PENDING_PM25 = 1 << 2,
  PYRONET_RISK_PENDING_HOST_LINK = 1 << 3,
};

typedef struct {
  bool valid;
  uint8_t pending_flags;
  pyronet_risk_snapshot_t snapshot;
} pyronet_risk_service_pending_request_t;

typedef struct {
  pyronet_risk_engine_t engine;
  void *time_context;
  pyronet_risk_service_resolve_unix_time_fn_t resolve_unix_time;
  pyronet_risk_service_sensor_ops_t sensor_ops;
  bool node_id_valid;
  uint16_t node_id;
  uint8_t battery_pct;
  pyronet_risk_service_pending_request_t pending_report;
  pyronet_risk_service_pending_request_t pending_sensor_alert;
  pyronet_risk_service_pending_request_t pending_neighbor_alert;
} pyronet_risk_service_t;

void pyronet_risk_service_init(pyronet_risk_service_t *service,
                               int64_t now_ns,
                               void *time_context,
                               pyronet_risk_service_resolve_unix_time_fn_t resolve_unix_time,
                               const pyronet_risk_service_sensor_ops_t *sensor_ops);
void pyronet_risk_service_set_node_id(pyronet_risk_service_t *service,
                                      uint16_t node_id);
void pyronet_risk_service_set_battery_pct(pyronet_risk_service_t *service,
                                          uint8_t battery_pct);
void pyronet_risk_service_submit_air_quality(pyronet_risk_service_t *service,
                                             const air_quality_reading_t *reading);
void pyronet_risk_service_submit_pm25(pyronet_risk_service_t *service,
                                      int64_t timestamp_ns,
                                      float pm25_ug_m3);
void pyronet_risk_service_tick(pyronet_risk_service_t *service, int64_t now_ns);
void pyronet_risk_service_retry_pending(pyronet_risk_service_t *service);
bool pyronet_risk_service_apply_config_update(pyronet_risk_service_t *service,
                                              const pyronet_risk_config_t *config,
                                              int64_t now_ns);
void pyronet_risk_service_receive_neighbor_alert(pyronet_risk_service_t *service,
                                                 int64_t now_ns);
void pyronet_risk_service_set_override(pyronet_risk_service_t *service,
                                       bool active,
                                       pyronet_risk_level_t level,
                                       int64_t now_ns);
bool pyronet_risk_service_current_snapshot(const pyronet_risk_service_t *service,
                                           pyronet_risk_snapshot_t *out_snapshot);
pyronet_risk_level_t pyronet_risk_service_current_level(
  const pyronet_risk_service_t *service);
const pyronet_risk_config_t *pyronet_risk_service_config(
  const pyronet_risk_service_t *service);

#endif
