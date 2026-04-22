#include "services/air_quality/bsec_service.h"

#include <stddef.h>
#include <string.h>

#include "platform/monotonic_time.h"

#define BSEC_SERVICE_HEATSOURCE_C            0.466f
#define BSEC_SERVICE_MAX_INPUTS              6U
#define BSEC_SERVICE_MAX_REQUESTED_OUTPUTS   9U

static bool bsec_service_has_valid_gas_data(const bme688_base_sample_t *sample)
{
  return (sample->status & BME68X_GASM_VALID_MSK) != 0U;
}

static bsec_library_return_t bsec_service_subscribe_outputs(void)
{
  bsec_sensor_configuration_t requested_outputs[BSEC_SERVICE_MAX_REQUESTED_OUTPUTS] = {
    { .sample_rate = BSEC_SAMPLE_RATE_ULP, .sensor_id = BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE },
    { .sample_rate = BSEC_SAMPLE_RATE_ULP, .sensor_id = BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY },
    { .sample_rate = BSEC_SAMPLE_RATE_ULP, .sensor_id = BSEC_OUTPUT_BREATH_VOC_EQUIVALENT },
    { .sample_rate = BSEC_SAMPLE_RATE_ULP, .sensor_id = BSEC_OUTPUT_IAQ },
    { .sample_rate = BSEC_SAMPLE_RATE_ULP, .sensor_id = BSEC_OUTPUT_CO2_EQUIVALENT },
    { .sample_rate = BSEC_SAMPLE_RATE_ULP, .sensor_id = BSEC_OUTPUT_RAW_GAS },
    { .sample_rate = BSEC_SAMPLE_RATE_ULP, .sensor_id = BSEC_OUTPUT_GAS_PERCENTAGE },
    { .sample_rate = BSEC_SAMPLE_RATE_ULP, .sensor_id = BSEC_OUTPUT_STABILIZATION_STATUS },
    { .sample_rate = BSEC_SAMPLE_RATE_ULP, .sensor_id = BSEC_OUTPUT_RUN_IN_STATUS },
  };
  bsec_sensor_configuration_t required_inputs[BSEC_MAX_PHYSICAL_SENSOR] = { 0 };
  uint8_t required_input_count = BSEC_MAX_PHYSICAL_SENSOR;

  return bsec_update_subscription(requested_outputs,
                                  BSEC_SERVICE_MAX_REQUESTED_OUTPUTS,
                                  required_inputs,
                                  &required_input_count);
}

static bool bsec_service_append_input(bsec_input_t *inputs,
                                      uint8_t *input_count,
                                      uint8_t sensor_id,
                                      float signal,
                                      int64_t timestamp_ns)
{
  if (*input_count >= BSEC_SERVICE_MAX_INPUTS) {
    return false;
  }

  inputs[*input_count].sensor_id = sensor_id;
  inputs[*input_count].signal = signal;
  inputs[*input_count].signal_dimensions = 1U;
  inputs[*input_count].time_stamp = timestamp_ns;
  (*input_count)++;

  return true;
}

static bool bsec_service_build_inputs(const bsec_bme_settings_t *settings,
                                      const bme688_base_sample_t *sample,
                                      bsec_input_t *inputs,
                                      uint8_t *input_count)
{
  *input_count = 0U;

  if ((settings->process_data & BSEC_PROCESS_PRESSURE) != 0U) {
    if (!bsec_service_append_input(inputs,
                                   input_count,
                                   BSEC_INPUT_PRESSURE,
                                   sample->pressure_pa,
                                   sample->timestamp_ns)) {
      return false;
    }
  }

  if ((settings->process_data & BSEC_PROCESS_HUMIDITY) != 0U) {
    if (!bsec_service_append_input(inputs,
                                   input_count,
                                   BSEC_INPUT_HUMIDITY,
                                   sample->humidity_percent,
                                   sample->timestamp_ns)) {
      return false;
    }
  }

  if ((settings->process_data & BSEC_PROCESS_TEMPERATURE) != 0U) {
    if (!bsec_service_append_input(inputs,
                                   input_count,
                                   BSEC_INPUT_TEMPERATURE,
                                   sample->temperature_c,
                                   sample->timestamp_ns)) {
      return false;
    }

    if (!bsec_service_append_input(inputs,
                                   input_count,
                                   BSEC_INPUT_HEATSOURCE,
                                   BSEC_SERVICE_HEATSOURCE_C,
                                   sample->timestamp_ns)) {
      return false;
    }
  }

  if ((settings->process_data & BSEC_PROCESS_GAS) != 0U) {
    if (bsec_service_has_valid_gas_data(sample)) {
      if (!bsec_service_append_input(inputs,
                                     input_count,
                                     BSEC_INPUT_GASRESISTOR,
                                     sample->gas_resistance_ohms,
                                     sample->timestamp_ns)) {
        return false;
      }
    }
  }

  if ((settings->process_data & BSEC_PROCESS_PROFILE_PART) != 0U) {
    if (bsec_service_has_valid_gas_data(sample)) {
      if (!bsec_service_append_input(inputs,
                                     input_count,
                                     BSEC_INPUT_PROFILE_PART,
                                     0.0f,
                                     sample->timestamp_ns)) {
        return false;
      }
    }
  }

  return true;
}

