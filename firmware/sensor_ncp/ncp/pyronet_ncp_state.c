#include "pyronet_ncp_state.h"

#include <pthread.h>
#include <string.h>

#include "../swo_debug.h"

#include "ip6string.h"
#include "wisun_tasklet.h"
#include "ws_management_api.h"

#include "pyronet_ncp_events.h"

typedef struct pyronet_ncp_state_context
{
    bool initialized;
    int8_t interface_id;
    int8_t coap_service_id;
    uint8_t network_state;
    bool ever_joined;
    bool session_joined;
    bool registration_required;
    bool local_node_id_known;
    uint16_t local_node_id;
    bool parent_initialized;
    uint8_t last_parent_ipv6[PYRONET_IPV6_ADDR_LEN];
    uint8_t neighbor_count;
    uint8_t neighbors[PYRONET_MAX_NEIGHBORS][PYRONET_IPV6_ADDR_LEN];
    bool pending_neighbor_table_valid;
    uint16_t pending_neighbor_target_node_id;
    uint8_t pending_neighbor_count;
    uint8_t pending_neighbors[PYRONET_MAX_NEIGHBORS][PYRONET_IPV6_ADDR_LEN];
    pthread_mutex_t lock;
} pyronet_ncp_state_context_t;

static pyronet_ncp_state_context_t pyronetNcpStateContext;
static const uint8_t pyronetZeroIpv6[PYRONET_IPV6_ADDR_LEN] = {0};

static void pyronetNcpStateResetLocked(void)
{
    pyronetNcpStateContext.interface_id = -1;
    pyronetNcpStateContext.coap_service_id = -1;
    pyronetNcpStateContext.network_state = HOST_NETWORK_STATE_DOWN;
    pyronetNcpStateContext.ever_joined = false;
    pyronetNcpStateContext.session_joined = false;
    pyronetNcpStateContext.registration_required = false;
    pyronetNcpStateContext.local_node_id_known = false;
    pyronetNcpStateContext.local_node_id = 0U;
    pyronetNcpStateContext.parent_initialized = false;
    pyronetNcpStateContext.neighbor_count = 0U;
    pyronetNcpStateContext.pending_neighbor_table_valid = false;
    pyronetNcpStateContext.pending_neighbor_target_node_id = 0U;
    pyronetNcpStateContext.pending_neighbor_count = 0U;
    memset(pyronetNcpStateContext.last_parent_ipv6, 0, sizeof(pyronetNcpStateContext.last_parent_ipv6));
    memset(pyronetNcpStateContext.neighbors, 0, sizeof(pyronetNcpStateContext.neighbors));
    memset(pyronetNcpStateContext.pending_neighbors, 0, sizeof(pyronetNcpStateContext.pending_neighbors));
}

static void pyronetNcpStateLock(void)
{
    (void)pthread_mutex_lock(&pyronetNcpStateContext.lock);
}

static void pyronetNcpStateUnlock(void)
{
    (void)pthread_mutex_unlock(&pyronetNcpStateContext.lock);
}

static bool pyronetNcpStateReadLocalNodeId(uint16_t *node_id_out)
{
    bool known;

    if (node_id_out == NULL)
    {
        return false;
    }

    pyronetNcpStateLock();
    known = pyronetNcpStateContext.local_node_id_known;
    if (known)
    {
        *node_id_out = pyronetNcpStateContext.local_node_id;
    }
    pyronetNcpStateUnlock();

    return known;
}

static void pyronetNcpStateApplyNeighborTableLocked(uint8_t neighbor_count,
                                                    const uint8_t neighbors[PYRONET_MAX_NEIGHBORS][PYRONET_IPV6_ADDR_LEN])
{
    uint8_t index;

    pyronetNcpStateContext.neighbor_count = neighbor_count;
    memset(pyronetNcpStateContext.neighbors, 0, sizeof(pyronetNcpStateContext.neighbors));
    for (index = 0; index < neighbor_count; ++index)
    {
        memcpy(pyronetNcpStateContext.neighbors[index], neighbors[index], PYRONET_IPV6_ADDR_LEN);
    }
}

