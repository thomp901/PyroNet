#include "services/air_quality/bsec_state_store.h"

#include <stddef.h>
#include <string.h>

#include "em_msc.h"

#define PYRONET_BSEC_STATE_STORE_PAGE_COUNT      2U
#define PYRONET_BSEC_STATE_STORE_MAGIC           0x50594253UL
#define PYRONET_BSEC_STATE_STORE_COMMIT          0x42534543UL
#define PYRONET_BSEC_STATE_STORE_FORMAT_VERSION  1U
#define PYRONET_BSEC_STATE_STORE_CRC_POLY        0xEDB88320UL

typedef struct {
  uint32_t magic;
  uint16_t format_version;
  uint16_t record_size;
  uint32_t sequence;
  uint32_t config_crc;
  uint32_t bsec_version;
  uint32_t state_length;
  uint32_t state_crc;
  uint8_t state[BSEC_MAX_STATE_BLOB_SIZE];
  uint32_t commit;
} pyronet_bsec_state_store_record_t;

__attribute__((used, section(".internal_storage.bsec_state"), aligned(FLASH_PAGE_SIZE)))
static uint8_t pyronet_bsec_state_store_reservation[
  PYRONET_BSEC_STATE_STORE_PAGE_COUNT * FLASH_PAGE_SIZE
];

extern uint8_t linker_storage_begin[];

typedef struct {
  const pyronet_bsec_state_store_record_t *record;
  uint32_t sequence;
  uint32_t page_index;
  bool found;
  bool version_mismatch;
} pyronet_bsec_state_store_scan_t;

uint32_t pyronet_bsec_state_store_crc32(const void *data, uint32_t length)
{
  const uint8_t *bytes = (const uint8_t *)data;
  uint32_t crc = 0xFFFFFFFFUL;
  uint32_t index;
  uint8_t bit;

  if ((data == NULL) && (length > 0U)) {
    return 0U;
  }

  for (index = 0U; index < length; index++) {
    crc ^= bytes[index];
    for (bit = 0U; bit < 8U; bit++) {
      if ((crc & 1U) != 0U) {
        crc = (crc >> 1U) ^ PYRONET_BSEC_STATE_STORE_CRC_POLY;
      } else {
        crc >>= 1U;
      }
    }
  }

  return crc ^ 0xFFFFFFFFUL;
}

uint32_t pyronet_bsec_state_store_bsec_version_word(const bsec_version_t *version)
{
  if (version == NULL) {
    return 0U;
  }

  return (((uint32_t)version->major) << 24)
         | (((uint32_t)version->minor) << 16)
         | (((uint32_t)version->major_bugfix) << 8)
         | ((uint32_t)version->minor_bugfix);
}

static uintptr_t pyronet_bsec_state_store_base(void)
{
  return (uintptr_t)linker_storage_begin;
}

static uintptr_t pyronet_bsec_state_store_page_address(uint32_t page_index)
{
  return pyronet_bsec_state_store_base() + ((uintptr_t)page_index * FLASH_PAGE_SIZE);
}

static uint32_t pyronet_bsec_state_store_slots_per_page(void)
{
  return FLASH_PAGE_SIZE / (uint32_t)sizeof(pyronet_bsec_state_store_record_t);
}

static bool pyronet_bsec_state_store_slot_empty(
  const pyronet_bsec_state_store_record_t *record)
{
  const uint32_t *words = (const uint32_t *)record;
  uint32_t word_count = sizeof(*record) / sizeof(uint32_t);
  uint32_t index;

  for (index = 0U; index < word_count; index++) {
    if (words[index] != 0xFFFFFFFFUL) {
      return false;
    }
  }

  return true;
}

static bool pyronet_bsec_state_store_record_committed(
  const pyronet_bsec_state_store_record_t *record)
{
  return (record->commit == PYRONET_BSEC_STATE_STORE_COMMIT)
         && (record->magic == PYRONET_BSEC_STATE_STORE_MAGIC)
         && (record->format_version == PYRONET_BSEC_STATE_STORE_FORMAT_VERSION)
         && (record->record_size == sizeof(*record))
         && (record->state_length <= BSEC_MAX_STATE_BLOB_SIZE);
}