static void bsec_service_capture_outputs(bsec_service_t *service,
                                         const bsec_output_t *outputs,
                                         uint8_t output_count)
{
  uint8_t index;

  for (index = 0U; index < output_count; index++) {
    switch (outputs[index].sensor_id) {
      case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
        service->latest_output.temperature_c = outputs[index].signal;
        service->latest_output.timestamp_ns = outputs[index].time_stamp;
        service->latest_output.valid = true;
        break;
      case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
        service->latest_output.humidity_percent = outputs[index].signal;
        service->latest_output.timestamp_ns = outputs[index].time_stamp;
        service->latest_output.valid = true;
        break;
      case BSEC_OUTPUT_IAQ:
        service->latest_output.iaq = outputs[index].signal;
        service->latest_output.iaq_accuracy = outputs[index].accuracy;
        break;
      case BSEC_OUTPUT_CO2_EQUIVALENT:
        service->latest_output.co2_equivalent_ppm = outputs[index].signal;
        break;
      case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
        service->latest_output.breath_voc_equivalent_ppm = outputs[index].signal;
        break;
      case BSEC_OUTPUT_RAW_GAS:
        service->latest_output.raw_gas_ohms = outputs[index].signal;
        break;
      case BSEC_OUTPUT_GAS_PERCENTAGE:
        service->latest_output.gas_percentage = outputs[index].signal;
        break;
      case BSEC_OUTPUT_STABILIZATION_STATUS:
        service->latest_output.stabilization_status = (uint8_t)outputs[index].signal;
        break;
      case BSEC_OUTPUT_RUN_IN_STATUS:
        service->latest_output.run_in_status = (uint8_t)outputs[index].signal;
        break;
      default:
        break;
    }
  }
}

static void bsec_service_capture_raw_sample(bsec_service_t *service,
                                            const bme688_base_sample_t *sample)
{
  service->latest_output.raw_temperature_c = sample->temperature_c;
  service->latest_output.raw_humidity_percent = sample->humidity_percent;
}

static void bsec_service_build_reading(const bsec_service_output_t *output,
                                       air_quality_reading_t *reading)
{
  reading->valid = output->valid;
  reading->timestamp_ns = output->timestamp_ns;
  reading->temperature_c = output->raw_temperature_c;
  reading->humidity_percent = output->raw_humidity_percent;
  reading->breath_voc_equivalent_ppm = output->breath_voc_equivalent_ppm;
}

static void bsec_service_make_measurement_request(const bsec_bme_settings_t *settings,
                                                  bme688_base_measurement_request_t *request)
{
  request->temperature_oversampling = settings->temperature_oversampling;
  request->humidity_oversampling = settings->humidity_oversampling;
  request->pressure_oversampling = settings->pressure_oversampling;
  request->filter = BME68X_FILTER_OFF;
  request->heater_temperature_c = settings->heater_temperature;
  request->heater_duration_ms = settings->heater_duration;
  request->run_gas = (settings->run_gas != 0U);
}

bool bsec_service_init(bsec_service_t *service, uint8_t i2c_address)
{
  memset(service, 0, sizeof(*service));
  service->last_bme68x_status = bme688_base_init(&service->sensor, i2c_address);
  if (service->last_bme68x_status != BME68X_OK) {
    return false;
  }

  service->last_bsec_status = bsec_init();
  if (service->last_bsec_status < BSEC_OK) {
    return false;
  }

  service->last_bsec_status = bsec_service_subscribe_outputs();
  if (service->last_bsec_status < BSEC_OK) {
    return false;
  }

  service->initialized = true;
  service->next_call_ns = monotonic_time_now_ns();
  return true;
}

