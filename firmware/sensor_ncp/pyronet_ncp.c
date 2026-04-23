#include "pyronet_ncp.h"

#include "ncp/pyronet_ncp_events.h"
#include "ncp/pyronet_ncp_local.h"
#include "ncp/pyronet_ncp_pending_tx.h"
#include "ncp/pyronet_ncp_rx.h"
#include "ncp/pyronet_ncp_state.h"

#include "swo_debug.h"

#include "NanostackTiRfPhy.h"
#include "coap_service_api.h"
#include "randLIB.h"
#include "wisun_tasklet.h"

static bool pyronetNcpInitialized;

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

    device_id = NanostackTiRfPhy_rf_register();
    if (device_id < 0)
    {
        return false;
    }

    randLIB_seed_random();
    wisun_tasklet_init();

    interface_id = wisun_tasklet_network_init(device_id);
    if (interface_id < 0)
    {
        return false;
    }

    coap_service_id = coap_service_initialize(interface_id,
                                              PYRONET_COAP_PORT,
                                              COAP_SERVICE_OPTIONS_NONE,
                                              NULL,
                                              NULL);
    if (coap_service_id < 0)
    {
        return false;
    }

    if (coap_service_register_uri(coap_service_id,
                                  PYRONET_COAP_DOWNLINK_URI,
                                  COAP_SERVICE_ACCESS_POST_ALLOWED,
                                  pyronet_ncp_downlink_receive) < 0)
    {
        coap_service_delete(coap_service_id);
        return false;
    }

    tasklet_id = wisun_tasklet_connect(pyronetNcpNetworkHandler, interface_id);
    if (tasklet_id < 0)
    {
        coap_service_delete(coap_service_id);
        return false;
    }

    pyronet_ncp_state_set_transport(interface_id, coap_service_id);
    pyronet_ncp_state_set_network_state(HOST_NETWORK_STATE_JOINING);
    pyronetNcpInitialized = true;

    (void)swoDebugWriteLine("PYRONET_NCP_INIT_OK");
    return true;
}

void pyronet_ncp_poll(void)
{
    if (!pyronetNcpInitialized)
    {
        return;
    }

    pyronet_ncp_state_poll_parent();
}

uint8_t pyronet_ncp_network_state(void)
{
    return pyronet_ncp_state_network_state();
}

bool pyronet_ncp_next_event(pyronet_ncp_event_t *out_event)
{
    return pyronet_ncp_events_next(out_event);
}
