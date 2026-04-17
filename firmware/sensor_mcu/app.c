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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "bme68x.h"
#include "bsec_iaq.h"
#include "bsec_interface.h"
#include "debug_console.h"
#include "config/pin_config.h"
#include "em_cmu.h"
#include "em_device.h"
#include "em_gpio.h"
#include "em_i2c.h"

#define I2C_PROBE_FREQ_HZ         100000U
#define I2C_TRANSFER_TIMEOUT      30000U
#define BME68X_SAMPLE_ATTEMPTS    3U
#define BME68X_HEATER_TEMP_C      300U
#define BME68X_HEATER_DUR_MS      100U
#define BSEC_SAMPLE_ATTEMPTS      4U

#define BSEC_PROCESS_SENSOR(sensor_id)  (1UL << ((sensor_id) - 1U))

static uint8_t bme68x_i2c_addr = BME68X_I2C_ADDR_LOW;

struct bsec_compare_results {
  bool raw_temp_valid;
  bool raw_hum_valid;
  bool comp_temp_valid;
  bool comp_hum_valid;
  int32_t raw_temp_x100;
  int32_t raw_hum_x1000;
  int32_t comp_temp_x100;
  int32_t comp_hum_x1000;
};

static uint8_t bsec_work_buffer[BSEC_MAX_WORKBUFFER_SIZE];
static bsec_input_t bsec_inputs[BSEC_MAX_PHYSICAL_SENSOR];
static bsec_output_t bsec_outputs[BSEC_NUMBER_OUTPUTS];

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

static I2C_TransferReturn_TypeDef i2c_write(uint8_t address, const uint8_t *data, size_t len)
{
  I2C_TransferSeq_TypeDef seq = { 0 };

  seq.addr = (uint16_t)address << 1;
  seq.flags = I2C_FLAG_WRITE;
  seq.buf[0].data = (uint8_t *)data;
  seq.buf[0].len = (uint16_t)len;

  return i2c_run_transfer(&seq);
}

