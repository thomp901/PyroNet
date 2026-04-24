#ifndef SERVICES_AIR_QUALITY_BSEC_SERVICE_H
#define SERVICES_AIR_QUALITY_BSEC_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "drivers/bme688/bme688_base.h"
#include "third_party/bsec2/inc/bsec_interface.h"

typedef struct {
  bool valid;
  int64_t timestamp_ns;
  float raw_temperature_c;
  float raw_humidity_percent;
  float temperature_c;
  float humidity_percent;
  float iaq;
  float co2_equivalent_ppm;
  float breath_voc_equivalent_ppm;
  float raw_gas_ohms;
  float gas_percentage;
  uint8_t iaq_accuracy;
  uint8_t stabilization_status;
  uint8_t run_in_status;
  uint8_t raw_gas_status;
  uint8_t raw_gas_index;
} bsec_service_output_t;

typedef struct {
  bool valid;
  int64_t timestamp_ns;
  float temperature_c;
  float humidity_percent;
  float breath_voc_equivalent_ppm;
} air_quality_reading_t;

typedef struct {
  bool initialized;
  bool bootstrap_sample_emitted;
  bool bsec_state_restored;
  bool bsec_state_saved_this_boot;
  bool bsec_state_calibrated_previous;
  int8_t last_bme68x_status;
  bsec_library_return_t last_bsec_status;
  int64_t next_call_ns;
  int64_t last_bsec_state_save_ns;
  uint32_t bsec_config_crc;
  uint32_t bsec_version;
  uint32_t bsec_state_sequence;
  uint8_t work_buffer[BSEC_MAX_WORKBUFFER_SIZE];
  bme688_base_t sensor;
  bsec_service_output_t latest_output;
} bsec_service_t;

bool bsec_service_init(bsec_service_t *service, uint8_t i2c_address);
bool bsec_service_process(bsec_service_t *service);
bool bsec_service_read(bsec_service_t *service, air_quality_reading_t *reading);
bool bsec_service_get_reading(const bsec_service_t *service, air_quality_reading_t *reading);
const bsec_service_output_t *bsec_service_latest_output(const bsec_service_t *service);
const char *bsec_service_status_name(bsec_library_return_t status);

#endif
