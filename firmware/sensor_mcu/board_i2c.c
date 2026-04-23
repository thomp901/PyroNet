#include "board_i2c.h"

#include "em_cmu.h"
#include "em_gpio.h"
#include "em_i2c.h"

#define BOARD_I2C_INSTANCE            I2C0
#define BOARD_I2C_CLOCK               cmuClock_I2C0
#define BOARD_I2C_ROUTE_INDEX         0U
#define BOARD_I2C_SCL_PORT            gpioPortD
#define BOARD_I2C_SCL_PIN             2U
#define BOARD_I2C_SDA_PORT            gpioPortD
#define BOARD_I2C_SDA_PIN             3U
#define BOARD_I2C_RECOVERY_CLOCKS     9U
#define BOARD_I2C_POLL_LIMIT          10000U

static void board_i2c_short_delay(void)
{
  volatile uint32_t delay = 256U;

  while (delay-- > 0U) {
  }
}

static void board_i2c_recover_bus(void)
{
  uint32_t pulse;

  GPIO_PinModeSet(BOARD_I2C_SCL_PORT,
                  BOARD_I2C_SCL_PIN,
                  gpioModeWiredAndPullUp,
                  1U);
  GPIO_PinModeSet(BOARD_I2C_SDA_PORT,
                  BOARD_I2C_SDA_PIN,
                  gpioModeWiredAndPullUp,
                  1U);

  for (pulse = 0U; pulse < BOARD_I2C_RECOVERY_CLOCKS; pulse++) {
    GPIO_PinOutClear(BOARD_I2C_SCL_PORT, BOARD_I2C_SCL_PIN);
    board_i2c_short_delay();
    GPIO_PinOutSet(BOARD_I2C_SCL_PORT, BOARD_I2C_SCL_PIN);
    board_i2c_short_delay();
  }
}

static void board_i2c_route_pins(void)
{
  GPIO->I2CROUTE[BOARD_I2C_ROUTE_INDEX].ROUTEEN = 0U;
  GPIO->I2CROUTE[BOARD_I2C_ROUTE_INDEX].SCLROUTE =
    ((uint32_t)BOARD_I2C_SCL_PIN << _GPIO_I2C_SCLROUTE_PIN_SHIFT)
    | ((uint32_t)BOARD_I2C_SCL_PORT << _GPIO_I2C_SCLROUTE_PORT_SHIFT);
  GPIO->I2CROUTE[BOARD_I2C_ROUTE_INDEX].SDAROUTE =
    ((uint32_t)BOARD_I2C_SDA_PIN << _GPIO_I2C_SDAROUTE_PIN_SHIFT)
    | ((uint32_t)BOARD_I2C_SDA_PORT << _GPIO_I2C_SDAROUTE_PORT_SHIFT);
  GPIO->I2CROUTE[BOARD_I2C_ROUTE_INDEX].ROUTEEN =
    GPIO_I2C_ROUTEEN_SCLPEN | GPIO_I2C_ROUTEEN_SDAPEN;
}

static board_i2c_transfer_status_t board_i2c_map_status(I2C_TransferReturn_TypeDef status)
{
  switch (status) {
    case i2cTransferDone:
      return BOARD_I2C_STATUS_OK;
    case i2cTransferNack:
      return BOARD_I2C_STATUS_NACK;
    case i2cTransferBusErr:
      return BOARD_I2C_STATUS_BUS_ERROR;
    case i2cTransferArbLost:
      return BOARD_I2C_STATUS_ARB_LOST;
    default:
      return BOARD_I2C_STATUS_SW_FAULT;
  }
}

static board_i2c_transfer_status_t board_i2c_run_transfer(I2C_TransferSeq_TypeDef *sequence)
{
  I2C_TransferReturn_TypeDef status;
  uint32_t polls = 0U;

  status = I2C_TransferInit(BOARD_I2C_INSTANCE, sequence);
  while ((status == i2cTransferInProgress) && (polls < BOARD_I2C_POLL_LIMIT)) {
    status = I2C_Transfer(BOARD_I2C_INSTANCE);
    polls++;
  }

  if (status == i2cTransferInProgress) {
    return BOARD_I2C_STATUS_TIMEOUT;
  }

  return board_i2c_map_status(status);
}

