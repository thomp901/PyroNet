#include "drivers/bme688/bme68x_port.h"

#include <string.h>

#include "board_i2c.h"
#include "platform/monotonic_time.h"

#define BME68X_PORT_WRITE_BUFFER_SIZE  32U

static BME68X_INTF_RET_TYPE bme68x_port_i2c_read(uint8_t register_address,
                                                 uint8_t *register_data,
                                                 uint32_t length,
                                                 void *interface_pointer)
{
  bme68x_port_i2c_context_t *context = (bme68x_port_i2c_context_t *)interface_pointer;
  board_i2c_transfer_status_t status;

  status = board_i2c_read_register(context->i2c_address,
                                   register_address,
                                   register_data,
                                   (uint16_t)length);

  return (status == BOARD_I2C_STATUS_OK) ? BME68X_INTF_RET_SUCCESS : BME68X_E_COM_FAIL;
}

static BME68X_INTF_RET_TYPE bme68x_port_i2c_write(uint8_t register_address,
                                                  const uint8_t *register_data,
                                                  uint32_t length,
                                                  void *interface_pointer)
{
  bme68x_port_i2c_context_t *context = (bme68x_port_i2c_context_t *)interface_pointer;
  uint8_t write_buffer[BME68X_PORT_WRITE_BUFFER_SIZE];

  if ((length + 1U) > sizeof(write_buffer)) {
    return BME68X_E_INVALID_LENGTH;
  }

  write_buffer[0] = register_address;
  memcpy(&write_buffer[1], register_data, length);

  return (board_i2c_write(context->i2c_address, write_buffer, (uint16_t)(length + 1U))
          == BOARD_I2C_STATUS_OK)
           ? BME68X_INTF_RET_SUCCESS
           : BME68X_E_COM_FAIL;
}

static void bme68x_port_delay_us(uint32_t period_us, void *interface_pointer)
{
  (void)interface_pointer;
  monotonic_time_delay_us(period_us);
}

void bme68x_port_init_i2c(struct bme68x_dev *device,
                          bme68x_port_i2c_context_t *context,
                          uint8_t i2c_address)
{
  memset(device, 0, sizeof(*device));
  context->i2c_address = i2c_address;

  device->intf = BME68X_I2C_INTF;
  device->intf_ptr = context;
  device->read = bme68x_port_i2c_read;
  device->write = bme68x_port_i2c_write;
  device->delay_us = bme68x_port_delay_us;
  device->amb_temp = 25;
}

const char *bme68x_port_status_name(int8_t status)
{
  switch (status) {
    case BME68X_OK:
      return "ok";
    case BME68X_E_NULL_PTR:
      return "null-ptr";
    case BME68X_E_COM_FAIL:
      return "com-fail";
    case BME68X_E_DEV_NOT_FOUND:
      return "dev-not-found";
    case BME68X_E_INVALID_LENGTH:
      return "invalid-length";
    case BME68X_E_SELF_TEST:
      return "self-test";
    case BME68X_W_DEFINE_OP_MODE:
      return "warn-op-mode";
    case BME68X_W_NO_NEW_DATA:
      return "warn-no-data";
    case BME68X_W_DEFINE_SHD_HEATR_DUR:
      return "warn-shared-heater";
    default:
      return "unknown";
  }
}
