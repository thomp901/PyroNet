#include "app/app_sensor_runtime.h"

#include <stdio.h>
#include <string.h>

#include "platform/monotonic_time.h"
#include "services/particulate/sps30_pm25.h"

#define APP_SENSOR_RUNTIME_POLL_INTERVAL_NS  (10LL * 1000000000LL)

static int64_t app_sensor_runtime_pm25_interval_ns(uint8_t samples_per_day)
{
  (void)samples_per_day;

  return APP_SENSOR_RUNTIME_POLL_INTERVAL_NS;
}

static int64_t app_sensor_runtime_air_quality_interval_ns(void)
{
  return APP_SENSOR_RUNTIME_POLL_INTERVAL_NS;
}

static long app_sensor_runtime_scale_float(float value, float scale)
{
  float scaled_value = value * scale;

  if (scaled_value >= 0.0f) {
    scaled_value += 0.5f;
  } else {
    scaled_value -= 0.5f;
  }

  return (long)scaled_value;
}

static long app_sensor_runtime_scaled_abs_fraction(long scaled,
                                                   unsigned int decimals)
{
  long divisor = 1L;
  unsigned int index;

  for (index = 0U; index < decimals; index++) {
    divisor *= 10L;
  }

  scaled %= divisor;
  return (scaled < 0L) ? -scaled : scaled;
}

static void app_sensor_runtime_emit_combined_reading(app_sensor_runtime_t *runtime)
{
  long temp_x100;
  long rh_x100;
  long voc_x1000;
  long pm25_x1000;
  long raw_gas_ohms;
  long iaq_x100;
  pyronet_risk_level_t risk_level;

  if ((runtime == NULL)
      || !runtime->has_latest_air_quality
      || !runtime->has_latest_pm25
      || (runtime->latest_pm25_timestamp_ns == runtime->last_emitted_pm25_timestamp_ns)) {
    return;
  }

  runtime->read_seq++;
  runtime->last_emitted_pm25_timestamp_ns = runtime->latest_pm25_timestamp_ns;

  temp_x100 = app_sensor_runtime_scale_float(runtime->latest_air_quality.temperature_c,
                                             100.0f);
  rh_x100 = app_sensor_runtime_scale_float(runtime->latest_air_quality.humidity_percent,
                                           100.0f);
  voc_x1000 = app_sensor_runtime_scale_float(
    runtime->latest_air_quality.breath_voc_equivalent_ppm,
    1000.0f);
  pm25_x1000 = app_sensor_runtime_scale_float(runtime->latest_pm25_ug_m3, 1000.0f);

  printf("SENSOR_READING temp_c=%ld.%02ld rh_pct=%ld.%02ld voc_ppm=%ld.%03ld pm2.5_ug_m3=%ld.%03ld read_seq=%lu\r\n",
         temp_x100 / 100L,
         app_sensor_runtime_scaled_abs_fraction(temp_x100, 2U),
         rh_x100 / 100L,
         app_sensor_runtime_scaled_abs_fraction(rh_x100, 2U),
         voc_x1000 / 1000L,
         app_sensor_runtime_scaled_abs_fraction(voc_x1000, 3U),
         pm25_x1000 / 1000L,
         app_sensor_runtime_scaled_abs_fraction(pm25_x1000, 3U),
         (unsigned long)runtime->read_seq);

  if (runtime->has_latest_gas_diagnostic) {
    raw_gas_ohms = app_sensor_runtime_scale_float(
      runtime->latest_gas_diagnostic.raw_gas_ohms,
      1.0f);
    iaq_x100 = app_sensor_runtime_scale_float(runtime->latest_gas_diagnostic.iaq,
                                              100.0f);
    printf("GAS_DIAGNOSTIC raw_gas_ohms=%ld gas_index=%u gas_status=0x%02X iaq=%ld.%02ld iaq_accuracy=%u stabilization_status=%u run_in_status=%u bsec_status=%s read_seq=%lu\r\n",
           raw_gas_ohms,
           (unsigned int)runtime->latest_gas_diagnostic.raw_gas_index,
           (unsigned int)runtime->latest_gas_diagnostic.raw_gas_status,
           iaq_x100 / 100L,
           app_sensor_runtime_scaled_abs_fraction(iaq_x100, 2U),
           (unsigned int)runtime->latest_gas_diagnostic.iaq_accuracy,
           (unsigned int)runtime->latest_gas_diagnostic.stabilization_status,
           (unsigned int)runtime->latest_gas_diagnostic.run_in_status,
           bsec_service_status_name(runtime->air_quality.last_bsec_status),
           (unsigned long)runtime->read_seq);
  }

  risk_level = pyronet_risk_service_current_level(runtime->risk_service);
  printf("RISK_LEVEL level=%u read_seq=%lu\r\n",
         (unsigned int)risk_level,
         (unsigned long)runtime->read_seq);
}