static I2C_TransferReturn_TypeDef i2c_write_then_read(uint8_t address,
                                                      const uint8_t *tx_data,
                                                      size_t tx_len,
                                                      uint8_t *rx_data,
                                                      size_t rx_len)
{
  I2C_TransferSeq_TypeDef seq = { 0 };

  seq.addr = (uint16_t)address << 1;
  seq.flags = I2C_FLAG_WRITE_READ;
  seq.buf[0].data = (uint8_t *)tx_data;
  seq.buf[0].len = (uint16_t)tx_len;
  seq.buf[1].data = rx_data;
  seq.buf[1].len = (uint16_t)rx_len;

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

static void delay_us_busy(uint32_t period_us)
{
  uint32_t start_cycles;
  uint32_t wait_cycles;

  if (period_us == 0U) {
    return;
  }

  wait_cycles = (SystemCoreClock / 1000000U) * period_us;
  if (wait_cycles == 0U) {
    wait_cycles = 1U;
  }

  start_cycles = DWT->CYCCNT;
  while ((uint32_t)(DWT->CYCCNT - start_cycles) < wait_cycles) {
  }
}

static BME68X_INTF_RET_TYPE bme68x_platform_read(uint8_t reg_addr,
                                                 uint8_t *reg_data,
                                                 uint32_t length,
                                                 void *intf_ptr)
{
  const uint8_t address = *(const uint8_t *)intf_ptr;
  I2C_TransferReturn_TypeDef status;

  status = i2c_write_then_read(address, &reg_addr, 1U, reg_data, length);
  return (status == i2cTransferDone) ? BME68X_INTF_RET_SUCCESS : (BME68X_INTF_RET_TYPE)status;
}

static BME68X_INTF_RET_TYPE bme68x_platform_write(uint8_t reg_addr,
                                                  const uint8_t *reg_data,
                                                  uint32_t length,
                                                  void *intf_ptr)
{
  const uint8_t address = *(const uint8_t *)intf_ptr;
  I2C_TransferReturn_TypeDef status;
  I2C_TransferSeq_TypeDef seq = { 0 };

  seq.addr = (uint16_t)address << 1;
  seq.flags = I2C_FLAG_WRITE_WRITE;
  seq.buf[0].data = &reg_addr;
  seq.buf[0].len = 1U;
  seq.buf[1].data = (uint8_t *)reg_data;
  seq.buf[1].len = (uint16_t)length;

  status = i2c_run_transfer(&seq);
  return (status == i2cTransferDone) ? BME68X_INTF_RET_SUCCESS : (BME68X_INTF_RET_TYPE)status;
}

static void bme68x_platform_delay_us(uint32_t period, void *intf_ptr)
{
  (void)intf_ptr;
  delay_us_busy(period);
}

static int64_t monotonic_time_ns(void)
{
  uint64_t cycles = DWT->CYCCNT;

  return (int64_t)((cycles * 1000000000ULL) / SystemCoreClock);
}

static void delay_until_ns(int64_t target_ns)
{
  while (true) {
    int64_t now_ns = monotonic_time_ns();
    uint32_t remaining_us;

    if (now_ns >= target_ns) {
      break;
    }

    remaining_us = (uint32_t)((target_ns - now_ns) / 1000LL);
    if (remaining_us == 0U) {
      remaining_us = 1U;
    } else if (remaining_us > 5000U) {
      remaining_us = 5000U;
    }

    delay_us_busy(remaining_us);
  }
}

static int32_t float_to_scaled(float value, int32_t scale)
{
  float scaled = value * (float)scale;

  if (scaled >= 0.0f) {
    return (int32_t)(scaled + 0.5f);
  }

  return (int32_t)(scaled - 0.5f);
}

static void log_temp_humidity_scaled(const char *prefix, int32_t temp_x100, int32_t hum_x1000)
{
  uint32_t abs_temp = (temp_x100 < 0) ? (uint32_t)(-temp_x100) : (uint32_t)temp_x100;
  uint32_t abs_hum = (hum_x1000 < 0) ? (uint32_t)(-hum_x1000) : (uint32_t)hum_x1000;

  printf("%s T=%s%lu.%02luC RH=%s%lu.%03lu%%\r\n",
         prefix,
         (temp_x100 < 0) ? "-" : "",
         (unsigned long)(abs_temp / 100U),
         (unsigned long)(abs_temp % 100U),
         (hum_x1000 < 0) ? "-" : "",
         (unsigned long)(abs_hum / 1000U),
         (unsigned long)(abs_hum % 1000U));
}

static bool bsec_outputs_ready(const struct bsec_compare_results *results)
{
  return results->raw_temp_valid
         && results->raw_hum_valid
         && results->comp_temp_valid
         && results->comp_hum_valid;
}

static void bsec_capture_outputs(const bsec_output_t *outputs,
                                 uint8_t n_outputs,
                                 struct bsec_compare_results *results)
{
  for (uint8_t i = 0U; i < n_outputs; i++) {
    switch (outputs[i].sensor_id) {
      case BSEC_OUTPUT_RAW_TEMPERATURE:
        results->raw_temp_x100 = float_to_scaled(outputs[i].signal, 100);
        results->raw_temp_valid = true;
        break;
      case BSEC_OUTPUT_RAW_HUMIDITY:
        results->raw_hum_x1000 = float_to_scaled(outputs[i].signal, 1000);
        results->raw_hum_valid = true;
        break;
      case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
        results->comp_temp_x100 = float_to_scaled(outputs[i].signal, 100);
        results->comp_temp_valid = true;
        break;
      case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
        results->comp_hum_x1000 = float_to_scaled(outputs[i].signal, 1000);
        results->comp_hum_valid = true;
        break;
      default:
        break;
    }
  }
}

static bsec_library_return_t bsec_subscribe_temp_humidity(void)
{
  bsec_sensor_configuration_t requested_virtual_sensors[4] = { 0 };
  bsec_sensor_configuration_t required_sensor_settings[BSEC_MAX_PHYSICAL_SENSOR] = { 0 };
  uint8_t n_required_sensor_settings = BSEC_MAX_PHYSICAL_SENSOR;

  requested_virtual_sensors[0].sensor_id = BSEC_OUTPUT_RAW_TEMPERATURE;
  requested_virtual_sensors[0].sample_rate = BSEC_SAMPLE_RATE_LP;
  requested_virtual_sensors[1].sensor_id = BSEC_OUTPUT_RAW_HUMIDITY;
  requested_virtual_sensors[1].sample_rate = BSEC_SAMPLE_RATE_LP;
  requested_virtual_sensors[2].sensor_id = BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE;
  requested_virtual_sensors[2].sample_rate = BSEC_SAMPLE_RATE_LP;
  requested_virtual_sensors[3].sensor_id = BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY;
  requested_virtual_sensors[3].sample_rate = BSEC_SAMPLE_RATE_LP;

  return bsec_update_subscription(requested_virtual_sensors,
                                  4U,
                                  required_sensor_settings,
                                  &n_required_sensor_settings);
}

static uint8_t bsec_prepare_inputs(const struct bme68x_data *data,
                                   uint32_t process_data,
                                   int64_t timestamp_ns,
                                   bsec_input_t *inputs)
{
  uint8_t n_inputs = 0U;

  if ((process_data & BSEC_PROCESS_SENSOR(BSEC_INPUT_HEATSOURCE)) != 0U) {
    inputs[n_inputs].sensor_id = BSEC_INPUT_HEATSOURCE;
    inputs[n_inputs].signal = 0.0f;
    inputs[n_inputs].time_stamp = timestamp_ns;
    inputs[n_inputs].signal_dimensions = 1U;
    n_inputs++;
  }
  if ((process_data & BSEC_PROCESS_TEMPERATURE) != 0U) {
    inputs[n_inputs].sensor_id = BSEC_INPUT_TEMPERATURE;
    inputs[n_inputs].signal = (float)data->temperature / 100.0f;
    inputs[n_inputs].time_stamp = timestamp_ns;
    inputs[n_inputs].signal_dimensions = 1U;
    n_inputs++;
  }
  if ((process_data & BSEC_PROCESS_HUMIDITY) != 0U) {
    inputs[n_inputs].sensor_id = BSEC_INPUT_HUMIDITY;
    inputs[n_inputs].signal = (float)data->humidity / 1000.0f;
    inputs[n_inputs].time_stamp = timestamp_ns;
    inputs[n_inputs].signal_dimensions = 1U;
    n_inputs++;
  }
  if ((process_data & BSEC_PROCESS_PRESSURE) != 0U) {
    inputs[n_inputs].sensor_id = BSEC_INPUT_PRESSURE;
    inputs[n_inputs].signal = (float)data->pressure;
    inputs[n_inputs].time_stamp = timestamp_ns;
    inputs[n_inputs].signal_dimensions = 1U;
    n_inputs++;
  }
  if (((process_data & BSEC_PROCESS_GAS) != 0U)
      && ((data->status & BME68X_GASM_VALID_MSK) != 0U)) {
    inputs[n_inputs].sensor_id = BSEC_INPUT_GASRESISTOR;
    inputs[n_inputs].signal = (float)data->gas_resistance;
    inputs[n_inputs].time_stamp = timestamp_ns;
    inputs[n_inputs].signal_dimensions = 1U;
    n_inputs++;
  }
  if (((process_data & BSEC_PROCESS_PROFILE_PART) != 0U)
      && ((data->status & BME68X_GASM_VALID_MSK) != 0U)) {
    inputs[n_inputs].sensor_id = BSEC_INPUT_PROFILE_PART;
    inputs[n_inputs].signal = 0.0f;
    inputs[n_inputs].time_stamp = timestamp_ns;
    inputs[n_inputs].signal_dimensions = 1U;
    n_inputs++;
  }

  return n_inputs;
}

static int8_t bme68x_run_bsec_measurement(struct bme68x_dev *dev,
                                          const bsec_bme_settings_t *sensor_settings,
                                          struct bme68x_data *data,
                                          uint8_t *n_fields)
{
  struct bme68x_conf conf = { 0 };
  struct bme68x_heatr_conf heatr_conf = { 0 };
  int8_t rslt;
  uint32_t delay_us;

  rslt = bme68x_get_conf(&conf, dev);
  if (rslt != BME68X_OK) {
    return rslt;
  }

  conf.os_hum = sensor_settings->humidity_oversampling;
  conf.os_temp = sensor_settings->temperature_oversampling;
  conf.os_pres = sensor_settings->pressure_oversampling;
  rslt = bme68x_set_conf(&conf, dev);
  if (rslt != BME68X_OK) {
    return rslt;
  }

  heatr_conf.enable = sensor_settings->run_gas ? BME68X_ENABLE : BME68X_DISABLE;
  heatr_conf.heatr_temp = sensor_settings->heater_temperature;
  heatr_conf.heatr_dur = sensor_settings->heater_duration;
  rslt = bme68x_set_heatr_conf(BME68X_FORCED_MODE, &heatr_conf, dev);
  if (rslt != BME68X_OK) {
    return rslt;
  }

  rslt = bme68x_set_op_mode(BME68X_FORCED_MODE, dev);
  if (rslt != BME68X_OK) {
    return rslt;
  }

  delay_us = bme68x_get_meas_dur(BME68X_FORCED_MODE, &conf, dev);
  if (sensor_settings->run_gas != 0U) {
    delay_us += (uint32_t)sensor_settings->heater_duration * 1000U;
  }
  dev->delay_us(delay_us + 1000U, dev->intf_ptr);

  return bme68x_get_data(BME68X_FORCED_MODE, data, n_fields, dev);
}

static void bsec_compare_sensor_readings(void)
{
  struct bme68x_dev dev = { 0 };
  struct bme68x_data data = { 0 };
  struct bsec_compare_results results = { 0 };
  bsec_version_t version = { 0 };
  bsec_bme_settings_t sensor_settings = { 0 };
  uint8_t n_fields = 0U;
  int8_t bme_rslt;
  bsec_library_return_t bsec_rslt;

  puts("BSEC_TEST: start");

  if (!i2c_probe_address(bme68x_i2c_addr)) {
    puts("BSEC_TEST: sensor not responding at 0x76");
    puts("BSEC_TEST: done");
    return;
  }

  dev.intf = BME68X_I2C_INTF;
  dev.intf_ptr = &bme68x_i2c_addr;
  dev.read = bme68x_platform_read;
  dev.write = bme68x_platform_write;
  dev.delay_us = bme68x_platform_delay_us;
  dev.amb_temp = 25;

  bme_rslt = bme68x_init(&dev);
  if (bme_rslt != BME68X_OK) {
    printf("BSEC_TEST: bme init failed rslt=%d\r\n", (int)bme_rslt);
    puts("BSEC_TEST: done");
    return;
  }

  bsec_rslt = bsec_get_version(&version);
  if (bsec_rslt == BSEC_OK) {
    printf("BSEC_TEST: version=%u.%u.%u.%u\r\n",
           (unsigned int)version.major,
           (unsigned int)version.minor,
           (unsigned int)version.major_bugfix,
           (unsigned int)version.minor_bugfix);
  }

  bsec_rslt = bsec_init();
  if (bsec_rslt != BSEC_OK) {
    printf("BSEC_TEST: bsec init failed rslt=%d\r\n", (int)bsec_rslt);
    puts("BSEC_TEST: done");
    return;
  }

  bsec_rslt = bsec_set_configuration(bsec_config_iaq,
                                     sizeof(bsec_config_iaq),
                                     bsec_work_buffer,
                                     sizeof(bsec_work_buffer));
  if (bsec_rslt != BSEC_OK) {
    printf("BSEC_TEST: config failed rslt=%d\r\n", (int)bsec_rslt);
    puts("BSEC_TEST: done");
    return;
  }

  bsec_rslt = bsec_subscribe_temp_humidity();
  if (bsec_rslt != BSEC_OK) {
    printf("BSEC_TEST: subscribe failed rslt=%d\r\n", (int)bsec_rslt);
    puts("BSEC_TEST: done");
    return;
  }

  for (uint32_t attempt = 1U; attempt <= BSEC_SAMPLE_ATTEMPTS; attempt++) {
    int64_t time_ns = monotonic_time_ns();
    uint8_t n_inputs;
    uint8_t n_outputs = BSEC_NUMBER_OUTPUTS;

    bsec_rslt = bsec_sensor_control(time_ns, &sensor_settings);
    if (bsec_rslt < BSEC_OK) {
      printf("BSEC_TEST: sensor_control failed rslt=%d\r\n", (int)bsec_rslt);
      puts("BSEC_TEST: done");
      return;
    }

    if ((sensor_settings.trigger_measurement == 0U)
        || (sensor_settings.op_mode == BME68X_SLEEP_MODE)) {
      printf("BSEC_TEST: wait attempt=%lu next_call_ns=%ld\r\n",
             (unsigned long)attempt,
             (long)sensor_settings.next_call);
      delay_until_ns(sensor_settings.next_call);
      continue;
    }

    bme_rslt = bme68x_run_bsec_measurement(&dev, &sensor_settings, &data, &n_fields);
    if ((bme_rslt != BME68X_OK) || (n_fields == 0U)) {
      printf("BSEC_TEST: sample failed attempt=%lu bme=%d fields=%u\r\n",
             (unsigned long)attempt,
             (int)bme_rslt,
             (unsigned int)n_fields);
      delay_until_ns(sensor_settings.next_call);
      continue;
    }

    time_ns = monotonic_time_ns();
    n_inputs = bsec_prepare_inputs(&data, sensor_settings.process_data, time_ns, bsec_inputs);
    bsec_rslt = bsec_do_steps(bsec_inputs, n_inputs, bsec_outputs, &n_outputs);
    if (bsec_rslt < BSEC_OK) {
      printf("BSEC_TEST: do_steps failed rslt=%d\r\n", (int)bsec_rslt);
      puts("BSEC_TEST: done");
      return;
    }

    bsec_capture_outputs(bsec_outputs, n_outputs, &results);

    if (bsec_outputs_ready(&results)) {
      log_temp_humidity_scaled("BSEC_TEST: direct", data.temperature, (int32_t)data.humidity);
      log_temp_humidity_scaled("BSEC_TEST: bsec raw", results.raw_temp_x100, results.raw_hum_x1000);
      log_temp_humidity_scaled("BSEC_TEST: bsec cmp", results.comp_temp_x100, results.comp_hum_x1000);
      log_temp_humidity_scaled("BSEC_TEST: delta raw",
                               results.raw_temp_x100 - data.temperature,
                               results.raw_hum_x1000 - (int32_t)data.humidity);
      log_temp_humidity_scaled("BSEC_TEST: delta cmp",
                               results.comp_temp_x100 - data.temperature,
                               results.comp_hum_x1000 - (int32_t)data.humidity);
      puts("BSEC_TEST: done");
      return;
    }

    printf("BSEC_TEST: partial attempt=%lu outputs=%u status=0x%02X\r\n",
           (unsigned long)attempt,
           (unsigned int)n_outputs,
           data.status);
    delay_until_ns(sensor_settings.next_call);
  }

  puts("BSEC_TEST: no comparison output");
  puts("BSEC_TEST: done");
}

static void log_bme68x_status(uint8_t status)
{
  printf("BME68X_TEST: status new=%u gas_valid=%u heat_stable=%u raw=0x%02X\r\n",
         (unsigned int)((status & BME68X_NEW_DATA_MSK) != 0U),
         (unsigned int)((status & BME68X_GASM_VALID_MSK) != 0U),
         (unsigned int)((status & BME68X_HEAT_STAB_MSK) != 0U),
         status);
}

static void log_bme68x_data(const struct bme68x_data *data)
{
  int32_t temp_centi = data->temperature;
  uint32_t temp_abs = (temp_centi < 0) ? (uint32_t)(-temp_centi) : (uint32_t)temp_centi;

  printf("BME68X_TEST: T=%ld.%02luC P=%luPa H=%lu.%03lu%% Gas=%luOhm\r\n",
         (long)(temp_centi / 100),
         (unsigned long)(temp_abs % 100U),
         (unsigned long)data->pressure,
         (unsigned long)(data->humidity / 1000U),
         (unsigned long)(data->humidity % 1000U),
         (unsigned long)data->gas_resistance);
}

static int8_t bme68x_trigger_forced_sample(struct bme68x_dev *dev,
                                           const struct bme68x_conf *conf,
                                           struct bme68x_data *data,
                                           uint8_t *n_fields)
{
  int8_t rslt;
  uint32_t delay_us;

  rslt = bme68x_set_op_mode(BME68X_FORCED_MODE, dev);
  if (rslt != BME68X_OK) {
    return rslt;
  }

  delay_us = bme68x_get_meas_dur(BME68X_FORCED_MODE, (struct bme68x_conf *)conf, dev);
  delay_us += (uint32_t)BME68X_HEATER_DUR_MS * 1000U;
  dev->delay_us(delay_us, dev->intf_ptr);

  return bme68x_get_data(BME68X_FORCED_MODE, data, n_fields, dev);
}

static void bme68x_test_sensor(void)
{
  struct bme68x_dev dev = { 0 };
  struct bme68x_conf conf = { 0 };
  struct bme68x_heatr_conf heatr_conf = { 0 };
  struct bme68x_data data = { 0 };
  uint8_t n_fields = 0U;
  int8_t rslt;

  if (!i2c_probe_address(bme68x_i2c_addr)) {
    puts("BME68X_TEST: sensor not responding at 0x76");
    puts("BME68X_TEST: done");
    return;
  }

  dev.intf = BME68X_I2C_INTF;
  dev.intf_ptr = &bme68x_i2c_addr;
  dev.read = bme68x_platform_read;
  dev.write = bme68x_platform_write;
  dev.delay_us = bme68x_platform_delay_us;
  dev.amb_temp = 25;

  rslt = bme68x_init(&dev);
  if (rslt != BME68X_OK) {
    printf("BME68X_TEST: init failed rslt=%d\r\n", (int)rslt);
    puts("BME68X_TEST: done");
    return;
  }

  printf("BME68X_TEST: chip=0x%02X variant=%lu\r\n",
         dev.chip_id,
         (unsigned long)dev.variant_id);

  conf.os_hum = BME68X_OS_2X;
  conf.os_temp = BME68X_OS_4X;
  conf.os_pres = BME68X_OS_4X;
  conf.filter = BME68X_FILTER_OFF;
  conf.odr = BME68X_ODR_NONE;

  heatr_conf.enable = BME68X_ENABLE;
  heatr_conf.heatr_temp = BME68X_HEATER_TEMP_C;
  heatr_conf.heatr_dur = BME68X_HEATER_DUR_MS;

  rslt = bme68x_set_conf(&conf, &dev);
  if (rslt == BME68X_OK) {
    rslt = bme68x_set_heatr_conf(BME68X_FORCED_MODE, &heatr_conf, &dev);
  }
  if (rslt != BME68X_OK) {
    printf("BME68X_TEST: config failed rslt=%d\r\n", (int)rslt);
    puts("BME68X_TEST: done");
    return;
  }

  for (uint32_t attempt = 1U; attempt <= BME68X_SAMPLE_ATTEMPTS; attempt++) {
    rslt = bme68x_trigger_forced_sample(&dev, &conf, &data, &n_fields);
    if (rslt == BME68X_OK && n_fields > 0U) {
      printf("BME68X_TEST: sample attempt=%lu fields=%u\r\n",
             (unsigned long)attempt,
             (unsigned int)n_fields);
      log_bme68x_status(data.status);
      log_bme68x_data(&data);
      puts("BME68X_TEST: done");
      return;
    }

    printf("BME68X_TEST: sample attempt=%lu rslt=%d fields=%u\r\n",
           (unsigned long)attempt,
           (int)rslt,
           (unsigned int)n_fields);
  }

  puts("BME68X_TEST: no valid sample");
  puts("BME68X_TEST: done");
}

static void i2c_scan_expected_addresses(void)
{
  bool found_76;
  bool found_77;

  i2c_bus_init();
  printf("I2C_SCAN: init bus=I2C1 scl=PD02 sda=PD03 freq=%lu\r\n",
         (unsigned long)I2C_PROBE_FREQ_HZ);

  found_76 = i2c_probe_address(0x76U);
  found_77 = i2c_probe_address(0x77U);

  if (!found_76 && !found_77) {
    puts("I2C_SCAN: no devices detected at 0x76 or 0x77");
  }

  puts("I2C_SCAN: done");
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
  i2c_scan_expected_addresses();
  bme68x_test_sensor();
  bsec_compare_sensor_readings();
}

/***************************************************************************//**
 * App ticking function.
 ******************************************************************************/
void app_process_action(void)
{
}
