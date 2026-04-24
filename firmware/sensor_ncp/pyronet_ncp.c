#include "pyronet_ncp.h"

#include "ncp/pyronet_ncp_events.h"
#include "ncp/pyronet_ncp_local.h"
#include "ncp/pyronet_ncp_pending_tx.h"
#include "ncp/pyronet_ncp_rx.h"
#include "ncp/pyronet_ncp_state.h"
#include "ncp/pyronet_ncp_events.h"
#include "ncp/pyronet_ncp_tx.h"

#include "swo_debug.h"

#include "NanostackTiRfPhy.h"
#include "coap_service_api.h"
#include "mesh_system.h"
#include "randLIB.h"
#include "ti_wisunfan_config.h"
#include "ws_management_api.h"
#include "wisun_tasklet.h"

static bool pyronetNcpInitialized;
static uint8_t pyronetNcpUnicastChannelMask[] = CONFIG_UNICAST_CHANNEL_MASK;
static uint32_t pyronetNcpDiagPolls;

#define PYRONET_NCP_DIAG_POLL_INTERVAL 5000U

static bool pyronetNcpConfigureDefaults(void)
{
    int result;
    const int8_t config_interface_id = -1;

    result = ws_management_network_name_set(config_interface_id, CONFIG_NETNAME);
    if (result < 0)
    {
        (void)swoDebugPrintf("PYRONET_WISUN_CFG_FAILED step=network_name rc=%d", result);
        return false;
    }

    result = ws_management_regulatory_domain_set(config_interface_id,
                                                 CONFIG_REG_DOMAIN,
                                                 CONFIG_OP_MODE_CLASS,
                                                 CONFIG_OP_MODE_ID);
    if (result < 0)
    {
        (void)swoDebugPrintf("PYRONET_WISUN_CFG_FAILED step=reg_domain rc=%d", result);
        return false;
    }

    result = ws_management_fhss_unicast_channel_function_configure(config_interface_id,
                                                                   CONFIG_CHANNEL_FUNCTION,
                                                                   CONFIG_UNICAST_FIXED_CHANNEL_NUM,
                                                                   CONFIG_UNICAST_DWELL_TIME);
    if (result < 0)
    {
        (void)swoDebugPrintf("PYRONET_WISUN_CFG_FAILED step=uc_fhss rc=%d", result);
        return false;
    }

    result = ws_management_fhss_broadcast_channel_function_configure(config_interface_id, 0U, 0U, 0U, 0U);
    if (result < 0)
    {
        (void)swoDebugPrintf("PYRONET_WISUN_CFG_FAILED step=bc_fhss rc=%d", result);
        return false;
    }

    result = ws_management_channel_mask_set(config_interface_id, pyronetNcpUnicastChannelMask, NULL);
    if (result < 0)
    {
        (void)swoDebugPrintf("PYRONET_WISUN_CFG_FAILED step=channel_mask rc=%d", result);
        return false;
    }

    result = ws_management_network_size_set(config_interface_id, FEATURE_NETWORK_PROFILE);
    if (result < 0)
    {
        (void)swoDebugPrintf("PYRONET_WISUN_CFG_FAILED step=network_size rc=%d", result);
        return false;
    }

    return true;
}

static void pyronetNcpNetworkHandler(mesh_connection_status_t status)
{
    pyronet_ncp_state_handle_network_status(status);
}