static bool pyronet_bsec_state_store_record_matches(
  const pyronet_bsec_state_store_record_t *record,
  uint32_t config_crc,
  uint32_t bsec_version)
{
  return (record->config_crc == config_crc)
         && (record->bsec_version == bsec_version);
}

static bool pyronet_bsec_state_store_record_valid(
  const pyronet_bsec_state_store_record_t *record,
  uint32_t config_crc,
  uint32_t bsec_version)
{
  if (!pyronet_bsec_state_store_record_committed(record)
      || !pyronet_bsec_state_store_record_matches(record, config_crc, bsec_version)) {
    return false;
  }

  return pyronet_bsec_state_store_crc32(record->state, record->state_length)
         == record->state_crc;
}

static pyronet_bsec_state_store_scan_t pyronet_bsec_state_store_scan(
  uint32_t config_crc,
  uint32_t bsec_version)
{
  pyronet_bsec_state_store_scan_t scan = { 0 };
  uint32_t slots_per_page = pyronet_bsec_state_store_slots_per_page();
  uint32_t page;
  uint32_t slot;

  for (page = 0U; page < PYRONET_BSEC_STATE_STORE_PAGE_COUNT; page++) {
    const pyronet_bsec_state_store_record_t *records =
      (const pyronet_bsec_state_store_record_t *)pyronet_bsec_state_store_page_address(page);

    for (slot = 0U; slot < slots_per_page; slot++) {
      const pyronet_bsec_state_store_record_t *record = &records[slot];

      if (!pyronet_bsec_state_store_record_committed(record)) {
        continue;
      }

      if (!pyronet_bsec_state_store_record_matches(record, config_crc, bsec_version)) {
        scan.version_mismatch = true;
        continue;
      }

      if (!pyronet_bsec_state_store_record_valid(record, config_crc, bsec_version)) {
        continue;
      }

      if (!scan.found || (record->sequence > scan.sequence)) {
        scan.found = true;
        scan.sequence = record->sequence;
        scan.record = record;
        scan.page_index = page;
      }
    }
  }

  return scan;
}

static bool pyronet_bsec_state_store_find_empty_slot(
  uint32_t page_index,
  pyronet_bsec_state_store_record_t **slot)
{
  pyronet_bsec_state_store_record_t *records =
    (pyronet_bsec_state_store_record_t *)pyronet_bsec_state_store_page_address(page_index);
  uint32_t slots_per_page = pyronet_bsec_state_store_slots_per_page();
  uint32_t index;

  for (index = 0U; index < slots_per_page; index++) {
    if (pyronet_bsec_state_store_slot_empty(&records[index])) {
      *slot = &records[index];
      return true;
    }
  }

  return false;
}

static pyronet_bsec_state_store_status_t pyronet_bsec_state_store_erase_page(
  uint32_t page_index)
{
  MSC_Status_TypeDef status;

  MSC_Init();
  status = MSC_ErasePage((uint32_t *)pyronet_bsec_state_store_page_address(page_index));
  MSC_Deinit();

  return (status == mscReturnOk)
           ? PYRONET_BSEC_STATE_STORE_OK
           : PYRONET_BSEC_STATE_STORE_ERASE_FAILED;
}

static pyronet_bsec_state_store_status_t pyronet_bsec_state_store_write_record(
  pyronet_bsec_state_store_record_t *slot,
  const pyronet_bsec_state_store_record_t *record)
{
  MSC_Status_TypeDef status;
  uint32_t commit = PYRONET_BSEC_STATE_STORE_COMMIT;

  MSC_Init();
  status = MSC_WriteWord((uint32_t *)slot,
                         record,
                         (uint32_t)offsetof(pyronet_bsec_state_store_record_t, commit));
  if (status == mscReturnOk) {
    status = MSC_WriteWord((uint32_t *)&slot->commit, &commit, sizeof(commit));
  }
  MSC_Deinit();

  return (status == mscReturnOk)
           ? PYRONET_BSEC_STATE_STORE_OK
           : PYRONET_BSEC_STATE_STORE_WRITE_FAILED;
}

