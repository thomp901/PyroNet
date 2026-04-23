#include "app/app_provisioning.h"

#include <stddef.h>

#include "pyronet_app_provisioning_config.h"

static const app_registration_identity_t app_registration_identity = {
  .configured = (PYRONET_APP_IDENTITY_CONFIGURED != 0),
  .node_id = PYRONET_APP_NODE_ID,
  .latitude = PYRONET_APP_LATITUDE_DEG,
  .longitude = PYRONET_APP_LONGITUDE_DEG,
  .fw_version_8_8 = PYRONET_APP_FW_VERSION_8_8,
  .battery_pct = PYRONET_APP_BATTERY_PCT,
};

const app_registration_identity_t *app_provisioning_identity(void)
{
  return &app_registration_identity;
}

bool app_provisioning_identity_valid(const app_registration_identity_t *identity)
{
  return (identity != NULL)
         && identity->configured
         && (identity->node_id != APP_PROVISIONING_NODE_ID_INVALID)
         && (identity->battery_pct <= 100U);
}

uint8_t app_provisioning_battery_pct(const app_registration_identity_t *identity)
{
  if (!app_provisioning_identity_valid(identity)) {
    return APP_PROVISIONING_BATTERY_UNAVAILABLE;
  }

  return identity->battery_pct;
}

bool app_provisioning_boot_unix_time_valid(void)
{
  return (PYRONET_APP_BOOT_UNIX_TIME_VALID != 0);
}

uint32_t app_provisioning_boot_unix_time_s(void)
{
  return PYRONET_APP_BOOT_UNIX_TIME_S;
}
