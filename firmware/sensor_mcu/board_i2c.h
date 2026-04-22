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
board_i2c_transfer_status_t board_i2c_write(uint8_t address,
                                            const uint8_t *data,
                                            uint16_t length);
board_i2c_transfer_status_t board_i2c_read(uint8_t address,
                                           uint8_t *data,
                                           uint16_t length);
board_i2c_transfer_status_t board_i2c_write_read(uint8_t address,
                                                 const uint8_t *write_data,
                                                 uint16_t write_length,
                                                 uint8_t *read_data,
                                                 uint16_t read_length);
board_i2c_transfer_status_t board_i2c_write_register(uint8_t address,
                                                     uint8_t register_address,
                                                     const uint8_t *data,
                                                     uint16_t length);
board_i2c_transfer_status_t board_i2c_read_register(uint8_t address,
                                                    uint8_t register_address,
                                                    uint8_t *data,
                                                    uint16_t length);

#endif