bool pyronet_ncp_state_init(void)
{
    if (pyronetNcpStateContext.initialized)
    {
        pyronetNcpStateLock();
        pyronetNcpStateResetLocked();
        pyronetNcpStateUnlock();
        return true;
    }

    memset(&pyronetNcpStateContext, 0, sizeof(pyronetNcpStateContext));
    if (pthread_mutex_init(&pyronetNcpStateContext.lock, NULL) != 0)
    {
        return false;
    }

    pyronetNcpStateContext.initialized = true;
    pyronetNcpStateLock();
    pyronetNcpStateResetLocked();
    pyronetNcpStateUnlock();
    return true;
}

void pyronet_ncp_state_set_transport(int8_t interface_id, int8_t coap_service_id)
{
    pyronetNcpStateLock();
    pyronetNcpStateContext.interface_id = interface_id;
    pyronetNcpStateContext.coap_service_id = coap_service_id;
    pyronetNcpStateUnlock();
}

int8_t pyronet_ncp_state_coap_service_id(void)
{
    int8_t coap_service_id;

    pyronetNcpStateLock();
    coap_service_id = pyronetNcpStateContext.coap_service_id;
    pyronetNcpStateUnlock();

    return coap_service_id;
}

void pyronet_ncp_state_set_network_state(uint8_t network_state)
{
    pyronetNcpStateLock();
    pyronetNcpStateContext.network_state = network_state;
    pyronetNcpStateUnlock();
}

uint8_t pyronet_ncp_state_network_state(void)
{
    uint8_t network_state;

    pyronetNcpStateLock();
    network_state = pyronetNcpStateContext.network_state;
    pyronetNcpStateUnlock();

    return network_state;
}

void pyronet_ncp_state_clear_registration_required(void)
{
    pyronetNcpStateLock();
    pyronetNcpStateContext.registration_required = false;
    pyronetNcpStateUnlock();
}

void pyronet_ncp_state_remember_local_node_id(uint16_t node_id)
{
    bool apply_pending = false;
    bool drop_pending = false;

    pyronetNcpStateLock();
    pyronetNcpStateContext.local_node_id = node_id;
    pyronetNcpStateContext.local_node_id_known = true;
    if (pyronetNcpStateContext.pending_neighbor_table_valid)
    {
        if (pyronetNcpStateContext.pending_neighbor_target_node_id == node_id)
        {
            pyronetNcpStateApplyNeighborTableLocked(pyronetNcpStateContext.pending_neighbor_count,
                                                    pyronetNcpStateContext.pending_neighbors);
            apply_pending = true;
        }
        else
        {
            drop_pending = true;
        }

        pyronetNcpStateContext.pending_neighbor_table_valid = false;
        pyronetNcpStateContext.pending_neighbor_target_node_id = 0U;
        pyronetNcpStateContext.pending_neighbor_count = 0U;
        memset(pyronetNcpStateContext.pending_neighbors, 0, sizeof(pyronetNcpStateContext.pending_neighbors));
    }
    pyronetNcpStateUnlock();

    if (apply_pending)
    {
        (void)swoDebugPrintf("PYRONET_NN_TABLE_APPLIED_DEFERRED local=%u",
                             (unsigned int)node_id);
    }
    else if (drop_pending)
    {
        (void)swoDebugPrintf("PYRONET_NN_TABLE_DROP_DEFERRED local=%u",
                             (unsigned int)node_id);
    }
}

bool pyronet_ncp_state_read_current_parent(uint8_t parent_out[static PYRONET_IPV6_ADDR_LEN])
{
    ws_stack_info_t info;
    int8_t interface_id;
    uint8_t network_state;

    memset(parent_out, 0, PYRONET_IPV6_ADDR_LEN);

    pyronetNcpStateLock();
    interface_id = pyronetNcpStateContext.interface_id;
    network_state = pyronetNcpStateContext.network_state;
    pyronetNcpStateUnlock();

    if ((interface_id < 0) || (network_state != HOST_NETWORK_STATE_JOINED))
    {
        return false;
    }

    memset(&info, 0, sizeof(info));
    if (ws_stack_info_get(interface_id, &info) < 0)
    {
        return false;
    }

    memcpy(parent_out, info.parent, PYRONET_IPV6_ADDR_LEN);
    return (memcmp(parent_out, pyronetZeroIpv6, PYRONET_IPV6_ADDR_LEN) != 0);
}