board_i2c_init_result_t board_i2c_init(void)
{
  I2C_Init_TypeDef init = I2C_INIT_DEFAULT;
  board_i2c_init_result_t result;

  CMU_ClockEnable(cmuClock_GPIO, true);
  CMU_ClockEnable(BOARD_I2C_CLOCK, true);

  result.scl_before = GPIO_PinInGet(BOARD_I2C_SCL_PORT, BOARD_I2C_SCL_PIN);
  result.sda_before = GPIO_PinInGet(BOARD_I2C_SDA_PORT, BOARD_I2C_SDA_PIN);

  board_i2c_recover_bus();

  result.scl_after_recovery = GPIO_PinInGet(BOARD_I2C_SCL_PORT, BOARD_I2C_SCL_PIN);
  result.sda_after_recovery = GPIO_PinInGet(BOARD_I2C_SDA_PORT, BOARD_I2C_SDA_PIN);

  board_i2c_route_pins();

  I2C_Reset(BOARD_I2C_INSTANCE);
  init.freq = I2C_FREQ_STANDARD_MAX;
  I2C_Init(BOARD_I2C_INSTANCE, &init);

  result.frequency_hz = I2C_BusFreqGet(BOARD_I2C_INSTANCE);
  return result;
}

board_i2c_transfer_status_t board_i2c_probe(uint8_t address)
{
  I2C_TransferSeq_TypeDef seq = { 0 };
  uint8_t dummy = 0U;

  seq.addr = (uint16_t)(address << 1);
  seq.flags = I2C_FLAG_WRITE;
  seq.buf[0].data = &dummy;
  seq.buf[0].len = 0U;

  return board_i2c_run_transfer(&seq);
}

board_i2c_transfer_status_t board_i2c_write(uint8_t address,
                                            const uint8_t *data,
                                            uint16_t length)
{
  I2C_TransferSeq_TypeDef seq = { 0 };

  seq.addr = (uint16_t)(address << 1);
  seq.flags = I2C_FLAG_WRITE;
  seq.buf[0].data = (uint8_t *)data;
  seq.buf[0].len = length;

  return board_i2c_run_transfer(&seq);
}

board_i2c_transfer_status_t board_i2c_read(uint8_t address,
                                           uint8_t *data,
                                           uint16_t length)
{
  I2C_TransferSeq_TypeDef seq = { 0 };

  seq.addr = (uint16_t)(address << 1);
  seq.flags = I2C_FLAG_READ;
  seq.buf[0].data = data;
  seq.buf[0].len = length;

  return board_i2c_run_transfer(&seq);
}

board_i2c_transfer_status_t board_i2c_write_read(uint8_t address,
                                                 const uint8_t *write_data,
                                                 uint16_t write_length,
                                                 uint8_t *read_data,
                                                 uint16_t read_length)
{
  I2C_TransferSeq_TypeDef seq = { 0 };

  seq.addr = (uint16_t)(address << 1);
  seq.flags = I2C_FLAG_WRITE_READ;
  seq.buf[0].data = (uint8_t *)write_data;
  seq.buf[0].len = write_length;
  seq.buf[1].data = read_data;
  seq.buf[1].len = read_length;

  return board_i2c_run_transfer(&seq);
}

board_i2c_transfer_status_t board_i2c_write_register(uint8_t address,
                                                     uint8_t register_address,
                                                     const uint8_t *data,
                                                     uint16_t length)
{
  uint8_t buffer[32];

  if (length > (sizeof(buffer) - 1U)) {
    return BOARD_I2C_STATUS_SW_FAULT;
  }

  buffer[0] = register_address;
  for (uint16_t i = 0U; i < length; i++) {
    buffer[i + 1U] = data[i];
  }

  return board_i2c_write(address, buffer, (uint16_t)(length + 1U));
}

board_i2c_transfer_status_t board_i2c_read_register(uint8_t address,
                                                    uint8_t register_address,
                                                    uint8_t *data,
                                                    uint16_t length)
{
  return board_i2c_write_read(address, &register_address, 1U, data, length);
}
