#ifndef SENSOR_BUS_H
#define SENSOR_BUS_H

#include <stdbool.h>
#include <stdint.h>

#include "board_i2c.h"

#define SENSOR_BUS_SPS30_ADDRESS   0x69U
#define SENSOR_BUS_BME68X_ADDRESS  0x76U

typedef struct {
  board_i2c_init_result_t i2c;
  board_i2c_transfer_status_t sps30_status;
  board_i2c_transfer_status_t bme68x_status;
  bool sps30_present;
  bool bme68x_present;
} sensor_bus_state_t;

sensor_bus_state_t sensor_bus_init(void);

#endif