static void app_sensor_runtime_log_status(const app_sensor_runtime_t *runtime)
{
  if (runtime == NULL) {
    return;
  }

  if (runtime->bsec_ready) {
    printf("BSEC_READY addr=0x%02X\r\n", SENSOR_BUS_BME68X_ADDRESS);
  } else if (runtime->bus.bme68x_present) {
    printf("BSEC_INIT_FAILED status=%s\r\n",
           bsec_service_status_name(runtime->air_quality.last_bsec_status));
  }

  if (runtime->sps30_ready) {
    uint8_t major = 0U;
    uint8_t minor = 0U;

    sps30_pm25_get_firmware_version(&runtime->pm25, &major, &minor);
    printf("SPS30_READY addr=0x%02X fw=%u.%u\r\n",
           SENSOR_BUS_SPS30_ADDRESS,
           (unsigned int)major,
           (unsigned int)minor);
  } else if (runtime->bus.sps30_present) {
    printf("SPS30_INIT_FAILED status=%s\r\n",
           sps30_base_status_name(sps30_pm25_last_status(&runtime->pm25)));
  }

  printf("SENSORS_READY bme68x=%u sps30=%u\r\n",
         runtime->bsec_ready ? 1U : 0U,
         runtime->sps30_ready ? 1U : 0U);
}

static void app_sensor_runtime_try_submit_air_quality(app_sensor_runtime_t *runtime)
{
  air_quality_reading_t reading = { 0 };
  const bsec_service_output_t *output;
  int64_t now_ns;

  if ((runtime == NULL) || !runtime->bsec_ready || (runtime->risk_service == NULL)) {
    return;
  }

  (void)bsec_service_process(&runtime->air_quality);

  now_ns = monotonic_time_now_ns();
  if (now_ns < runtime->next_air_quality_due_ns) {
    return;
  }

  if (!bsec_service_get_reading(&runtime->air_quality, &reading)) {
    return;
  }

  if (reading.timestamp_ns == runtime->last_air_quality_timestamp_ns) {
    return;
  }

  pyronet_risk_service_submit_air_quality(runtime->risk_service, &reading);
  runtime->has_latest_air_quality = true;
  runtime->latest_air_quality = reading;
  runtime->last_air_quality_timestamp_ns = reading.timestamp_ns;
  runtime->next_air_quality_due_ns = now_ns + runtime->air_quality_interval_ns;
  output = bsec_service_latest_output(&runtime->air_quality);
  if (output != NULL) {
    runtime->has_latest_gas_diagnostic = true;
    runtime->latest_gas_diagnostic = *output;
  }
}

static void app_sensor_runtime_try_submit_pm25(app_sensor_runtime_t *runtime)
{
  float pm25_ug_m3 = 0.0f;
  int64_t now_ns;

  if ((runtime == NULL) || !runtime->sps30_ready || (runtime->risk_service == NULL)) {
    return;
  }

  now_ns = monotonic_time_now_ns();
  if (!runtime->pm25_immediate_requested && (now_ns < runtime->next_pm25_due_ns)) {
    return;
  }

  if (!sps30_pm25_read(&runtime->pm25, &pm25_ug_m3)) {
    return;
  }

  pyronet_risk_service_submit_pm25(runtime->risk_service, now_ns, pm25_ug_m3);
  runtime->has_latest_pm25 = true;
  runtime->latest_pm25_timestamp_ns = now_ns;
  runtime->latest_pm25_ug_m3 = pm25_ug_m3;
  runtime->pm25_immediate_requested = false;
  runtime->next_pm25_due_ns = now_ns + runtime->pm25_interval_ns;
  app_sensor_runtime_emit_combined_reading(runtime);
}

void app_sensor_runtime_init(app_sensor_runtime_t *runtime,
                             pyronet_risk_service_t *risk_service)
{
  int64_t now_ns;

  if (runtime == NULL) {
    return;
  }

  memset(runtime, 0, sizeof(*runtime));
  runtime->risk_service = risk_service;
  runtime->bus = sensor_bus_init();
  runtime->bsec_ready = runtime->bus.bme68x_present
                        && bsec_service_init(&runtime->air_quality,
                                             SENSOR_BUS_BME68X_ADDRESS);
  runtime->sps30_ready = runtime->bus.sps30_present
                         && sps30_pm25_init(&runtime->pm25,
                                            SENSOR_BUS_SPS30_ADDRESS);

  now_ns = monotonic_time_now_ns();
  runtime->air_quality_interval_ns = app_sensor_runtime_air_quality_interval_ns();
  runtime->next_air_quality_due_ns = now_ns;
  runtime->last_air_quality_timestamp_ns = -1LL;
  runtime->pm25_interval_ns = app_sensor_runtime_pm25_interval_ns(0U);
  runtime->next_pm25_due_ns = now_ns;
  runtime->pm25_immediate_requested = runtime->sps30_ready;
  runtime->last_emitted_pm25_timestamp_ns = -1LL;

  app_sensor_runtime_log_status(runtime);
}

void app_sensor_runtime_process(app_sensor_runtime_t *runtime)
{
  app_sensor_runtime_try_submit_air_quality(runtime);
  app_sensor_runtime_try_submit_pm25(runtime);
}

void app_sensor_runtime_request_pm25_sample(void *context)
{
  app_sensor_runtime_t *runtime = (app_sensor_runtime_t *)context;

  if (runtime == NULL) {
    return;
  }

  runtime->pm25_immediate_requested = true;
}

void app_sensor_runtime_set_pm25_schedule(void *context, uint8_t samples_per_day)
{
  app_sensor_runtime_t *runtime = (app_sensor_runtime_t *)context;

  if (runtime == NULL) {
    return;
  }

  (void)samples_per_day;
  runtime->pm25_interval_ns = app_sensor_runtime_pm25_interval_ns(samples_per_day);
  if (!runtime->pm25_immediate_requested) {
    runtime->next_pm25_due_ns = monotonic_time_now_ns() + runtime->pm25_interval_ns;
  }
}
