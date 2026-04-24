#ifndef SERVICES_AIR_QUALITY_BSEC_STATE_STORE_H
#define SERVICES_AIR_QUALITY_BSEC_STATE_STORE_H

#include <stdbool.h>
#include <stdint.h>

#include "third_party/bsec2/inc/bsec_datatypes.h"

typedef enum {
  PYRONET_BSEC_STATE_STORE_OK = 0,
  PYRONET_BSEC_STATE_STORE_NOT_FOUND,
  PYRONET_BSEC_STATE_STORE_VERSION_MISMATCH,
  PYRONET_BSEC_STATE_STORE_INVALID,
  PYRONET_BSEC_STATE_STORE_WRITE_FAILED,
  PYRONET_BSEC_STATE_STORE_ERASE_FAILED,
  PYRONET_BSEC_STATE_STORE_BAD_ARG,
} pyronet_bsec_state_store_status_t;

uint32_t pyronet_bsec_state_store_crc32(const void *data, uint32_t length);
uint32_t pyronet_bsec_state_store_bsec_version_word(const bsec_version_t *version);

pyronet_bsec_state_store_status_t pyronet_bsec_state_store_load(
  uint32_t config_crc,
  uint32_t bsec_version,
  uint8_t *state,
  uint32_t *state_length,
  uint32_t *sequence);

pyronet_bsec_state_store_status_t pyronet_bsec_state_store_save(
  uint32_t config_crc,
  uint32_t bsec_version,
  const uint8_t *state,
  uint32_t state_length,
  uint32_t *sequence);

const char *pyronet_bsec_state_store_status_name(
  pyronet_bsec_state_store_status_t status);

#endif
