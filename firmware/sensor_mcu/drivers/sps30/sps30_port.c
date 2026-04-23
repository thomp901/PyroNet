#include "third_party/sps30/sensirion_i2c_hal.h"

#include "board_i2c.h"
#include "platform/monotonic_time.h"
#include "third_party/sps30/sensirion_common.h"
#include "third_party/sps30/sensirion_i2c.h"

static int16_t sps30_port_map_status(board_i2c_transfer_status_t status)
{
  switch (status) {
    case BOARD_I2C_STATUS_OK:
      return NO_ERROR;
    case BOARD_I2C_STATUS_NACK:
      return I2C_NACK_ERROR;
    case BOARD_I2C_STATUS_BUS_ERROR:
    case BOARD_I2C_STATUS_ARB_LOST:
    case BOARD_I2C_STATUS_TIMEOUT:
    case BOARD_I2C_STATUS_SW_FAULT:
    default:
      return I2C_BUS_ERROR;
  }
}

int16_t sensirion_i2c_hal_select_bus(uint8_t bus_idx)
{
  (void)bus_idx;
  return NO_ERROR;
}

void sensirion_i2c_hal_init(void)
{
  (void)board_i2c_init();
}

void sensirion_i2c_hal_free(void)
{
}

int8_t sensirion_i2c_hal_read(uint8_t address, uint8_t *data, uint8_t count)
{
  return (int8_t)sps30_port_map_status(board_i2c_read(address, data, count));
}

int8_t sensirion_i2c_hal_write(uint8_t address, const uint8_t *data, uint8_t count)
{
  return (int8_t)sps30_port_map_status(board_i2c_write(address, data, count));
}

void sensirion_i2c_hal_sleep_usec(uint32_t useconds)
{
  monotonic_time_delay_us(useconds);
}