bool pyronet_ncp_state_read_router_address(uint8_t destination_out[static PYRONET_IPV6_ADDR_LEN])
{
    char address[PYRONET_ROUTER_ADDR_STR_LEN];
    int8_t interface_id;
    uint8_t network_state;

    memset(destination_out, 0, PYRONET_IPV6_ADDR_LEN);
    memset(address, 0, sizeof(address));

    pyronetNcpStateLock();
    interface_id = pyronetNcpStateContext.interface_id;
    network_state = pyronetNcpStateContext.network_state;
    pyronetNcpStateUnlock();

    if ((interface_id < 0) || (network_state != HOST_NETWORK_STATE_JOINED))
    {
        return false;
    }

    if (wisun_tasklet_get_router_ip_address(address, (int8_t)sizeof(address)) != 0)
    {
        return false;
    }

    return stoip6(address, strlen(address), destination_out);
}

uint8_t pyronet_ncp_state_copy_neighbors(uint8_t neighbors_out[PYRONET_MAX_NEIGHBORS][PYRONET_IPV6_ADDR_LEN])
{
    uint8_t neighbor_count;

    if (neighbors_out == NULL)
    {
        return 0U;
    }

    pyronetNcpStateLock();
    neighbor_count = pyronetNcpStateContext.neighbor_count;
    memcpy(neighbors_out, pyronetNcpStateContext.neighbors, sizeof(pyronetNcpStateContext.neighbors));
    pyronetNcpStateUnlock();

    return neighbor_count;
}

pyronet_ncp_neighbor_table_result_t pyronet_ncp_state_accept_neighbor_table(
  const pyronet_mesh_nn_table_update_view_t *update)
{
    uint16_t local_node_id;
    uint8_t index;

    if ((update == NULL) || (update->neighbors == NULL))
    {
        return PYRONET_NCP_NEIGHBOR_TABLE_REJECTED_TARGET;
    }

    if (!pyronetNcpStateReadLocalNodeId(&local_node_id))
    {
        pyronetNcpStateLock();
        pyronetNcpStateContext.pending_neighbor_table_valid = true;
        pyronetNcpStateContext.pending_neighbor_target_node_id = update->header.target_node_id;
        pyronetNcpStateContext.pending_neighbor_count = update->header.neighbor_count;
        memset(pyronetNcpStateContext.pending_neighbors, 0, sizeof(pyronetNcpStateContext.pending_neighbors));
        for (index = 0; index < update->header.neighbor_count; ++index)
        {
            memcpy(pyronetNcpStateContext.pending_neighbors[index],
                   &update->neighbors[(uint16_t)index * PYRONET_IPV6_ADDR_LEN],
                   PYRONET_IPV6_ADDR_LEN);
        }
        pyronetNcpStateUnlock();

        (void)swoDebugPrintf("PYRONET_NN_TABLE_DEFER target=%u count=%u",
                             (unsigned int)update->header.target_node_id,
                             (unsigned int)update->header.neighbor_count);
        return PYRONET_NCP_NEIGHBOR_TABLE_DEFERRED;
    }

    if (update->header.target_node_id != local_node_id)
    {
        (void)swoDebugPrintf("PYRONET_NN_TABLE_REJECT target=%u local=%u",
                             (unsigned int)update->header.target_node_id,
                             (unsigned int)local_node_id);
        return PYRONET_NCP_NEIGHBOR_TABLE_REJECTED_TARGET;
    }

    pyronetNcpStateLock();
    pyronetNcpStateContext.neighbor_count = update->header.neighbor_count;
    memset(pyronetNcpStateContext.neighbors, 0, sizeof(pyronetNcpStateContext.neighbors));
    for (index = 0; index < update->header.neighbor_count; ++index)
    {
        memcpy(pyronetNcpStateContext.neighbors[index],
               &update->neighbors[(uint16_t)index * PYRONET_IPV6_ADDR_LEN],
               PYRONET_IPV6_ADDR_LEN);
    }
    pyronetNcpStateUnlock();

    return PYRONET_NCP_NEIGHBOR_TABLE_APPLIED;
}

