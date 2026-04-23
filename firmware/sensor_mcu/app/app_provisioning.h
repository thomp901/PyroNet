#ifndef APP_APP_PROVISIONING_H
#define APP_APP_PROVISIONING_H

#include <stdbool.h>
#include <stdint.h>

#define APP_PROVISIONING_NODE_ID_INVALID       0U
#define APP_PROVISIONING_BATTERY_UNAVAILABLE   0xFFU

typedef struct {
  bool configured;
  uint16_t node_id;
  float latitude;
  float longitude;
  uint16_t fw_version_8_8;
  uint8_t battery_pct;
} app_registration_identity_t;

const app_registration_identity_t *app_provisioning_identity(void);
bool app_provisioning_identity_valid(const app_registration_identity_t *identity);
uint8_t app_provisioning_battery_pct(const app_registration_identity_t *identity);
bool app_provisioning_boot_unix_time_valid(void);
uint32_t app_provisioning_boot_unix_time_s(void);

#endif
