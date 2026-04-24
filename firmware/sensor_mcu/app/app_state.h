#ifndef APP_APP_STATE_H
#define APP_APP_STATE_H

#include "app/app_boundary_tx.h"
#include "app/app_sensor_runtime.h"
#include "app/app_time_anchor.h"
#include "services/risk/pyronet_risk_service.h"

typedef struct {
  app_time_anchor_t time_anchor;
  app_boundary_tx_t boundary_tx;
  pyronet_risk_service_t risk_service;
  app_sensor_runtime_t sensors;
} app_context_t;

app_context_t *app_context_get(void);

#endif
