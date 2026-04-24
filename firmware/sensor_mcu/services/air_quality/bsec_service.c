#include "services/air_quality/bsec_service.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "platform/monotonic_time.h"
#include "services/air_quality/bsec_state_store.h"
#include "third_party/bsec2/config/bsec_iaq.h"

#define BSEC_SERVICE_HEATSOURCE_C            0.466f
#define BSEC_SERVICE_MAX_INPUTS              6U
#define BSEC_SERVICE_MAX_REQUESTED_OUTPUTS   9U
#define BSEC_SERVICE_WORK_BUFFER_SIZE        BSEC_MAX_WORKBUFFER_SIZE
#define BSEC_SERVICE_STATE_SAVE_INTERVAL_NS  (15LL * 60LL * 1000000000LL)

static bool bsec_service_has_valid_gas_data(const bme688_base_sample_t *sample)
{
  return (sample->status & BME68X_GASM_VALID_MSK) != 0U;
}

static float bsec_service_profile_part(const bme688_base_sample_t *sample)
{
  return (float)((uint32_t)sample->gas_index + 1U);
}

static bsec_library_return_t bsec_service_subscribe_outputs(void)
{
  bsec_sensor_configuration_t requested_outputs[BSEC_SERVICE_MAX_REQUESTED_OUTPUTS] = {
    { .sample_rate = BSEC_SAMPLE_RATE_LP, .sensor_id = BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE },
    { .sample_rate = BSEC_SAMPLE_RATE_LP, .sensor_id = BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY },
    { .sample_rate = BSEC_SAMPLE_RATE_LP, .sensor_id = BSEC_OUTPUT_BREATH_VOC_EQUIVALENT },
    { .sample_rate = BSEC_SAMPLE_RATE_LP, .sensor_id = BSEC_OUTPUT_IAQ },
    { .sample_rate = BSEC_SAMPLE_RATE_LP, .sensor_id = BSEC_OUTPUT_CO2_EQUIVALENT },
    { .sample_rate = BSEC_SAMPLE_RATE_LP, .sensor_id = BSEC_OUTPUT_RAW_GAS },
    { .sample_rate = BSEC_SAMPLE_RATE_LP, .sensor_id = BSEC_OUTPUT_GAS_PERCENTAGE },
    { .sample_rate = BSEC_SAMPLE_RATE_LP, .sensor_id = BSEC_OUTPUT_STABILIZATION_STATUS },
    { .sample_rate = BSEC_SAMPLE_RATE_LP, .sensor_id = BSEC_OUTPUT_RUN_IN_STATUS },
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
                                     bsec_service_profile_part(sample),
                                     sample->timestamp_ns)) {
        return false;
      }
    }
  }

  return true;
}

