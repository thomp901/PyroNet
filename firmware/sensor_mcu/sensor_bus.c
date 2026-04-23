#include "sensor_bus.h"

#include <stdio.h>

static const char *sensor_bus_status_name(board_i2c_transfer_status_t status)
{
  switch (status) {
    case BOARD_I2C_STATUS_OK:
      return "ok";
    case BOARD_I2C_STATUS_NACK:
      return "nack";
    case BOARD_I2C_STATUS_BUS_ERROR:
      return "bus-error";
    case BOARD_I2C_STATUS_ARB_LOST:
      return "arb-lost";
    case BOARD_I2C_STATUS_TIMEOUT:
      return "timeout";
    case BOARD_I2C_STATUS_SW_FAULT:
    default:
      return "sw-fault";
  }
}

static void sensor_bus_log_probe_result(const char *name,
                                        uint8_t address,
                                        board_i2c_transfer_status_t status)
{
  if (status == BOARD_I2C_STATUS_OK) {
    printf("SENSOR_FOUND name=%s addr=0x%02X\r\n", name, address);
    return;
  }

  printf("SENSOR_PROBE name=%s addr=0x%02X status=%s\r\n",
         name,
         address,
         sensor_bus_status_name(status));
}

sensor_bus_state_t sensor_bus_init(void)
{
  sensor_bus_state_t state = { 0 };

  state.i2c = board_i2c_init();

  printf("I2C_LINES before scl=%u sda=%u\r\n",
         state.i2c.scl_before,
         state.i2c.sda_before);
  printf("I2C_LINES after-recovery scl=%u sda=%u\r\n",
         state.i2c.scl_after_recovery,
         state.i2c.sda_after_recovery);
  printf("I2C_READY bus=I2C0 freq=%lu scl=PD02 sda=PD03\r\n",
         (unsigned long)state.i2c.frequency_hz);

  state.sps30_status = board_i2c_probe(SENSOR_BUS_SPS30_ADDRESS);
  state.sps30_present = (state.sps30_status == BOARD_I2C_STATUS_OK);
  sensor_bus_log_probe_result("SPS30", SENSOR_BUS_SPS30_ADDRESS, state.sps30_status);

  if ((state.sps30_status != BOARD_I2C_STATUS_OK)
      && (state.sps30_status != BOARD_I2C_STATUS_NACK)) {
    state.i2c = board_i2c_init();
  }

  state.bme68x_status = board_i2c_probe(SENSOR_BUS_BME68X_ADDRESS);
  state.bme68x_present = (state.bme68x_status == BOARD_I2C_STATUS_OK);
  sensor_bus_log_probe_result("BME68x", SENSOR_BUS_BME68X_ADDRESS, state.bme68x_status);

  return state;
}
