#ifndef BOARD_I2C_H
#define BOARD_I2C_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  bool scl_before;
  bool sda_before;
  bool scl_after_recovery;
  bool sda_after_recovery;
  uint32_t frequency_hz;
} board_i2c_init_result_t;

typedef enum {
  BOARD_I2C_STATUS_OK = 0,
  BOARD_I2C_STATUS_NACK,
  BOARD_I2C_STATUS_BUS_ERROR,
  BOARD_I2C_STATUS_ARB_LOST,
  BOARD_I2C_STATUS_TIMEOUT,
  BOARD_I2C_STATUS_SW_FAULT
} board_i2c_transfer_status_t;

board_i2c_init_result_t board_i2c_init(void);
board_i2c_transfer_status_t board_i2c_probe(uint8_t address);

#endif