static bool bsec_service_capture_outputs(bsec_service_t *service,
                                         const bsec_output_t *outputs,
                                         uint8_t output_count)
{
  uint8_t index;
  bool reading_ready = false;

  for (index = 0U; index < output_count; index++) {
    switch (outputs[index].sensor_id) {
      case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
        service->latest_output.temperature_c = outputs[index].signal;
        service->latest_output.timestamp_ns = outputs[index].time_stamp;
        service->latest_output.valid = true;
        reading_ready = true;
        break;
      case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
        service->latest_output.humidity_percent = outputs[index].signal;
        service->latest_output.timestamp_ns = outputs[index].time_stamp;
        service->latest_output.valid = true;
        reading_ready = true;
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

  return reading_ready;
}

static void bsec_service_capture_raw_sample(bsec_service_t *service,
                                            const bme688_base_sample_t *sample)
{
  service->latest_output.raw_temperature_c = sample->temperature_c;
  service->latest_output.raw_humidity_percent = sample->humidity_percent;
  service->latest_output.raw_gas_status = sample->status;
  service->latest_output.raw_gas_index = sample->gas_index;
  service->latest_output.timestamp_ns = sample->timestamp_ns;
}

static bool bsec_service_state_save_eligible(const bsec_service_t *service)
{
  return (service != NULL)
         && service->initialized
         && (service->last_bsec_status == BSEC_OK)
         && (service->latest_output.iaq_accuracy >= 3U)
         && (service->latest_output.stabilization_status != 0U)
         && (service->latest_output.run_in_status != 0U);
}

static void bsec_service_maybe_save_state(bsec_service_t *service, int64_t now_ns)
{
  uint8_t serialized_state[BSEC_MAX_STATE_BLOB_SIZE] = { 0 };
  uint32_t serialized_state_length = BSEC_MAX_STATE_BLOB_SIZE;
  uint32_t sequence = 0U;
  bsec_library_return_t bsec_status;
  pyronet_bsec_state_store_status_t store_status;
  const char *reason = "periodic";
  bool eligible;
  bool returned_calibrated;

  eligible = bsec_service_state_save_eligible(service);
  returned_calibrated = eligible && !service->bsec_state_calibrated_previous;
  service->bsec_state_calibrated_previous = eligible;

  if (!eligible) {
    return;
  }

  if (!service->bsec_state_saved_this_boot) {
    reason = "first";
  } else if (returned_calibrated) {
    reason = "recalibrated";
  } else if ((now_ns - service->last_bsec_state_save_ns)
             < BSEC_SERVICE_STATE_SAVE_INTERVAL_NS) {
    return;
  }

  bsec_status = bsec_get_state(0U,
                               serialized_state,
                               sizeof(serialized_state),
                               service->work_buffer,
                               sizeof(service->work_buffer),
                               &serialized_state_length);
  if (bsec_status < BSEC_OK) {
    service->last_bsec_status = bsec_status;
    printf("BSEC_STATE_SAVE_FAILED reason=bsec-get-state status=%s\r\n",
           bsec_service_status_name(bsec_status));
    return;
  }

  store_status = pyronet_bsec_state_store_save(service->bsec_config_crc,
                                               service->bsec_version,
                                               serialized_state,
                                               serialized_state_length,
                                               &sequence);
  if (store_status != PYRONET_BSEC_STATE_STORE_OK) {
    printf("BSEC_STATE_SAVE_FAILED reason=%s status=%s\r\n",
           reason,
           pyronet_bsec_state_store_status_name(store_status));
    return;
  }

  service->bsec_state_saved_this_boot = true;
  service->last_bsec_state_save_ns = now_ns;
  service->bsec_state_sequence = sequence;
  printf("BSEC_STATE_SAVED reason=%s len=%lu seq=%lu\r\n",
         reason,
         (unsigned long)serialized_state_length,
         (unsigned long)sequence);
}

static bool bsec_service_bootstrap_immediate_reading(bsec_service_t *service,
                                                     const bme688_base_sample_t *sample)
{
  if ((service == NULL) || (sample == NULL) || service->bootstrap_sample_emitted) {
    return false;
  }

  service->latest_output.temperature_c = sample->temperature_c;
  service->latest_output.humidity_percent = sample->humidity_percent;
  service->latest_output.timestamp_ns = sample->timestamp_ns;
  service->latest_output.valid = true;
  service->bootstrap_sample_emitted = true;
  return true;
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

static void bsec_service_restore_state(bsec_service_t *service)
{
  uint8_t serialized_state[BSEC_MAX_STATE_BLOB_SIZE] = { 0 };
  uint32_t serialized_state_length = 0U;
  uint32_t sequence = 0U;
  pyronet_bsec_state_store_status_t store_status;

  store_status = pyronet_bsec_state_store_load(service->bsec_config_crc,
                                               service->bsec_version,
                                               serialized_state,
                                               &serialized_state_length,
                                               &sequence);
  if (store_status != PYRONET_BSEC_STATE_STORE_OK) {
    printf("BSEC_STATE_RESTORE_SKIPPED reason=%s\r\n",
           pyronet_bsec_state_store_status_name(store_status));
    return;
  }

  service->last_bsec_status = bsec_set_state(serialized_state,
                                             serialized_state_length,
                                             service->work_buffer,
                                             sizeof(service->work_buffer));
  if (service->last_bsec_status < BSEC_OK) {
    printf("BSEC_STATE_RESTORE_FAILED reason=bsec-set-state status=%s\r\n",
           bsec_service_status_name(service->last_bsec_status));
    return;
  }

  service->bsec_state_restored = true;
  service->bsec_state_saved_this_boot = true;
  service->bsec_state_calibrated_previous = true;
  service->last_bsec_state_save_ns = monotonic_time_now_ns();
  service->bsec_state_sequence = sequence;
  printf("BSEC_STATE_RESTORED len=%lu seq=%lu\r\n",
         (unsigned long)serialized_state_length,
         (unsigned long)sequence);
}

bool bsec_service_init(bsec_service_t *service, uint8_t i2c_address)
{
  bsec_version_t bsec_version = { 0 };

  memset(service, 0, sizeof(*service));
  service->last_bme68x_status = bme688_base_init(&service->sensor, i2c_address);
  if (service->last_bme68x_status != BME68X_OK) {
    return false;
  }

  service->last_bsec_status = bsec_init();
  if (service->last_bsec_status < BSEC_OK) {
    return false;
  }

  service->last_bsec_status = bsec_set_configuration(bsec_config_iaq,
                                                     sizeof(bsec_config_iaq),
                                                     service->work_buffer,
                                                     sizeof(service->work_buffer));
  if (service->last_bsec_status < BSEC_OK) {
    return false;
  }

  service->bsec_config_crc = pyronet_bsec_state_store_crc32(bsec_config_iaq,
                                                            sizeof(bsec_config_iaq));
  service->last_bsec_status = bsec_get_version(&bsec_version);
  if (service->last_bsec_status < BSEC_OK) {
    return false;
  }
  service->bsec_version = pyronet_bsec_state_store_bsec_version_word(&bsec_version);
  bsec_service_restore_state(service);

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
  bool reading_ready = false;

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

  if (output_count > 0U) {
    reading_ready = bsec_service_capture_outputs(service, outputs, output_count);
  }

  if (!reading_ready) {
    reading_ready = bsec_service_bootstrap_immediate_reading(service, &sample);
  }

  bsec_service_maybe_save_state(service, now_ns);

  return reading_ready;
}

bool bsec_service_read(bsec_service_t *service, air_quality_reading_t *reading)
{
  if ((reading == NULL) || !bsec_service_process(service)) {
    return false;
  }

  bsec_service_build_reading(&service->latest_output, reading);
  return reading->valid;
}

bool bsec_service_get_reading(const bsec_service_t *service, air_quality_reading_t *reading)
{
  if ((service == NULL) || (reading == NULL) || !service->latest_output.valid) {
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
