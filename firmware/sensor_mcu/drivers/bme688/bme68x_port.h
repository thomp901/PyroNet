#ifndef DRIVERS_BME688_BME68X_PORT_H
#define DRIVERS_BME688_BME68X_PORT_H

#include <stdint.h>

#include "third_party/bme68x/bme68x.h"

typedef struct {
  uint8_t i2c_address;
} bme68x_port_i2c_context_t;

void bme68x_port_init_i2c(struct bme68x_dev *device,
                          bme68x_port_i2c_context_t *context,
                          uint8_t i2c_address);
const char *bme68x_port_status_name(int8_t status);

#endif
