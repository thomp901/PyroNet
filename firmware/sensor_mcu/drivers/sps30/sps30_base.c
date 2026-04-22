#include "drivers/sps30/sps30_base.h"

#include <string.h>

#include "platform/monotonic_time.h"
#include "third_party/sps30/sensirion_common.h"
#include "third_party/sps30/sensirion_i2c.h"
#include "third_party/sps30/sensirion_i2c_hal.h"
#include "third_party/sps30/sps30_i2c.h"

int16_t sps30_base_init(sps30_base_t *sensor, uint8_t i2c_address)
{
  int16_t result;

  if (sensor == NULL) {
    return SPS30_BASE_STATUS_INVALID_STATE;
  }

  memset(sensor, 0, sizeof(*sensor));
  sensor->i2c_address = i2c_address;

  sensirion_i2c_hal_init();
  sps30_init(i2c_address);

  result = sps30_read_firmware_version(&sensor->firmware_major,
                                       &sensor->firmware_minor);
  if (result == NO_ERROR) {
    sensor->initialized = true;
  }

  return result;
}

int16_t sps30_base_start_measurement(sps30_base_t *sensor)
{
  int16_t result;

  if ((sensor == NULL) || !sensor->initialized) {
    return SPS30_BASE_STATUS_INVALID_STATE;
  }

  sps30_init(sensor->i2c_address);
  result = sps30_start_measurement(SPS30_OUTPUT_FORMAT_OUTPUT_FORMAT_FLOAT);
  if (result == NO_ERROR) {
    sensor->measuring = true;
  }

  return result;
}

int16_t sps30_base_stop_measurement(sps30_base_t *sensor)
{
  int16_t result;

  if ((sensor == NULL) || !sensor->initialized) {
    return SPS30_BASE_STATUS_INVALID_STATE;
  }

  sps30_init(sensor->i2c_address);
  result = sps30_stop_measurement();
  if (result == NO_ERROR) {
    sensor->measuring = false;
  }

  return result;
}

int16_t sps30_base_read_data_ready(sps30_base_t *sensor, bool *data_ready)
{
  uint16_t ready_flag = 0U;
  int16_t result;

  if ((sensor == NULL) || (data_ready == NULL) || !sensor->initialized || !sensor->measuring) {
    return SPS30_BASE_STATUS_INVALID_STATE;
  }

  sps30_init(sensor->i2c_address);
  result = sps30_read_data_ready_flag(&ready_flag);
  if (result == NO_ERROR) {
    *data_ready = (ready_flag != 0U);
  }

  return result;
}

int16_t sps30_base_read_sample(sps30_base_t *sensor, sps30_base_sample_t *sample)
{
  int16_t result;

  if ((sensor == NULL) || (sample == NULL) || !sensor->initialized || !sensor->measuring) {
    return SPS30_BASE_STATUS_INVALID_STATE;
  }

  sps30_init(sensor->i2c_address);
  result = sps30_read_measurement_values_float(&sample->pm1p0_ug_m3,
                                               &sample->pm2p5_ug_m3,
                                               &sample->pm4p0_ug_m3,
                                               &sample->pm10p0_ug_m3,
                                               &sample->nc0p5_per_cm3,
                                               &sample->nc1p0_per_cm3,
                                               &sample->nc2p5_per_cm3,
                                               &sample->nc4p0_per_cm3,
                                               &sample->nc10p0_per_cm3,
                                               &sample->typical_particle_size_um);
  if (result == NO_ERROR) {
    sample->timestamp_ns = monotonic_time_now_ns();
  }

  return result;
}

const char *sps30_base_status_name(int16_t status)
{
  switch (status) {
    case NO_ERROR:
      return "ok";
    case CRC_ERROR:
      return "crc-error";
    case I2C_BUS_ERROR:
      return "i2c-bus-error";
    case I2C_NACK_ERROR:
      return "i2c-nack";
    case BYTE_NUM_ERROR:
      return "byte-num-error";
    case SPS30_BASE_STATUS_INVALID_STATE:
      return "invalid-state";
    default:
      return "unknown";
  }
}
