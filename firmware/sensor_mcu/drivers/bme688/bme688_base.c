#include "drivers/bme688/bme688_base.h"

#include <string.h>

#include "platform/monotonic_time.h"

#define BME688_BASE_EXTRA_WAIT_US  1000U

static void bme688_base_populate_sample(const struct bme68x_data *data,
                                        bme688_base_sample_t *sample)
{
  sample->temperature_c = data->temperature;
  sample->humidity_percent = data->humidity;
  sample->pressure_pa = data->pressure;
  sample->gas_resistance_ohms = data->gas_resistance;
  sample->gas_index = data->gas_index;
  sample->measurement_index = data->meas_index;
  sample->status = data->status;
  sample->timestamp_ns = monotonic_time_now_ns();
}

int8_t bme688_base_init(bme688_base_t *sensor, uint8_t i2c_address)
{
  int8_t result;

  memset(sensor, 0, sizeof(*sensor));
  sensor->i2c_address = i2c_address;

  bme68x_port_init_i2c(&sensor->device, &sensor->port_context, i2c_address);
  result = bme68x_init(&sensor->device);
  if (result == BME68X_OK) {
    sensor->initialized = true;
  }

  return result;
}

int8_t bme688_base_measure_forced(bme688_base_t *sensor,
                                  const bme688_base_measurement_request_t *request,
                                  bme688_base_sample_t *sample)
{
  struct bme68x_conf configuration = { 0 };
  struct bme68x_heatr_conf heater_configuration = { 0 };
  struct bme68x_data raw_data = { 0 };
  uint32_t measurement_duration_us;
  uint8_t field_count = 0U;
  int8_t result;

  if ((!sensor->initialized) || (request == NULL) || (sample == NULL)) {
    return BME68X_E_NULL_PTR;
  }

  configuration.os_temp = request->temperature_oversampling;
  configuration.os_hum = request->humidity_oversampling;
  configuration.os_pres = request->pressure_oversampling;
  configuration.filter = request->filter;
  configuration.odr = BME68X_ODR_NONE;

  heater_configuration.enable = request->run_gas ? BME68X_ENABLE : BME68X_DISABLE;
  heater_configuration.heatr_temp = request->heater_temperature_c;
  heater_configuration.heatr_dur = request->heater_duration_ms;

  result = bme68x_set_conf(&configuration, &sensor->device);
  if (result != BME68X_OK) {
    return result;
  }

  result = bme68x_set_heatr_conf(BME68X_FORCED_MODE, &heater_configuration, &sensor->device);
  if (result != BME68X_OK) {
    return result;
  }

  result = bme68x_set_op_mode(BME68X_FORCED_MODE, &sensor->device);
  if (result != BME68X_OK) {
    return result;
  }

  measurement_duration_us = bme68x_get_meas_dur(BME68X_FORCED_MODE,
                                                &configuration,
                                                &sensor->device);
  measurement_duration_us += ((uint32_t)request->heater_duration_ms * 1000U) + BME688_BASE_EXTRA_WAIT_US;
  monotonic_time_delay_us(measurement_duration_us);

  result = bme68x_get_data(BME68X_FORCED_MODE, &raw_data, &field_count, &sensor->device);
  if ((result != BME68X_OK) || (field_count == 0U)) {
    return (field_count == 0U) ? BME68X_W_NO_NEW_DATA : result;
  }

  bme688_base_populate_sample(&raw_data, sample);
  return BME68X_OK;
}

const char *bme688_base_status_name(int8_t status)
{
  return bme68x_port_status_name(status);
}
