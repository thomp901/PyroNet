#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app/app_sensor_runtime.h"

static int64_t test_now_ns;
static sensor_bus_state_t test_sensor_bus_state;
static bool test_bsec_init_ok;
static bool test_bsec_process_ok;
static bool test_bsec_get_reading_ok;
static air_quality_reading_t test_bsec_reading;
static bsec_service_output_t test_bsec_output;
static bool test_sps30_init_ok;
static bool test_sps30_read_ok;
static float test_sps30_pm25_value;
static unsigned int test_air_quality_submit_count;
static unsigned int test_pm25_submit_count;
static air_quality_reading_t test_last_air_quality_reading;
static int64_t test_last_pm25_timestamp_ns;
static float test_last_pm25_ug_m3;
static pyronet_risk_level_t test_current_risk_level;

int64_t monotonic_time_now_ns(void)
{
  return test_now_ns;
}

sensor_bus_state_t sensor_bus_init(void)
{
  return test_sensor_bus_state;
}

bool bsec_service_init(bsec_service_t *service, uint8_t i2c_address)
{
  (void)service;
  (void)i2c_address;
  return test_bsec_init_ok;
}

bool bsec_service_process(bsec_service_t *service)
{
  (void)service;

  return test_bsec_process_ok;
}

bool bsec_service_get_reading(const bsec_service_t *service, air_quality_reading_t *reading)
{
  (void)service;

  if ((reading == NULL) || !test_bsec_get_reading_ok) {
    return false;
  }

  *reading = test_bsec_reading;
  return true;
}

const bsec_service_output_t *bsec_service_latest_output(const bsec_service_t *service)
{
  (void)service;
  return &test_bsec_output;
}

const char *bsec_service_status_name(bsec_library_return_t status)
{
  (void)status;
  return "stub";
}

bool sps30_pm25_init(sps30_pm25_t *sensor, uint8_t i2c_address)
{
  (void)sensor;
  (void)i2c_address;
  return test_sps30_init_ok;
}

bool sps30_pm25_read(sps30_pm25_t *sensor, float *pm25_ug_m3)
{
  (void)sensor;

  if ((pm25_ug_m3 == NULL) || !test_sps30_read_ok) {
    return false;
  }

  *pm25_ug_m3 = test_sps30_pm25_value;
  return true;
}

int16_t sps30_pm25_last_status(const sps30_pm25_t *sensor)
{
  (void)sensor;
  return 0;
}

void sps30_pm25_get_firmware_version(const sps30_pm25_t *sensor,
                                     uint8_t *major,
                                     uint8_t *minor)
{
  (void)sensor;

  if (major != NULL) {
    *major = 1U;
  }

  if (minor != NULL) {
    *minor = 2U;
  }
}

const char *sps30_base_status_name(int16_t status)
{
  (void)status;
  return "stub";
}

void pyronet_risk_service_submit_air_quality(pyronet_risk_service_t *service,
                                             const air_quality_reading_t *reading)
{
  (void)service;

  test_air_quality_submit_count++;
  test_last_air_quality_reading = *reading;
}

void pyronet_risk_service_submit_pm25(pyronet_risk_service_t *service,
                                      int64_t timestamp_ns,
                                      float pm25_ug_m3)
{
  (void)service;

  test_pm25_submit_count++;
  test_last_pm25_timestamp_ns = timestamp_ns;
  test_last_pm25_ug_m3 = pm25_ug_m3;
}

pyronet_risk_level_t pyronet_risk_service_current_level(
  const pyronet_risk_service_t *service)
{
  (void)service;

  return test_current_risk_level;
}

static void test_reset_state(void)
{
  memset(&test_sensor_bus_state, 0, sizeof(test_sensor_bus_state));
  memset(&test_bsec_reading, 0, sizeof(test_bsec_reading));
  memset(&test_bsec_output, 0, sizeof(test_bsec_output));
  memset(&test_last_air_quality_reading, 0, sizeof(test_last_air_quality_reading));
  test_now_ns = 0LL;
  test_bsec_init_ok = false;
  test_bsec_process_ok = false;
  test_bsec_get_reading_ok = false;
  test_sps30_init_ok = false;
  test_sps30_read_ok = false;
  test_sps30_pm25_value = 0.0f;
  test_air_quality_submit_count = 0U;
  test_pm25_submit_count = 0U;
  test_last_pm25_timestamp_ns = 0LL;
  test_last_pm25_ug_m3 = 0.0f;
  test_current_risk_level = PYRONET_RISK_LEVEL_1;
}

