#ifndef APP_APP_SENSOR_RUNTIME_H
#define APP_APP_SENSOR_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#include "sensor_bus.h"
#include "services/air_quality/bsec_service.h"
#include "services/particulate/sps30_pm25.h"
#include "services/risk/pyronet_risk_service.h"

typedef struct {
  sensor_bus_state_t bus;
  bool bsec_ready;
  bool sps30_ready;
  bsec_service_t air_quality;
  sps30_pm25_t pm25;
  pyronet_risk_service_t *risk_service;
  int64_t air_quality_interval_ns;
  int64_t next_air_quality_due_ns;
  int64_t last_air_quality_timestamp_ns;
  int64_t pm25_interval_ns;
  int64_t next_pm25_due_ns;
  bool pm25_immediate_requested;
  bool has_latest_air_quality;
  bool has_latest_gas_diagnostic;
  bool has_latest_pm25;
  air_quality_reading_t latest_air_quality;
  bsec_service_output_t latest_gas_diagnostic;
  int64_t latest_pm25_timestamp_ns;
  float latest_pm25_ug_m3;
  int64_t last_emitted_pm25_timestamp_ns;
  uint32_t read_seq;
} app_sensor_runtime_t;

void app_sensor_runtime_init(app_sensor_runtime_t *runtime,
                             pyronet_risk_service_t *risk_service);
void app_sensor_runtime_process(app_sensor_runtime_t *runtime);
void app_sensor_runtime_request_pm25_sample(void *context);
void app_sensor_runtime_set_pm25_schedule(void *context, uint8_t samples_per_day);

#endif
