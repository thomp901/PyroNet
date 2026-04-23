#include "services/particulate/sps30_pm25.h"

#include <string.h>

#include "platform/monotonic_time.h"

#define SPS30_SERVICE_MEASUREMENT_INTERVAL_NS  1000000000LL
#define SPS30_SERVICE_POLL_INTERVAL_NS          250000000LL

static bool sps30_pm25_poll(sps30_pm25_t *sensor, float *pm25_ug_m3)
{
  bool data_ready = false;
  sps30_base_sample_t sample = { 0 };
  int64_t now_ns;

  if ((sensor == NULL) || !sensor->initialized || (pm25_ug_m3 == NULL)) {
    return false;
  }

  now_ns = monotonic_time_now_ns();
  if (now_ns < sensor->next_poll_ns) {
    return false;
  }

  sensor->next_poll_ns = now_ns + SPS30_SERVICE_POLL_INTERVAL_NS;

  sensor->last_status = sps30_base_read_data_ready(&sensor->sensor, &data_ready);
  if ((sensor->last_status != 0) || !data_ready) {
    return false;
  }

  sensor->last_status = sps30_base_read_sample(&sensor->sensor, &sample);
  if (sensor->last_status != 0) {
    return false;
  }

  *pm25_ug_m3 = sample.pm2p5_ug_m3;
  return true;
}

bool sps30_pm25_init(sps30_pm25_t *sensor, uint8_t i2c_address)
{
  if (sensor == NULL) {
    return false;
  }

  memset(sensor, 0, sizeof(*sensor));

  sensor->last_status = sps30_base_init(&sensor->sensor, i2c_address);
  if (sensor->last_status != 0) {
    return false;
  }

  sensor->last_status = sps30_base_start_measurement(&sensor->sensor);
  if (sensor->last_status != 0) {
    return false;
  }

  sensor->initialized = true;
  sensor->next_poll_ns = monotonic_time_now_ns() + SPS30_SERVICE_MEASUREMENT_INTERVAL_NS;
  return true;
}

bool sps30_pm25_read(sps30_pm25_t *sensor, float *pm25_ug_m3)
{
  return sps30_pm25_poll(sensor, pm25_ug_m3);
}

int16_t sps30_pm25_last_status(const sps30_pm25_t *sensor)
{
  return (sensor != NULL) ? sensor->last_status : SPS30_BASE_STATUS_INVALID_STATE;
}

void sps30_pm25_get_firmware_version(const sps30_pm25_t *sensor,
                                     uint8_t *major,
                                     uint8_t *minor)
{
  if ((sensor == NULL) || (major == NULL) || (minor == NULL)) {
    return;
  }

  *major = sensor->sensor.firmware_major;
  *minor = sensor->sensor.firmware_minor;
}
