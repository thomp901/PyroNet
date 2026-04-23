#include <stdbool.h>
#include <stdint.h>

#include "6LoWPAN/ws/ws_common_defines.h"
#include "application.h"
#include "ti_wisunfan_config.h"
#include "ti_wisunfan_features.h"
#include "ws_config.h"

bool disable_ns_messages = false;

ti_wisun_config_t ti_wisun_config =
{
    .rapid_join = FEATURE_RAPID_JOIN_ENABLE,
    .network_size_config = FEATURE_NETWORK_PROFILE,
    .mpl_low_latency = FEATURE_MPL_LOW_LATENCY_ENABLE,
    .rapid_disconnect_detect_br = FEATURE_RAPID_DISCONNECT_DETECT_BR_SEC,
    .rapid_disconnect_detect_rn = FEATURE_RAPID_DISCONNECT_DETECT_RN_SEC,
    .auth_type = NETWORK_AUTH_TYPE,
    .use_fixed_gtk_keys = false,
    .force_star_topology = FEATURE_FORCE_STAR_TOPOLOGY,
    .use_dhcp_solicit_for_renew = true,
    .fixed_gtk_keys = {
        FIXED_GTK_KEY_1,
        FIXED_GTK_KEY_2,
        FIXED_GTK_KEY_3,
        FIXED_GTK_KEY_4,
    }
};

ti_br_config_t ti_br_config = {0};

configurable_props_t cfg_props =
{
    .ccaDefaultdBm = CONFIG_CCA_THRESHOLD,
    .phyTxPower = CONFIG_TRANSMIT_POWER,
    .uc_channel_function = CONFIG_CHANNEL_FUNCTION,
    .uc_channel_list = CONFIG_UNICAST_CHANNEL_MASK,
    .uc_fixed_channel = CONFIG_UNICAST_FIXED_CHANNEL_NUM,
    .uc_dwell_interval = CONFIG_UNICAST_DWELL_TIME,
    .bc_channel_function = 0,
    .bc_channel_list = CONFIG_BROADCAST_CHANNEL_MASK,
    .bc_fixed_channel = 0,
    .bc_interval = 0,
    .bc_dwell_interval = 0,
    .async_channel_list = CONFIG_ASYNC_CHANNEL_MASK,
    .pan_id = CONFIG_PAN_ID,
    .network_name = CONFIG_NETNAME,
    .wisun_device_type = CONFIG_WISUN_DEVICE_TYPE,
    .ch0_center_frequency = (uint32_t)(CONFIG_CENTER_FREQ * 1000),
    .config_channel_spacing = CONFIG_CHANNEL_SPACING,
    .config_number_of_channels = CONFIG_TOTAL_CHANNELS,
    .config_phy_id = CONFIG_PHY_ID,
    .config_reg_domain = CONFIG_REG_DOMAIN,
    .operating_class = CONFIG_OP_MODE_CLASS,
    .operating_mode = CONFIG_OP_MODE_ID,
    .config_chan_plan = 0,
    .config_chan_plan_id = 255,
    .hwaddr = CONFIG_INVALID_HWADDR,
#ifdef WISUN_FAN_CORE_1_1
    .mdr_enable = 0,
    .num_phy_mode = 1,
    .Phy_Mode_Id = { CONFIG_PHY_ID },
#endif
    .channel_page = CONFIG_CHANNEL_PAGE,
    .rx_on_when_idle = true,
#ifdef FEATURE_FULL_FUNCTION_DEVICE
    .ffd = true,
#else
    .ffd = false,
#endif
    .regulatory_channel_list = CONFIG_REGULATION_CHANNEL_MASK,
};