static void test_runtime_initializes_detected_sensors(void)
{
  app_sensor_runtime_t runtime;
  pyronet_risk_service_t risk_service;

  test_reset_state();
  test_now_ns = 123LL;
  test_sensor_bus_state.bme68x_present = true;
  test_sensor_bus_state.sps30_present = true;
  test_bsec_init_ok = true;
  test_sps30_init_ok = true;

  app_sensor_runtime_init(&runtime, &risk_service);

  assert(runtime.bsec_ready);
  assert(runtime.sps30_ready);
  assert(runtime.risk_service == &risk_service);
  assert(runtime.next_air_quality_due_ns == 123LL);
  assert(runtime.pm25_immediate_requested);
  assert(runtime.next_pm25_due_ns == 123LL);
  assert(runtime.pm25_interval_ns == 10000000000LL);
  assert(runtime.air_quality_interval_ns == 10000000000LL);
  assert(runtime.read_seq == 0U);
}

static void test_runtime_process_submits_sensor_data(void)
{
  app_sensor_runtime_t runtime;
  pyronet_risk_service_t risk_service;

  test_reset_state();
  test_now_ns = 5000LL;
  test_sensor_bus_state.bme68x_present = true;
  test_sensor_bus_state.sps30_present = true;
  test_bsec_init_ok = true;
  test_bsec_process_ok = true;
  test_bsec_get_reading_ok = true;
  test_bsec_reading.valid = true;
  test_bsec_reading.timestamp_ns = 4000LL;
  test_bsec_reading.temperature_c = 25.5f;
  test_bsec_reading.humidity_percent = 40.0f;
  test_bsec_reading.breath_voc_equivalent_ppm = 123.0f;
  test_sps30_init_ok = true;
  test_sps30_read_ok = true;
  test_sps30_pm25_value = 12.5f;

  app_sensor_runtime_init(&runtime, &risk_service);
  app_sensor_runtime_process(&runtime);

  assert(test_air_quality_submit_count == 1U);
  assert(test_last_air_quality_reading.timestamp_ns == 4000LL);
  assert(test_pm25_submit_count == 1U);
  assert(test_last_pm25_timestamp_ns == 5000LL);
  assert(test_last_pm25_ug_m3 == 12.5f);
  assert(!runtime.pm25_immediate_requested);
  assert(runtime.next_pm25_due_ns > 5000LL);
  assert(runtime.has_latest_air_quality);
  assert(runtime.has_latest_pm25);
  assert(runtime.latest_pm25_ug_m3 == 12.5f);
  assert(runtime.read_seq == 1U);
}

static void test_runtime_immediate_pm25_request_bypasses_schedule(void)
{
  app_sensor_runtime_t runtime;
  pyronet_risk_service_t risk_service;

  test_reset_state();
  test_sensor_bus_state.sps30_present = true;
  test_sps30_init_ok = true;
  test_sps30_read_ok = true;
  test_sps30_pm25_value = 22.0f;

  app_sensor_runtime_init(&runtime, &risk_service);
  runtime.pm25_immediate_requested = false;
  test_now_ns = 1000LL;
  app_sensor_runtime_set_pm25_schedule(&runtime, 8U);

  test_now_ns = runtime.next_pm25_due_ns - 1LL;
  app_sensor_runtime_process(&runtime);
  assert(test_pm25_submit_count == 0U);

  app_sensor_runtime_request_pm25_sample(&runtime);
  app_sensor_runtime_process(&runtime);
  assert(test_pm25_submit_count == 1U);
  assert(test_last_pm25_timestamp_ns == (runtime.next_pm25_due_ns - runtime.pm25_interval_ns));
}

static void test_runtime_air_quality_waits_for_10_second_interval(void)
{
  app_sensor_runtime_t runtime;
  pyronet_risk_service_t risk_service;

  test_reset_state();
  test_sensor_bus_state.bme68x_present = true;
  test_bsec_init_ok = true;
  test_bsec_process_ok = true;
  test_bsec_get_reading_ok = true;
  test_bsec_reading.valid = true;
  test_bsec_reading.timestamp_ns = 100LL;
  test_bsec_reading.temperature_c = 24.0f;
  test_bsec_reading.humidity_percent = 50.0f;
  test_bsec_reading.breath_voc_equivalent_ppm = 10.0f;

  app_sensor_runtime_init(&runtime, &risk_service);
  app_sensor_runtime_process(&runtime);
  assert(test_air_quality_submit_count == 1U);

  test_now_ns = 9999999999LL;
  test_bsec_reading.timestamp_ns = 200LL;
  app_sensor_runtime_process(&runtime);
  assert(test_air_quality_submit_count == 1U);

  test_now_ns = 10000000000LL;
  app_sensor_runtime_process(&runtime);
  assert(test_air_quality_submit_count == 2U);
  assert(test_last_air_quality_reading.timestamp_ns == 200LL);
}

int main(void)
{
  test_runtime_initializes_detected_sensors();
  test_runtime_process_submits_sensor_data();
  test_runtime_immediate_pm25_request_bypasses_schedule();
  test_runtime_air_quality_waits_for_10_second_interval();
  return 0;
}
