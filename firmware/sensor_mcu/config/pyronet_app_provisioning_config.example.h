#ifndef PYRONET_APP_PROVISIONING_CONFIG_H
#define PYRONET_APP_PROVISIONING_CONFIG_H

/*
 * Template for machine- or board-local node provisioning.
 *
 * Copy this file to pyronet_app_provisioning_config.h and replace the default
 * values with deployment-specific settings when provisioning a real node.
 *
 * The copied .h file is intentionally ignored so local provisioning does not
 * create noisy diffs in normal firmware development.
 */

#ifndef PYRONET_APP_IDENTITY_CONFIGURED
#define PYRONET_APP_IDENTITY_CONFIGURED        0
#endif

#ifndef PYRONET_APP_NODE_ID
#define PYRONET_APP_NODE_ID                    0U
#endif

#ifndef PYRONET_APP_LATITUDE_DEG
#define PYRONET_APP_LATITUDE_DEG               0.0f
#endif

#ifndef PYRONET_APP_LONGITUDE_DEG
#define PYRONET_APP_LONGITUDE_DEG              0.0f
#endif

#ifndef PYRONET_APP_FW_VERSION_8_8
#define PYRONET_APP_FW_VERSION_8_8             0U
#endif

#ifndef PYRONET_APP_BATTERY_PCT
#define PYRONET_APP_BATTERY_PCT                100U
#endif

#ifndef PYRONET_APP_BOOT_UNIX_TIME_VALID
#define PYRONET_APP_BOOT_UNIX_TIME_VALID       0
#endif

#ifndef PYRONET_APP_BOOT_UNIX_TIME_S
#define PYRONET_APP_BOOT_UNIX_TIME_S           0U
#endif

#endif
