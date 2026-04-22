#ifndef SERVICES_RISK_PYRONET_RISK_SERVICE_H
#define SERVICES_RISK_PYRONET_RISK_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "services/air_quality/bsec_service.h"
#include "services/risk/pyronet_risk_engine.h"

typedef struct {
  pyronet_risk_engine_t engine;
} pyronet_risk_service_t;

void pyronet_risk_service_init(pyronet_risk_service_t *service, int64_t now_ns);
void pyronet_risk_service_submit_air_quality(pyronet_risk_service_t *service,
                                             const air_quality_reading_t *reading);
void pyronet_risk_service_submit_pm25(pyronet_risk_service_t *service,
                                      int64_t timestamp_ns,
                                      float pm25_ug_m3);
void pyronet_risk_service_tick(pyronet_risk_service_t *service, int64_t now_ns);
bool pyronet_risk_service_apply_config_update(pyronet_risk_service_t *service,
                                              const pyronet_risk_config_t *config,
                                              int64_t now_ns);
void pyronet_risk_service_receive_neighbor_alert(pyronet_risk_service_t *service,
                                                 int64_t now_ns);
void pyronet_risk_service_set_override(pyronet_risk_service_t *service,
                                       bool active,
                                       pyronet_risk_level_t level,
                                       int64_t now_ns);
pyronet_risk_level_t pyronet_risk_service_current_level(
  const pyronet_risk_service_t *service);
const pyronet_risk_config_t *pyronet_risk_service_config(
  const pyronet_risk_service_t *service);

#endif