pyronet_bsec_state_store_status_t pyronet_bsec_state_store_load(
  uint32_t config_crc,
  uint32_t bsec_version,
  uint8_t *state,
  uint32_t *state_length,
  uint32_t *sequence)
{
  pyronet_bsec_state_store_scan_t scan;

  if ((state == NULL) || (state_length == NULL)) {
    return PYRONET_BSEC_STATE_STORE_BAD_ARG;
  }

  scan = pyronet_bsec_state_store_scan(config_crc, bsec_version);
  if (!scan.found) {
    return scan.version_mismatch
             ? PYRONET_BSEC_STATE_STORE_VERSION_MISMATCH
             : PYRONET_BSEC_STATE_STORE_NOT_FOUND;
  }

  memcpy(state, scan.record->state, scan.record->state_length);
  *state_length = scan.record->state_length;
  if (sequence != NULL) {
    *sequence = scan.record->sequence;
  }

  return PYRONET_BSEC_STATE_STORE_OK;
}

pyronet_bsec_state_store_status_t pyronet_bsec_state_store_save(
  uint32_t config_crc,
  uint32_t bsec_version,
  const uint8_t *state,
  uint32_t state_length,
  uint32_t *sequence)
{
  pyronet_bsec_state_store_record_t record;
  pyronet_bsec_state_store_record_t *slot = NULL;
  pyronet_bsec_state_store_scan_t scan;
  pyronet_bsec_state_store_status_t status;
  uint32_t page_index;
  uint32_t old_page_index;

  if ((state == NULL) || (state_length == 0U) || (state_length > BSEC_MAX_STATE_BLOB_SIZE)) {
    return PYRONET_BSEC_STATE_STORE_BAD_ARG;
  }

  scan = pyronet_bsec_state_store_scan(config_crc, bsec_version);
  page_index = scan.found ? scan.page_index : 0U;
  old_page_index = page_index;

  if (!pyronet_bsec_state_store_find_empty_slot(page_index, &slot)) {
    page_index = (page_index + 1U) % PYRONET_BSEC_STATE_STORE_PAGE_COUNT;
    status = pyronet_bsec_state_store_erase_page(page_index);
    if (status != PYRONET_BSEC_STATE_STORE_OK) {
      return status;
    }

    if (!pyronet_bsec_state_store_find_empty_slot(page_index, &slot)) {
      return PYRONET_BSEC_STATE_STORE_INVALID;
    }
  }

  memset(&record, 0xFF, sizeof(record));
  record.magic = PYRONET_BSEC_STATE_STORE_MAGIC;
  record.format_version = PYRONET_BSEC_STATE_STORE_FORMAT_VERSION;
  record.record_size = sizeof(record);
  record.sequence = scan.found ? (scan.sequence + 1U) : 1U;
  record.config_crc = config_crc;
  record.bsec_version = bsec_version;
  record.state_length = state_length;
  memcpy(record.state, state, state_length);
  record.state_crc = pyronet_bsec_state_store_crc32(state, state_length);

  status = pyronet_bsec_state_store_write_record(slot, &record);
  if (status != PYRONET_BSEC_STATE_STORE_OK) {
    return status;
  }

  if (sequence != NULL) {
    *sequence = record.sequence;
  }

  if (scan.found && (page_index != old_page_index)) {
    (void)pyronet_bsec_state_store_erase_page(old_page_index);
  }

  return PYRONET_BSEC_STATE_STORE_OK;
}

const char *pyronet_bsec_state_store_status_name(
  pyronet_bsec_state_store_status_t status)
{
  switch (status) {
    case PYRONET_BSEC_STATE_STORE_OK:
      return "ok";
    case PYRONET_BSEC_STATE_STORE_NOT_FOUND:
      return "not-found";
    case PYRONET_BSEC_STATE_STORE_VERSION_MISMATCH:
      return "version-mismatch";
    case PYRONET_BSEC_STATE_STORE_INVALID:
      return "invalid";
    case PYRONET_BSEC_STATE_STORE_WRITE_FAILED:
      return "write-failed";
    case PYRONET_BSEC_STATE_STORE_ERASE_FAILED:
      return "erase-failed";
    case PYRONET_BSEC_STATE_STORE_BAD_ARG:
      return "bad-arg";
    default:
      return "unknown";
  }
}