bool bsec_service_process(bsec_service_t *service)
{
  bsec_bme_settings_t sensor_settings = { 0 };
  bme688_base_measurement_request_t measurement_request = { 0 };
  bme688_base_sample_t sample = { 0 };
  bsec_input_t inputs[BSEC_SERVICE_MAX_INPUTS] = { 0 };
  bsec_output_t outputs[BSEC_NUMBER_OUTPUTS] = { 0 };
  uint8_t input_count = 0U;
  uint8_t output_count = BSEC_NUMBER_OUTPUTS;
  int64_t now_ns;

  if (!service->initialized) {
    return false;
  }

  now_ns = monotonic_time_now_ns();
  if (now_ns < service->next_call_ns) {
    return false;
  }

  service->last_bsec_status = bsec_sensor_control(now_ns, &sensor_settings);
  if (service->last_bsec_status < BSEC_OK) {
    return false;
  }

  service->next_call_ns = sensor_settings.next_call;
  if (sensor_settings.trigger_measurement == 0U) {
    return false;
  }

  bsec_service_make_measurement_request(&sensor_settings, &measurement_request);
  service->last_bme68x_status = bme688_base_measure_forced(&service->sensor,
                                                           &measurement_request,
                                                           &sample);
  if (service->last_bme68x_status != BME68X_OK) {
    return false;
  }

  bsec_service_capture_raw_sample(service, &sample);

  if (!bsec_service_build_inputs(&sensor_settings, &sample, inputs, &input_count)) {
    service->last_bsec_status = BSEC_E_DOSTEPS_INVALIDINPUT;
    return false;
  }

  service->last_bsec_status = bsec_do_steps(inputs, input_count, outputs, &output_count);
  if (service->last_bsec_status < BSEC_OK) {
    return false;
  }

  bsec_service_capture_outputs(service, outputs, output_count);
  return service->latest_output.valid;
}

bool bsec_service_read(bsec_service_t *service, air_quality_reading_t *reading)
{
  if ((reading == NULL) || !bsec_service_process(service)) {
    return false;
  }

  bsec_service_build_reading(&service->latest_output, reading);
  return reading->valid;
}

const bsec_service_output_t *bsec_service_latest_output(const bsec_service_t *service)
{
  return service->latest_output.valid ? &service->latest_output : NULL;
}

const char *bsec_service_status_name(bsec_library_return_t status)
{
  switch (status) {
    case BSEC_OK:
      return "ok";
    case BSEC_W_DOSTEPS_TSINTRADIFFOUTOFRANGE:
      return "warn-ts-range";
    case BSEC_I_DOSTEPS_NOOUTPUTSRETURNABLE:
      return "info-no-output";
    case BSEC_W_DOSTEPS_EXCESSOUTPUTS:
      return "warn-excess-output";
    case BSEC_W_DOSTEPS_GASINDEXMISS:
      return "warn-gas-index";
    case BSEC_W_SU_SAMPLERATEMISMATCH:
      return "warn-sample-rate-mismatch";
    case BSEC_W_SC_CALL_TIMING_VIOLATION:
      return "warn-call-timing";
    case BSEC_E_DOSTEPS_INVALIDINPUT:
      return "invalid-input";
    case BSEC_E_DOSTEPS_VALUELIMITS:
      return "value-limits";
    case BSEC_E_DOSTEPS_DUPLICATEINPUT:
      return "duplicate-input";
    case BSEC_E_SU_WRONGDATARATE:
      return "wrong-data-rate";
    case BSEC_E_SU_SAMPLERATELIMITS:
      return "sample-rate-limits";
    case BSEC_E_SU_DUPLICATEGATE:
      return "duplicate-gate";
    case BSEC_E_SU_INVALIDSAMPLERATE:
      return "invalid-sample-rate";
    case BSEC_E_SU_GATECOUNTEXCEEDSARRAY:
      return "gate-count-exceeds";
    case BSEC_E_SU_SAMPLINTVLINTEGERMULT:
      return "sample-interval";
    case BSEC_E_SU_MULTGASSAMPLINTVL:
      return "gas-sample-interval";
    case BSEC_E_SU_HIGHHEATERONDURATION:
      return "heater-duration";
    case BSEC_E_CONFIG_FAIL:
      return "config-fail";
    case BSEC_E_CONFIG_VERSIONMISMATCH:
      return "config-version";
    case BSEC_E_CONFIG_FEATUREMISMATCH:
      return "config-feature";
    default:
      return "unknown";
  }
}
