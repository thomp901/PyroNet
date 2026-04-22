#ifndef SERVICES_PARTICULATE_SPS30_PM25_H
#define SERVICES_PARTICULATE_SPS30_PM25_H

#include <stdbool.h>
#include <stdint.h>

#include "drivers/sps30/sps30_base.h"

typedef struct {
  bool initialized;
  int16_t last_status;
  int64_t next_poll_ns;
  sps30_base_t sensor;
} sps30_pm25_t;

bool sps30_pm25_init(sps30_pm25_t *sensor, uint8_t i2c_address);
bool sps30_pm25_read(sps30_pm25_t *sensor, float *pm25_ug_m3);
int16_t sps30_pm25_last_status(const sps30_pm25_t *sensor);
void sps30_pm25_get_firmware_version(const sps30_pm25_t *sensor,
                                     uint8_t *major,
                                     uint8_t *minor);

#endif