bool pyronet_ncp_init(void)
{
    int8_t device_id;
    int8_t interface_id;
    int8_t coap_service_id;
    int8_t tasklet_id;
    bool ok = false;

    if (pyronetNcpInitialized)
    {
        return true;
    }

    if (!pyronet_ncp_events_init() ||
        !pyronet_ncp_state_init() ||
        !pyronet_ncp_pending_tx_init())
    {
        return false;
    }

    nanostack_lock();

    if (!pyronetNcpConfigureDefaults())
    {
        goto done;
    }

    device_id = NanostackTiRfPhy_rf_register();
    if (device_id < 0)
    {
        goto done;
    }

    randLIB_seed_random();
    wisun_tasklet_init();

    interface_id = wisun_tasklet_network_init(device_id);
    if (interface_id < 0)
    {
        goto done;
    }

    if (wisun_tasklet_statistics_start() != 0)
    {
        (void)swoDebugWriteLine("PYRONET_WISUN_STATS_START_FAILED");
    }

    coap_service_id = coap_service_initialize(interface_id,
                                              PYRONET_COAP_PORT,
                                              COAP_SERVICE_OPTIONS_NONE,
                                              NULL,
                                              NULL);
    if (coap_service_id < 0)
    {
        goto done;
    }

    if (coap_service_register_uri(coap_service_id,
                                  PYRONET_COAP_DOWNLINK_URI,
                                  COAP_SERVICE_ACCESS_POST_ALLOWED,
                                  pyronet_ncp_downlink_receive) < 0)
    {
        coap_service_delete(coap_service_id);
        goto done;
    }

    tasklet_id = wisun_tasklet_connect(pyronetNcpNetworkHandler, interface_id);
    if (tasklet_id < 0)
    {
        coap_service_delete(coap_service_id);
        goto done;
    }

    pyronet_ncp_state_set_transport(interface_id, coap_service_id);
    pyronet_ncp_state_set_network_state(HOST_NETWORK_STATE_JOINING);
    pyronetNcpInitialized = true;

    (void)swoDebugWriteLine("PYRONET_NCP_INIT_OK");
    ok = true;

done:
    nanostack_unlock();
    return ok;
}

void pyronet_ncp_poll(void)
{
    mesh_nw_statistics_t stats;
    char router_address[40];
    uint8_t network_state;

    if (!pyronetNcpInitialized)
    {
        return;
    }

    pyronet_ncp_state_poll_connectivity();
    pyronet_ncp_state_poll_parent();
    pyronet_ncp_tx_poll();

    pyronetNcpDiagPolls++;
    if (pyronetNcpDiagPolls < PYRONET_NCP_DIAG_POLL_INTERVAL)
    {
        return;
    }
    pyronetNcpDiagPolls = 0U;

    network_state = pyronet_ncp_state_network_state();
    if (network_state == HOST_NETWORK_STATE_JOINED)
    {
        return;
    }

    memset(&stats, 0, sizeof(stats));
    memset(router_address, 0, sizeof(router_address));

    if (wisun_tasklet_statistics_nw_read(&stats) == 0)
    {
        int8_t router_status = wisun_tasklet_get_router_ip_address(router_address, (int8_t)sizeof(router_address));

        (void)swoDebugPrintf("PYRONET_WISUN_DIAG host_state=%u router_status=%d router=%s j1=%lu j2=%lu j3=%lu j4=%lu j5=%lu recv_pa=%lu recv_pas=%lu recv_pc=%lu recv_pcs=%lu",
                             (unsigned int)network_state,
                             (int)router_status,
                             (router_status == 0) ? router_address : "-",
                             (unsigned long)stats.join_state_1,
                             (unsigned long)stats.join_state_2,
                             (unsigned long)stats.join_state_3,
                             (unsigned long)stats.join_state_4,
                             (unsigned long)stats.join_state_5,
                             (unsigned long)stats.recv_PA,
                             (unsigned long)stats.recv_PAS,
                             (unsigned long)stats.recv_PC,
                             (unsigned long)stats.recv_PCS);
        pyronet_ncp_state_log_connectivity_snapshot(network_state);
    }
    else
    {
        (void)swoDebugPrintf("PYRONET_WISUN_DIAG host_state=%u stats=unavailable",
                             (unsigned int)network_state);
    }
}

uint8_t pyronet_ncp_network_state(void)
{
    return pyronet_ncp_state_network_state();
}

void pyronet_ncp_host_session_ready(void)
{
    uint8_t reason = PYRONET_REG_REASON_JOIN;

    if (!pyronet_ncp_state_request_registration_sync(&reason))
    {
        return;
    }

    (void)swoDebugPrintf("PYRONET_HOST_SYNC queue_registration=1 reason=%u host_state=%u",
                         (unsigned int)reason,
                         (unsigned int)pyronet_ncp_state_network_state());
    pyronet_ncp_events_queue_registration_needed(reason);
}

bool pyronet_ncp_next_event(pyronet_ncp_event_t *out_event)
{
    return pyronet_ncp_events_next(out_event);
}
