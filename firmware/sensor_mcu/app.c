/***************************************************************************//**
 * @file
 * @brief Top level application functions
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#include "app.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "config/pin_config.h"
#include "debug_console.h"
#include "em_cmu.h"
#include "em_device.h"
#include "em_gpio.h"
#include "em_i2c.h"
#include "sensirion_common.h"
#include "sensirion_i2c.h"
#include "sensirion_i2c_hal.h"
#include "sps30_i2c.h"

#define I2C_PROBE_FREQ_HZ                    100000U
#define I2C_TRANSFER_TIMEOUT                 30000U
#define SPS30_I2C_ADDRESS                    SPS30_I2C_ADDR_69
#define SPS30_PRODUCT_TYPE_BUFFER_SIZE       9U
#define SPS30_SERIAL_NUMBER_BUFFER_SIZE      33U
#define SPS30_STARTUP_DELAY_US               100000U
#define SPS30_READY_POLL_DELAY_US            1000000U
#define SPS30_READY_POLL_ATTEMPTS            10U

struct sps30_measurement {
  float mc_1p0;
  float mc_2p5;
  float mc_4p0;
  float mc_10p0;
  float nc_0p5;
  float nc_1p0;
  float nc_2p5;
  float nc_4p0;
  float nc_10p0;
  float typical_particle_size;
};

static void cycle_counter_enable(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_us_busy(uint32_t period_us)
{
  uint32_t start_cycles;
  uint32_t wait_cycles;

  if (period_us == 0U) {
    return;
  }

  cycle_counter_enable();

  wait_cycles = (SystemCoreClock / 1000000U) * period_us;
  if (wait_cycles == 0U) {
    wait_cycles = 1U;
  }

  start_cycles = DWT->CYCCNT;
  while ((uint32_t)(DWT->CYCCNT - start_cycles) < wait_cycles) {
  }
}

static void i2c_bus_init(void)
{
  I2C_Init_TypeDef init = I2C_INIT_DEFAULT;

  CMU_ClockEnable(cmuClock_GPIO, true);
  CMU_ClockEnable(SL_I2C_BUS_CLOCK, true);

  GPIO_PinModeSet((GPIO_Port_TypeDef)SL_I2C_BUS_SCL_PORT,
                  SL_I2C_BUS_SCL_PIN,
                  gpioModeWiredAndPullUp,
                  1);
  GPIO_PinModeSet((GPIO_Port_TypeDef)SL_I2C_BUS_SDA_PORT,
                  SL_I2C_BUS_SDA_PIN,
                  gpioModeWiredAndPullUp,
                  1);

  GPIO->I2CROUTE[SL_I2C_BUS_PERIPHERAL_NO].SCLROUTE =
    ((uint32_t)SL_I2C_BUS_SCL_PORT << _GPIO_I2C_SCLROUTE_PORT_SHIFT)
    | ((uint32_t)SL_I2C_BUS_SCL_PIN << _GPIO_I2C_SCLROUTE_PIN_SHIFT);
  GPIO->I2CROUTE[SL_I2C_BUS_PERIPHERAL_NO].SDAROUTE =
    ((uint32_t)SL_I2C_BUS_SDA_PORT << _GPIO_I2C_SDAROUTE_PORT_SHIFT)
    | ((uint32_t)SL_I2C_BUS_SDA_PIN << _GPIO_I2C_SDAROUTE_PIN_SHIFT);
  GPIO->I2CROUTE[SL_I2C_BUS_PERIPHERAL_NO].ROUTEEN =
    GPIO_I2C_ROUTEEN_SCLPEN | GPIO_I2C_ROUTEEN_SDAPEN;

  I2C_Reset(SL_I2C_BUS_PERIPHERAL);

  init.freq = I2C_PROBE_FREQ_HZ;
  I2C_Init(SL_I2C_BUS_PERIPHERAL, &init);
}

static I2C_TransferReturn_TypeDef i2c_run_transfer(I2C_TransferSeq_TypeDef *seq)
{
  I2C_TransferReturn_TypeDef status;
  uint32_t timeout = I2C_TRANSFER_TIMEOUT;

  status = I2C_TransferInit(SL_I2C_BUS_PERIPHERAL, seq);
  while ((status == i2cTransferInProgress) && (timeout > 0U)) {
    status = I2C_Transfer(SL_I2C_BUS_PERIPHERAL);
    timeout--;
  }

  if ((status == i2cTransferInProgress) && (timeout == 0U)) {
    I2C_Reset(SL_I2C_BUS_PERIPHERAL);
    i2c_bus_init();
    return i2cTransferSwFault;
  }

  return status;
}

static I2C_TransferReturn_TypeDef i2c_write(uint8_t address,
                                            const uint8_t *data,
                                            size_t len)
{
  I2C_TransferSeq_TypeDef seq = { 0 };

  seq.addr = (uint16_t)address << 1;
  seq.flags = I2C_FLAG_WRITE;
  seq.buf[0].data = (uint8_t *)data;
  seq.buf[0].len = (uint16_t)len;

  return i2c_run_transfer(&seq);
}

static I2C_TransferReturn_TypeDef i2c_read(uint8_t address,
                                           uint8_t *data,
                                           size_t len)
{
  I2C_TransferSeq_TypeDef seq = { 0 };

  seq.addr = (uint16_t)address << 1;
  seq.flags = I2C_FLAG_READ;
  seq.buf[0].data = data;
  seq.buf[0].len = (uint16_t)len;

  return i2c_run_transfer(&seq);
}

static bool i2c_probe_address(uint8_t address)
{
  I2C_TransferReturn_TypeDef status;

  status = i2c_write(address, NULL, 0U);
  if (status == i2cTransferDone) {
    printf("I2C_SCAN: found addr=0x%02X\r\n", address);
    return true;
  }

  printf("I2C_SCAN: miss addr=0x%02X status=%d\r\n", address, (int)status);
  return false;
}

static int8_t i2c_status_to_sensirion_error(I2C_TransferReturn_TypeDef status)
{
  if (status == i2cTransferDone) {
    return NO_ERROR;
  }

  if (status == i2cTransferNack) {
    return I2C_NACK_ERROR;
  }

  return I2C_BUS_ERROR;
}

static uint32_t float_to_scaled(float value, uint32_t scale)
{
  float scaled = value * (float)scale;

  if (scaled <= 0.0f) {
    return 0U;
  }

  return (uint32_t)(scaled + 0.5f);
}

static void log_fixed_2dp(const char *label, float value)
{
  uint32_t scaled = float_to_scaled(value, 100U);

  printf("%s=%lu.%02lu",
         label,
         (unsigned long)(scaled / 100U),
         (unsigned long)(scaled % 100U));
}

static void log_sps30_measurement(const struct sps30_measurement *measurement)
{
  printf("SPS30_TEST: mass_ug_m3 ");
  log_fixed_2dp("pm1p0", measurement->mc_1p0);
  printf(" ");
  log_fixed_2dp("pm2p5", measurement->mc_2p5);
  printf(" ");
  log_fixed_2dp("pm4p0", measurement->mc_4p0);
  printf(" ");
  log_fixed_2dp("pm10p0", measurement->mc_10p0);
  printf("\r\n");

  printf("SPS30_TEST: count_per_cm3 ");
  log_fixed_2dp("pm0p5", measurement->nc_0p5);
  printf(" ");
  log_fixed_2dp("pm1p0", measurement->nc_1p0);
  printf(" ");
  log_fixed_2dp("pm2p5", measurement->nc_2p5);
  printf(" ");
  log_fixed_2dp("pm4p0", measurement->nc_4p0);
  printf(" ");
  log_fixed_2dp("pm10p0", measurement->nc_10p0);
  printf("\r\n");

  printf("SPS30_TEST: particle_size_um ");
  log_fixed_2dp("typical", measurement->typical_particle_size);
  printf("\r\n");
}

static void log_sps30_error(const char *operation, int16_t error)
{
  printf("SPS30_TEST: %s failed err=%d\r\n", operation, (int)error);
}

int16_t sensirion_i2c_hal_select_bus(uint8_t bus_idx)
{
  (void)bus_idx;
  return NO_ERROR;
}

void sensirion_i2c_hal_init(void)
{
  cycle_counter_enable();
  i2c_bus_init();
}

void sensirion_i2c_hal_free(void)
{
}

int8_t sensirion_i2c_hal_read(uint8_t address, uint8_t *data, uint8_t count)
{
  I2C_TransferReturn_TypeDef status = i2c_read(address, data, count);

  return i2c_status_to_sensirion_error(status);
}

int8_t sensirion_i2c_hal_write(uint8_t address, const uint8_t *data, uint8_t count)
{
  I2C_TransferReturn_TypeDef status = i2c_write(address, data, count);

  return i2c_status_to_sensirion_error(status);
}

void sensirion_i2c_hal_sleep_usec(uint32_t useconds)
{
  delay_us_busy(useconds);
}

static void sps30_test_sensor(void)
{
  struct sps30_measurement measurement = { 0 };
  int8_t product_type[SPS30_PRODUCT_TYPE_BUFFER_SIZE] = { 0 };
  int8_t serial_number[SPS30_SERIAL_NUMBER_BUFFER_SIZE] = { 0 };
  uint8_t firmware_major = 0U;
  uint8_t firmware_minor = 0U;
  uint32_t device_status = 0U;
  uint16_t data_ready_flag = 0U;
  int16_t error;
  bool sample_captured = false;

  puts("SPS30_TEST: start");
  printf("SWO_CONSOLE: backend=%s speed=%lu\r\n",
         debug_console_backend_name(),
         (unsigned long)debug_console_swo_speed_hz());

  sensirion_i2c_hal_init();

  printf("I2C_SCAN: init bus=I2C1 scl=PD02 sda=PD03 freq=%lu\r\n",
         (unsigned long)I2C_PROBE_FREQ_HZ);
  if (!i2c_probe_address(SPS30_I2C_ADDRESS)) {
    printf("SPS30_TEST: sensor not responding at 0x%02X\r\n", SPS30_I2C_ADDRESS);
    puts("SPS30_TEST: done");
    return;
  }
  puts("I2C_SCAN: done");

  sps30_init(SPS30_I2C_ADDRESS);

  error = sps30_read_product_type(product_type, 8U);
  if (error != NO_ERROR) {
    log_sps30_error("read_product_type", error);
    puts("SPS30_TEST: done");
    return;
  }

  error = sps30_read_serial_number(serial_number, 32U);
  if (error != NO_ERROR) {
    log_sps30_error("read_serial_number", error);
    puts("SPS30_TEST: done");
    return;
  }

  error = sps30_read_firmware_version(&firmware_major, &firmware_minor);
  if (error != NO_ERROR) {
    log_sps30_error("read_firmware_version", error);
    puts("SPS30_TEST: done");
    return;
  }

  error = sps30_read_device_status_register(&device_status);
  if (error != NO_ERROR) {
    log_sps30_error("read_device_status_register", error);
    puts("SPS30_TEST: done");
    return;
  }

  printf("SPS30_TEST: product=%s serial=%s fw=%u.%u status=0x%08lX\r\n",
         (char *)product_type,
         (char *)serial_number,
         (unsigned int)firmware_major,
         (unsigned int)firmware_minor,
         (unsigned long)device_status);

  if (device_status != 0U) {
    error = sps30_clear_device_status_register();
    if (error != NO_ERROR) {
      log_sps30_error("clear_device_status_register", error);
      puts("SPS30_TEST: done");
      return;
    }

    error = sps30_read_device_status_register(&device_status);
    if (error != NO_ERROR) {
      log_sps30_error("read_device_status_register_after_clear", error);
      puts("SPS30_TEST: done");
      return;
    }

    printf("SPS30_TEST: status_after_clear=0x%08lX\r\n",
           (unsigned long)device_status);
  }

  error = sps30_start_measurement(SPS30_OUTPUT_FORMAT_OUTPUT_FORMAT_FLOAT);
  if (error != NO_ERROR) {
    log_sps30_error("start_measurement", error);
    puts("SPS30_TEST: done");
    return;
  }

  sensirion_i2c_hal_sleep_usec(SPS30_STARTUP_DELAY_US);

  for (uint32_t attempt = 1U; attempt <= SPS30_READY_POLL_ATTEMPTS; attempt++) {
    sensirion_i2c_hal_sleep_usec(SPS30_READY_POLL_DELAY_US);

    error = sps30_read_data_ready_flag(&data_ready_flag);
    if (error != NO_ERROR) {
      log_sps30_error("read_data_ready_flag", error);
      break;
    }

    printf("SPS30_TEST: ready attempt=%lu flag=%u\r\n",
           (unsigned long)attempt,
           (unsigned int)data_ready_flag);

    if (data_ready_flag == 0U) {
      continue;
    }

    error = sps30_read_measurement_values_float(&measurement.mc_1p0,
                                                &measurement.mc_2p5,
                                                &measurement.mc_4p0,
                                                &measurement.mc_10p0,
                                                &measurement.nc_0p5,
                                                &measurement.nc_1p0,
                                                &measurement.nc_2p5,
                                                &measurement.nc_4p0,
                                                &measurement.nc_10p0,
                                                &measurement.typical_particle_size);
    if (error != NO_ERROR) {
      log_sps30_error("read_measurement_values_float", error);
      break;
    }

    log_sps30_measurement(&measurement);
    sample_captured = true;
    break;
  }

  error = sps30_stop_measurement();
  if (error != NO_ERROR) {
    log_sps30_error("stop_measurement", error);
  }

  if (!sample_captured) {
    puts("SPS30_TEST: no measurement captured");
  }

  puts("SPS30_TEST: done");
}

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/
void app_init_early(void)
{
  debug_console_init();
  puts(SWO_SELF_TEST_TOKEN " phase=EARLY");
}

void app_init(void)
{
  sps30_test_sensor();
}

/***************************************************************************//**
 * App ticking function.
 ******************************************************************************/
void app_process_action(void)
{
}