void pyronet_ncp_state_handle_network_status(mesh_connection_status_t status)
{
    bool queue_registration = false;
    uint8_t reason = PYRONET_REG_REASON_JOIN;

    pyronetNcpStateLock();

    switch (status)
    {
        case MESH_CONNECTED:
        case MESH_CONNECTED_LOCAL:
        case MESH_CONNECTED_GLOBAL:
            pyronetNcpStateContext.network_state = HOST_NETWORK_STATE_JOINED;
            if (!pyronetNcpStateContext.session_joined)
            {
                pyronetNcpStateContext.session_joined = true;
                pyronetNcpStateContext.registration_required = true;
                pyronetNcpStateContext.parent_initialized = false;
                memset(pyronetNcpStateContext.last_parent_ipv6, 0, sizeof(pyronetNcpStateContext.last_parent_ipv6));
                reason = pyronetNcpStateContext.ever_joined ? PYRONET_REG_REASON_REJOIN : PYRONET_REG_REASON_JOIN;
                pyronetNcpStateContext.ever_joined = true;
                queue_registration = true;
            }
            break;

        case MESH_BOOTSTRAP_STARTED:
            pyronetNcpStateContext.network_state = HOST_NETWORK_STATE_JOINING;
            pyronetNcpStateContext.session_joined = false;
            pyronetNcpStateContext.parent_initialized = false;
            memset(pyronetNcpStateContext.last_parent_ipv6, 0, sizeof(pyronetNcpStateContext.last_parent_ipv6));
            break;

        case MESH_DISCONNECTED:
        case MESH_BOOTSTRAP_START_FAILED:
        case MESH_BOOTSTRAP_FAILED:
        default:
            pyronetNcpStateContext.network_state = HOST_NETWORK_STATE_DOWN;
            pyronetNcpStateContext.session_joined = false;
            pyronetNcpStateContext.parent_initialized = false;
            memset(pyronetNcpStateContext.last_parent_ipv6, 0, sizeof(pyronetNcpStateContext.last_parent_ipv6));
            break;
    }

    pyronetNcpStateUnlock();

    if (queue_registration)
    {
        pyronet_ncp_events_queue_registration_needed(reason);
    }
}

void pyronet_ncp_state_poll_parent(void)
{
    uint8_t parent[PYRONET_IPV6_ADDR_LEN];
    bool has_parent;
    bool emit_event = false;
    uint8_t change_reason = 0U;
    bool is_joined;

    pyronetNcpStateLock();
    is_joined = (pyronetNcpStateContext.network_state == HOST_NETWORK_STATE_JOINED);
    pyronetNcpStateUnlock();

    if (!is_joined)
    {
        return;
    }

    has_parent = pyronet_ncp_state_read_current_parent(parent);

    pyronetNcpStateLock();
    if (!pyronetNcpStateContext.parent_initialized)
    {
        memcpy(pyronetNcpStateContext.last_parent_ipv6, has_parent ? parent : pyronetZeroIpv6, PYRONET_IPV6_ADDR_LEN);
        pyronetNcpStateContext.parent_initialized = true;
    }
    else
    {
        bool had_parent = (memcmp(pyronetNcpStateContext.last_parent_ipv6, pyronetZeroIpv6, PYRONET_IPV6_ADDR_LEN) != 0);

        if (had_parent && !has_parent)
        {
            emit_event = true;
            change_reason = PYRONET_PARENT_CHANGE_PARENT_LOST;
        }
        else if (had_parent && has_parent &&
                 (memcmp(pyronetNcpStateContext.last_parent_ipv6, parent, PYRONET_IPV6_ADDR_LEN) != 0))
        {
            emit_event = true;
            change_reason = PYRONET_PARENT_CHANGE_PREFERRED_PARENT_CHANGED;
        }

        memcpy(pyronetNcpStateContext.last_parent_ipv6, has_parent ? parent : pyronetZeroIpv6, PYRONET_IPV6_ADDR_LEN);
    }
    pyronetNcpStateUnlock();

    if (emit_event)
    {
        pyronet_ncp_events_queue_parent_changed(change_reason);
    }
}
