#ifndef DRIVERS_SPS30_SPS30_BASE_H
#define DRIVERS_SPS30_SPS30_BASE_H

#include <stdbool.h>
#include <stdint.h>

#define SPS30_BASE_STATUS_INVALID_STATE  32

typedef struct {
  float pm1p0_ug_m3;
  float pm2p5_ug_m3;
  float pm4p0_ug_m3;
  float pm10p0_ug_m3;
  float nc0p5_per_cm3;
  float nc1p0_per_cm3;
  float nc2p5_per_cm3;
  float nc4p0_per_cm3;
  float nc10p0_per_cm3;
  float typical_particle_size_um;
  int64_t timestamp_ns;
} sps30_base_sample_t;

typedef struct {
  bool initialized;
  bool measuring;
  uint8_t i2c_address;
  uint8_t firmware_major;
  uint8_t firmware_minor;
} sps30_base_t;

int16_t sps30_base_init(sps30_base_t *sensor, uint8_t i2c_address);
int16_t sps30_base_start_measurement(sps30_base_t *sensor);
int16_t sps30_base_stop_measurement(sps30_base_t *sensor);
int16_t sps30_base_read_data_ready(sps30_base_t *sensor, bool *data_ready);
int16_t sps30_base_read_sample(sps30_base_t *sensor, sps30_base_sample_t *sample);
const char *sps30_base_status_name(int16_t status);

#endif
