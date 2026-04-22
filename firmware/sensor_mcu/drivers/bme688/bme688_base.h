#ifndef DRIVERS_BME688_BME688_BASE_H
#define DRIVERS_BME688_BME688_BASE_H

#include <stdbool.h>
#include <stdint.h>

#include "drivers/bme688/bme68x_port.h"

typedef struct {
  uint8_t temperature_oversampling;
  uint8_t humidity_oversampling;
  uint8_t pressure_oversampling;
  uint8_t filter;
  uint16_t heater_temperature_c;
  uint16_t heater_duration_ms;
  bool run_gas;
} bme688_base_measurement_request_t;

typedef struct {
  float temperature_c;
  float humidity_percent;
  float pressure_pa;
  float gas_resistance_ohms;
  uint8_t gas_index;
  uint8_t measurement_index;
  uint8_t status;
  int64_t timestamp_ns;
} bme688_base_sample_t;

typedef struct {
  bool initialized;
  uint8_t i2c_address;
  struct bme68x_dev device;
  bme68x_port_i2c_context_t port_context;
} bme688_base_t;

int8_t bme688_base_init(bme688_base_t *sensor, uint8_t i2c_address);
int8_t bme688_base_measure_forced(bme688_base_t *sensor,
                                  const bme688_base_measurement_request_t *request,
                                  bme688_base_sample_t *sample);
const char *bme688_base_status_name(int8_t status);

#endif
