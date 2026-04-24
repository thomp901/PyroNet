#include "pyronet_ncp_state.h"

#include <pthread.h>
#include <string.h>

#include "../swo_debug.h"

#include "ip6string.h"
#include "mesh_system.h"
#include "net_interface.h"
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
    bool pending_join_ready;
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

typedef enum pyronet_ncp_route_source
{
    PYRONET_NCP_ROUTE_SOURCE_NONE = 0,
    PYRONET_NCP_ROUTE_SOURCE_SDK_ROUTER = 1,
    PYRONET_NCP_ROUTE_SOURCE_ND_BORDER_ROUTER = 2,
    PYRONET_NCP_ROUTE_SOURCE_CONFIGURED_BORDER_ROUTER = 3,
    PYRONET_NCP_ROUTE_SOURCE_DERIVED_PARENT_GLOBAL = 4,
} pyronet_ncp_route_source_t;

typedef struct pyronet_ncp_route_trace_context
{
    bool initialized;
    pyronet_ncp_router_address_result_t result;
    pyronet_ncp_route_source_t source;
    int8_t router_status;
    int8_t nd_status;
    bool has_global;
    bool has_parent;
    uint8_t destination[PYRONET_IPV6_ADDR_LEN];
    uint8_t global[PYRONET_IPV6_ADDR_LEN];
    uint8_t parent[PYRONET_IPV6_ADDR_LEN];
    uint8_t nd_border_router[PYRONET_IPV6_ADDR_LEN];
} pyronet_ncp_route_trace_context_t;

static pyronet_ncp_route_trace_context_t pyronetNcpRouteTraceContext;
static const uint8_t pyronetConfiguredBorderRouterIpv6[PYRONET_IPV6_ADDR_LEN] = {
    0xfdU, 0x12U, 0x34U, 0x56U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x8eU, 0x8bU, 0x48U, 0xffU, 0xfeU, 0x22U, 0x85U, 0x04U,
};

static const char *pyronetNcpRouteSourceName(pyronet_ncp_route_source_t source)
{
    switch (source)
    {
        case PYRONET_NCP_ROUTE_SOURCE_SDK_ROUTER:
            return "sdk_router";
        case PYRONET_NCP_ROUTE_SOURCE_ND_BORDER_ROUTER:
            return "nd_border_router";
        case PYRONET_NCP_ROUTE_SOURCE_CONFIGURED_BORDER_ROUTER:
            return "configured_border_router";
        case PYRONET_NCP_ROUTE_SOURCE_DERIVED_PARENT_GLOBAL:
            return "derived_parent_global";
        case PYRONET_NCP_ROUTE_SOURCE_NONE:
        default:
            return "none";
    }
}

static const char *pyronetNcpRouterAddressResultName(pyronet_ncp_router_address_result_t result)
{
    switch (result)
    {
        case PYRONET_NCP_ROUTER_ADDRESS_READY:
            return "ready";
        case PYRONET_NCP_ROUTER_ADDRESS_NOT_JOINED:
            return "not_joined";
        case PYRONET_NCP_ROUTER_ADDRESS_UNAVAILABLE:
            return "unavailable";
        case PYRONET_NCP_ROUTER_ADDRESS_INVALID:
            return "invalid";
        default:
            return "unknown";
    }
}

static const char *pyronetNcpIpv6ToString(const uint8_t address[static PYRONET_IPV6_ADDR_LEN],
                                          char text[static PYRONET_ROUTER_ADDR_STR_LEN])
{
    if ((address == NULL) ||
        (memcmp(address, pyronetZeroIpv6, PYRONET_IPV6_ADDR_LEN) == 0))
    {
        return "-";
    }

    ip6tos(address, text);
    return text;
}

static bool pyronetNcpDeriveGlobalFromPrefix(const uint8_t prefix_source[static PYRONET_IPV6_ADDR_LEN],
                                             const uint8_t iid_source[static PYRONET_IPV6_ADDR_LEN],
                                             uint8_t address_out[static PYRONET_IPV6_ADDR_LEN])
{
    memset(address_out, 0, PYRONET_IPV6_ADDR_LEN);

    if ((prefix_source == NULL) ||
        (iid_source == NULL) ||
        (memcmp(prefix_source, pyronetZeroIpv6, PYRONET_IPV6_ADDR_LEN) == 0) ||
        (memcmp(iid_source, pyronetZeroIpv6, PYRONET_IPV6_ADDR_LEN) == 0))
    {
        return false;
    }

    memcpy(address_out, prefix_source, 8U);
    memcpy(&address_out[8], &iid_source[8], 8U);
    return true;
}

static bool pyronetNcpIpv6IidMatches(const uint8_t lhs[static PYRONET_IPV6_ADDR_LEN],
                                     const uint8_t rhs[static PYRONET_IPV6_ADDR_LEN])
{
    return (lhs != NULL) &&
           (rhs != NULL) &&
           (memcmp(lhs, pyronetZeroIpv6, PYRONET_IPV6_ADDR_LEN) != 0) &&
           (memcmp(rhs, pyronetZeroIpv6, PYRONET_IPV6_ADDR_LEN) != 0) &&
           (memcmp(&lhs[8], &rhs[8], 8U) == 0);
}

static void pyronetNcpTraceRouteSelection(pyronet_ncp_router_address_result_t result,
                                          pyronet_ncp_route_source_t source,
                                          int8_t router_status,
                                          int8_t nd_status,
                                          bool has_global,
                                          bool has_parent,
                                          const uint8_t destination[static PYRONET_IPV6_ADDR_LEN],
                                          const uint8_t global[static PYRONET_IPV6_ADDR_LEN],
                                          const uint8_t parent[static PYRONET_IPV6_ADDR_LEN],
                                          const uint8_t nd_border_router[static PYRONET_IPV6_ADDR_LEN])
{
    char destination_str[PYRONET_ROUTER_ADDR_STR_LEN];
    char global_str[PYRONET_ROUTER_ADDR_STR_LEN];
    char parent_str[PYRONET_ROUTER_ADDR_STR_LEN];
    char nd_border_router_str[PYRONET_ROUTER_ADDR_STR_LEN];
    bool changed;

    memset(destination_str, 0, sizeof(destination_str));
    memset(global_str, 0, sizeof(global_str));
    memset(parent_str, 0, sizeof(parent_str));
    memset(nd_border_router_str, 0, sizeof(nd_border_router_str));

    changed = !pyronetNcpRouteTraceContext.initialized ||
              (pyronetNcpRouteTraceContext.result != result) ||
              (pyronetNcpRouteTraceContext.source != source) ||
              (pyronetNcpRouteTraceContext.router_status != router_status) ||
              (pyronetNcpRouteTraceContext.nd_status != nd_status) ||
              (pyronetNcpRouteTraceContext.has_global != has_global) ||
              (pyronetNcpRouteTraceContext.has_parent != has_parent) ||
              (memcmp(pyronetNcpRouteTraceContext.destination, destination, PYRONET_IPV6_ADDR_LEN) != 0) ||
              (memcmp(pyronetNcpRouteTraceContext.global, global, PYRONET_IPV6_ADDR_LEN) != 0) ||
              (memcmp(pyronetNcpRouteTraceContext.parent, parent, PYRONET_IPV6_ADDR_LEN) != 0) ||
              (memcmp(pyronetNcpRouteTraceContext.nd_border_router, nd_border_router, PYRONET_IPV6_ADDR_LEN) != 0);

    if (!changed)
    {
        return;
    }

    pyronetNcpRouteTraceContext.initialized = true;
    pyronetNcpRouteTraceContext.result = result;
    pyronetNcpRouteTraceContext.source = source;
    pyronetNcpRouteTraceContext.router_status = router_status;
    pyronetNcpRouteTraceContext.nd_status = nd_status;
    pyronetNcpRouteTraceContext.has_global = has_global;
    pyronetNcpRouteTraceContext.has_parent = has_parent;
    memcpy(pyronetNcpRouteTraceContext.destination, destination, PYRONET_IPV6_ADDR_LEN);
    memcpy(pyronetNcpRouteTraceContext.global, global, PYRONET_IPV6_ADDR_LEN);
    memcpy(pyronetNcpRouteTraceContext.parent, parent, PYRONET_IPV6_ADDR_LEN);
    memcpy(pyronetNcpRouteTraceContext.nd_border_router, nd_border_router, PYRONET_IPV6_ADDR_LEN);

    (void)swoDebugPrintf("PYRONET_ROUTE_SELECT result=%s source=%s router_status=%d nd_status=%d has_gp=%u has_parent=%u",
                         pyronetNcpRouterAddressResultName(result),
                         pyronetNcpRouteSourceName(source),
                         (int)router_status,
                         (int)nd_status,
                         has_global ? 1U : 0U,
                         has_parent ? 1U : 0U);
    (void)swoDebugPrintf("PYRONET_ROUTE_ADDR dest=%s node_gp=%s parent_ll=%s nd_br=%s",
                         pyronetNcpIpv6ToString(destination, destination_str),
                         has_global ? pyronetNcpIpv6ToString(global, global_str) : "-",
                         has_parent ? pyronetNcpIpv6ToString(parent, parent_str) : "-",
                         (nd_status == 0) ? pyronetNcpIpv6ToString(nd_border_router, nd_border_router_str) : "-");

    if (has_global && has_parent)
    {
        uint8_t derived_global[PYRONET_IPV6_ADDR_LEN];
        char configured_border_router_str[PYRONET_ROUTER_ADDR_STR_LEN];
        char derived_global_str[PYRONET_ROUTER_ADDR_STR_LEN];

        memset(configured_border_router_str, 0, sizeof(configured_border_router_str));
        memset(derived_global_str, 0, sizeof(derived_global_str));
        (void)swoDebugPrintf("PYRONET_ROUTE_CAND source=configured_border_router addr=%s parent_match=%u",
                             pyronetNcpIpv6ToString(pyronetConfiguredBorderRouterIpv6, configured_border_router_str),
                             pyronetNcpIpv6IidMatches(parent, pyronetConfiguredBorderRouterIpv6) ? 1U : 0U);
        if (pyronetNcpDeriveGlobalFromPrefix(global, parent, derived_global))
        {
            (void)swoDebugPrintf("PYRONET_ROUTE_CAND source=node_gp_parent_iid addr=%s",
                                 pyronetNcpIpv6ToString(derived_global, derived_global_str));
        }
    }
}

static const char *pyronetNcpMeshStatusName(mesh_connection_status_t status)
{
    switch (status)
    {
        case MESH_CONNECTED:
            return "MESH_CONNECTED";
        case MESH_CONNECTED_LOCAL:
            return "MESH_CONNECTED_LOCAL";
        case MESH_CONNECTED_GLOBAL:
            return "MESH_CONNECTED_GLOBAL";
        case MESH_BOOTSTRAP_STARTED:
            return "MESH_BOOTSTRAP_STARTED";
        case MESH_BOOTSTRAP_START_FAILED:
            return "MESH_BOOTSTRAP_START_FAILED";
        case MESH_BOOTSTRAP_FAILED:
            return "MESH_BOOTSTRAP_FAILED";
        case MESH_DISCONNECTED:
            return "MESH_DISCONNECTED";
        default:
            return "MESH_STATUS_UNKNOWN";
    }
}

static void pyronetNcpStateResetLocked(void)
{
    pyronetNcpStateContext.interface_id = -1;
    pyronetNcpStateContext.coap_service_id = -1;
    pyronetNcpStateContext.network_state = HOST_NETWORK_STATE_DOWN;
    pyronetNcpStateContext.ever_joined = false;
    pyronetNcpStateContext.session_joined = false;
    pyronetNcpStateContext.pending_join_ready = false;
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
    memset(&pyronetNcpRouteTraceContext, 0, sizeof(pyronetNcpRouteTraceContext));
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

void pyronet_ncp_state_log_connectivity_snapshot(uint8_t host_state)
{
    int8_t interface_id;
    uint8_t link_local[PYRONET_IPV6_ADDR_LEN];
    uint8_t global[PYRONET_IPV6_ADDR_LEN];
    uint8_t secondary_global[PYRONET_IPV6_ADDR_LEN];
    uint8_t nd_prefix[PYRONET_IPV6_ADDR_LEN];
    uint8_t parent[PYRONET_IPV6_ADDR_LEN];
    uint8_t derived_parent_global[PYRONET_IPV6_ADDR_LEN];
    network_layer_address_s nd_address;
    ws_stack_info_t info;
    char link_local_str[PYRONET_ROUTER_ADDR_STR_LEN];
    char global_str[PYRONET_ROUTER_ADDR_STR_LEN];
    char secondary_global_str[PYRONET_ROUTER_ADDR_STR_LEN];
    char border_router_str[PYRONET_ROUTER_ADDR_STR_LEN];
    char nd_prefix_str[PYRONET_ROUTER_ADDR_STR_LEN];
    char parent_str[PYRONET_ROUTER_ADDR_STR_LEN];
    char derived_parent_global_str[PYRONET_ROUTER_ADDR_STR_LEN];
    bool has_link_local = false;
    bool has_global = false;
    bool has_secondary_global = false;
    bool has_parent = false;
    bool has_derived_parent_global = false;
    int8_t nd_status;
    int stack_status;

    memset(link_local, 0, sizeof(link_local));
    memset(global, 0, sizeof(global));
    memset(secondary_global, 0, sizeof(secondary_global));
    memset(nd_prefix, 0, sizeof(nd_prefix));
    memset(parent, 0, sizeof(parent));
    memset(derived_parent_global, 0, sizeof(derived_parent_global));
    memset(&nd_address, 0, sizeof(nd_address));
    memset(&info, 0, sizeof(info));
    memset(link_local_str, 0, sizeof(link_local_str));
    memset(global_str, 0, sizeof(global_str));
    memset(secondary_global_str, 0, sizeof(secondary_global_str));
    memset(border_router_str, 0, sizeof(border_router_str));
    memset(nd_prefix_str, 0, sizeof(nd_prefix_str));
    memset(parent_str, 0, sizeof(parent_str));
    memset(derived_parent_global_str, 0, sizeof(derived_parent_global_str));

    pyronetNcpStateLock();
    interface_id = pyronetNcpStateContext.interface_id;
    pyronetNcpStateUnlock();

    if (interface_id < 0)
    {
        (void)swoDebugPrintf("PYRONET_ADDR_NODE host_state=%u if=%d ll=- node_gp=- node_gp_sec=-",
                             (unsigned int)host_state,
                             (int)interface_id);
        (void)swoDebugPrintf("PYRONET_ADDR_ND if=%d status=-128 br=- prefix=-", (int)interface_id);
        (void)swoDebugPrintf("PYRONET_ADDR_PARENT if=%d status=-128 ll=- cand_gp=-", (int)interface_id);
        return;
    }

    nanostack_lock();
    has_link_local = (arm_net_address_get(interface_id, ADDR_IPV6_LL, link_local) == 0);
    has_global = (arm_net_address_get(interface_id, ADDR_IPV6_GP, global) == 0);
    has_secondary_global = (arm_net_address_get(interface_id, ADDR_IPV6_GP_SEC, secondary_global) == 0);
    nd_status = arm_nwk_nd_address_read(interface_id, &nd_address);
    if (nd_status == 0)
    {
        memcpy(nd_prefix, nd_address.prefix, sizeof(nd_address.prefix));
    }
    stack_status = ws_stack_info_get(interface_id, &info);
    if (stack_status == 0)
    {
        memcpy(parent, info.parent, PYRONET_IPV6_ADDR_LEN);
        has_parent = (memcmp(parent, pyronetZeroIpv6, PYRONET_IPV6_ADDR_LEN) != 0);
    }
    nanostack_unlock();

    if (has_link_local)
    {
        ip6tos(link_local, link_local_str);
    }

    if (has_global)
    {
        ip6tos(global, global_str);
    }

    if (has_secondary_global)
    {
        ip6tos(secondary_global, secondary_global_str);
    }

    if (nd_status == 0)
    {
        ip6tos(nd_address.border_router, border_router_str);
        ip6tos(nd_prefix, nd_prefix_str);
    }

    if (has_parent)
    {
        ip6tos(parent, parent_str);
    }

    has_derived_parent_global = has_global &&
                                has_parent &&
                                pyronetNcpDeriveGlobalFromPrefix(global,
                                                                 parent,
                                                                 derived_parent_global);
    if (has_derived_parent_global)
    {
        ip6tos(derived_parent_global, derived_parent_global_str);
    }

    (void)swoDebugPrintf("PYRONET_ADDR_NODE host_state=%u if=%d ll=%s node_gp=%s node_gp_sec=%s",
                         (unsigned int)host_state,
                         (int)interface_id,
                         has_link_local ? link_local_str : "-",
                         has_global ? global_str : "-",
                         has_secondary_global ? secondary_global_str : "-");
    (void)swoDebugPrintf("PYRONET_ADDR_ND if=%d status=%d br=%s prefix=%s",
                         (int)interface_id,
                         (int)nd_status,
                         (nd_status == 0) ? border_router_str : "-",
                         (nd_status == 0) ? nd_prefix_str : "-");
    (void)swoDebugPrintf("PYRONET_ADDR_PARENT if=%d status=%d ll=%s cand_gp=%s",
                         (int)interface_id,
                         stack_status,
                         has_parent ? parent_str : "-",
                         has_derived_parent_global ? derived_parent_global_str : "-");
    (void)swoDebugPrintf("PYRONET_PARENT_METRIC if=%d join=%u pan=%u cost=%u rsl_in=%u rsl_out=%u",
                         (int)interface_id,
                         (stack_status == 0) ? (unsigned int)info.join_state : 0U,
                         (stack_status == 0) ? (unsigned int)info.pan_id : 0U,
                         (stack_status == 0) ? (unsigned int)info.routing_cost : 0U,
                         (stack_status == 0) ? (unsigned int)info.rsl_in : 0U,
                         (stack_status == 0) ? (unsigned int)info.rsl_out : 0U);
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

    if ((interface_id < 0) || (network_state == HOST_NETWORK_STATE_DOWN))
    {
        return false;
    }

    memset(&info, 0, sizeof(info));
    nanostack_lock();
    if (ws_stack_info_get(interface_id, &info) < 0)
    {
        nanostack_unlock();
        return false;
    }
    nanostack_unlock();

    memcpy(parent_out, info.parent, PYRONET_IPV6_ADDR_LEN);
    return (memcmp(parent_out, pyronetZeroIpv6, PYRONET_IPV6_ADDR_LEN) != 0);
}

bool pyronet_ncp_state_read_current_parent_global(uint8_t parent_out[static PYRONET_IPV6_ADDR_LEN])
{
    uint8_t parent[PYRONET_IPV6_ADDR_LEN];
    uint8_t global[PYRONET_IPV6_ADDR_LEN];
    int8_t interface_id;
    uint8_t network_state;
    bool has_global;

    memset(parent_out, 0, PYRONET_IPV6_ADDR_LEN);
    memset(parent, 0, sizeof(parent));
    memset(global, 0, sizeof(global));

    pyronetNcpStateLock();
    interface_id = pyronetNcpStateContext.interface_id;
    network_state = pyronetNcpStateContext.network_state;
    pyronetNcpStateUnlock();

    if ((interface_id < 0) || (network_state == HOST_NETWORK_STATE_DOWN))
    {
        return false;
    }

    nanostack_lock();
    has_global = (arm_net_address_get(interface_id, ADDR_IPV6_GP, global) == 0);
    nanostack_unlock();

    if (!has_global || !pyronet_ncp_state_read_current_parent(parent))
    {
        return false;
    }

    return pyronetNcpDeriveGlobalFromPrefix(global, parent, parent_out);
}

pyronet_ncp_router_address_result_t pyronet_ncp_state_read_router_address(
  uint8_t destination_out[static PYRONET_IPV6_ADDR_LEN])
{
    char address[PYRONET_ROUTER_ADDR_STR_LEN];
    int8_t interface_id;
    uint8_t network_state;
    uint8_t global[PYRONET_IPV6_ADDR_LEN];
    uint8_t parent[PYRONET_IPV6_ADDR_LEN];
    uint8_t nd_border_router[PYRONET_IPV6_ADDR_LEN];
    network_layer_address_s nd_address;
    int8_t router_status = -1;
    int8_t nd_status = -1;
    bool has_global = false;
    bool has_parent = false;

    memset(destination_out, 0, PYRONET_IPV6_ADDR_LEN);
    memset(address, 0, sizeof(address));
    memset(global, 0, sizeof(global));
    memset(parent, 0, sizeof(parent));
    memset(nd_border_router, 0, sizeof(nd_border_router));
    memset(&nd_address, 0, sizeof(nd_address));

    pyronetNcpStateLock();
    interface_id = pyronetNcpStateContext.interface_id;
    network_state = pyronetNcpStateContext.network_state;
    pyronetNcpStateUnlock();

    if ((interface_id < 0) || (network_state == HOST_NETWORK_STATE_DOWN))
    {
        pyronetNcpTraceRouteSelection(PYRONET_NCP_ROUTER_ADDRESS_NOT_JOINED,
                                      PYRONET_NCP_ROUTE_SOURCE_NONE,
                                      router_status,
                                      nd_status,
                                      false,
                                      false,
                                      destination_out,
                                      global,
                                      parent,
                                      nd_border_router);
        return PYRONET_NCP_ROUTER_ADDRESS_NOT_JOINED;
    }

    nanostack_lock();
    router_status = wisun_tasklet_get_router_ip_address(address, (int8_t)sizeof(address));
    nd_status = arm_nwk_nd_address_read(interface_id, &nd_address);
    if (nd_status == 0)
    {
        memcpy(nd_border_router, nd_address.border_router, PYRONET_IPV6_ADDR_LEN);
    }
    has_global = (arm_net_address_get(interface_id, ADDR_IPV6_GP, global) == 0);
    nanostack_unlock();

    has_parent = pyronet_ncp_state_read_current_parent(parent);

    if (router_status == 0)
    {
        if (!stoip6(address, strlen(address), destination_out))
        {
            pyronetNcpTraceRouteSelection(PYRONET_NCP_ROUTER_ADDRESS_INVALID,
                                          PYRONET_NCP_ROUTE_SOURCE_SDK_ROUTER,
                                          router_status,
                                          nd_status,
                                          has_global,
                                          has_parent,
                                          destination_out,
                                          global,
                                          parent,
                                          nd_border_router);
            return PYRONET_NCP_ROUTER_ADDRESS_INVALID;
        }

        pyronetNcpTraceRouteSelection(PYRONET_NCP_ROUTER_ADDRESS_READY,
                                      PYRONET_NCP_ROUTE_SOURCE_SDK_ROUTER,
                                      router_status,
                                      nd_status,
                                      has_global,
                                      has_parent,
                                      destination_out,
                                      global,
                                      parent,
                                      nd_border_router);
        return PYRONET_NCP_ROUTER_ADDRESS_READY;
    }

    if ((nd_status == 0) &&
        (memcmp(nd_border_router, pyronetZeroIpv6, PYRONET_IPV6_ADDR_LEN) != 0))
    {
        memcpy(destination_out, nd_border_router, PYRONET_IPV6_ADDR_LEN);
        pyronetNcpTraceRouteSelection(PYRONET_NCP_ROUTER_ADDRESS_READY,
                                      PYRONET_NCP_ROUTE_SOURCE_ND_BORDER_ROUTER,
                                      router_status,
                                      nd_status,
                                      has_global,
                                      has_parent,
                                      destination_out,
                                      global,
                                      parent,
                                      nd_border_router);
        return PYRONET_NCP_ROUTER_ADDRESS_READY;
    }

    if (has_global)
    {
        memcpy(destination_out, pyronetConfiguredBorderRouterIpv6, PYRONET_IPV6_ADDR_LEN);
        pyronetNcpTraceRouteSelection(PYRONET_NCP_ROUTER_ADDRESS_READY,
                                      PYRONET_NCP_ROUTE_SOURCE_CONFIGURED_BORDER_ROUTER,
                                      router_status,
                                      nd_status,
                                      has_global,
                                      has_parent,
                                      destination_out,
                                      global,
                                      parent,
                                      nd_border_router);
        return PYRONET_NCP_ROUTER_ADDRESS_READY;
    }

    if (has_global && has_parent)
    {
        /*
         * Last-resort diagnostic fallback only. In a multihop mesh, the RPL
         * parent can be a relay and is not necessarily the border router.
         */
        if (!pyronetNcpDeriveGlobalFromPrefix(global, parent, destination_out))
        {
            pyronetNcpTraceRouteSelection(PYRONET_NCP_ROUTER_ADDRESS_UNAVAILABLE,
                                          PYRONET_NCP_ROUTE_SOURCE_NONE,
                                          router_status,
                                          nd_status,
                                          has_global,
                                          has_parent,
                                          destination_out,
                                          global,
                                          parent,
                                          nd_border_router);
            return PYRONET_NCP_ROUTER_ADDRESS_UNAVAILABLE;
        }

        pyronetNcpTraceRouteSelection(PYRONET_NCP_ROUTER_ADDRESS_READY,
                                      PYRONET_NCP_ROUTE_SOURCE_DERIVED_PARENT_GLOBAL,
                                      router_status,
                                      nd_status,
                                      has_global,
                                      has_parent,
                                      destination_out,
                                      global,
                                      parent,
                                      nd_border_router);
        return PYRONET_NCP_ROUTER_ADDRESS_READY;
    }

    pyronetNcpTraceRouteSelection(PYRONET_NCP_ROUTER_ADDRESS_UNAVAILABLE,
                                  PYRONET_NCP_ROUTE_SOURCE_NONE,
                                  router_status,
                                  nd_status,
                                  has_global,
                                  has_parent,
                                  destination_out,
                                  global,
                                  parent,
                                  nd_border_router);
    return PYRONET_NCP_ROUTER_ADDRESS_UNAVAILABLE;
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
    uint8_t network_state;

    pyronetNcpStateLock();

    switch (status)
    {
        case MESH_CONNECTED:
        case MESH_CONNECTED_LOCAL:
        case MESH_CONNECTED_GLOBAL:
            if (!pyronetNcpStateContext.session_joined)
            {
                pyronetNcpStateContext.network_state = HOST_NETWORK_STATE_JOINING;
                pyronetNcpStateContext.pending_join_ready = true;
                pyronetNcpStateContext.parent_initialized = false;
                memset(pyronetNcpStateContext.last_parent_ipv6, 0, sizeof(pyronetNcpStateContext.last_parent_ipv6));
            }
            else
            {
                pyronetNcpStateContext.network_state = HOST_NETWORK_STATE_JOINED;
            }
            break;

        case MESH_BOOTSTRAP_STARTED:
            pyronetNcpStateContext.network_state = HOST_NETWORK_STATE_JOINING;
            pyronetNcpStateContext.session_joined = false;
            pyronetNcpStateContext.pending_join_ready = false;
            pyronetNcpStateContext.parent_initialized = false;
            memset(pyronetNcpStateContext.last_parent_ipv6, 0, sizeof(pyronetNcpStateContext.last_parent_ipv6));
            break;

        case MESH_DISCONNECTED:
        case MESH_BOOTSTRAP_START_FAILED:
        case MESH_BOOTSTRAP_FAILED:
        default:
            pyronetNcpStateContext.network_state = HOST_NETWORK_STATE_DOWN;
            pyronetNcpStateContext.session_joined = false;
            pyronetNcpStateContext.pending_join_ready = false;
            pyronetNcpStateContext.parent_initialized = false;
            memset(pyronetNcpStateContext.last_parent_ipv6, 0, sizeof(pyronetNcpStateContext.last_parent_ipv6));
            break;
    }

    network_state = pyronetNcpStateContext.network_state;
    pyronetNcpStateUnlock();

    (void)swoDebugPrintf("PYRONET_WISUN_STATUS status=%s host_state=%u queue_registration=%u",
                         pyronetNcpMeshStatusName(status),
                         (unsigned int)network_state,
                         0U);
}

void pyronet_ncp_state_poll_connectivity(void)
{
    uint8_t destination[PYRONET_IPV6_ADDR_LEN];
    bool queue_registration = false;
    uint8_t reason = PYRONET_REG_REASON_JOIN;

    pyronetNcpStateLock();
    if (!pyronetNcpStateContext.pending_join_ready || pyronetNcpStateContext.session_joined)
    {
        pyronetNcpStateUnlock();
        return;
    }
    pyronetNcpStateUnlock();

    if (pyronet_ncp_state_read_router_address(destination) != PYRONET_NCP_ROUTER_ADDRESS_READY)
    {
        return;
    }

    pyronetNcpStateLock();
    if (pyronetNcpStateContext.pending_join_ready && !pyronetNcpStateContext.session_joined)
    {
        pyronetNcpStateContext.network_state = HOST_NETWORK_STATE_JOINED;
        pyronetNcpStateContext.session_joined = true;
        pyronetNcpStateContext.pending_join_ready = false;
        pyronetNcpStateContext.registration_required = true;
        pyronetNcpStateContext.parent_initialized = false;
        memset(pyronetNcpStateContext.last_parent_ipv6, 0, sizeof(pyronetNcpStateContext.last_parent_ipv6));
        reason = pyronetNcpStateContext.ever_joined ? PYRONET_REG_REASON_REJOIN : PYRONET_REG_REASON_JOIN;
        pyronetNcpStateContext.ever_joined = true;
        queue_registration = true;
    }
    pyronetNcpStateUnlock();

    if (queue_registration)
    {
        (void)swoDebugPrintf("PYRONET_WISUN_READY reason=%u", (unsigned int)reason);
        pyronet_ncp_events_queue_registration_needed(reason);
    }
}

bool pyronet_ncp_state_request_registration_sync(uint8_t *reason_out)
{
    bool should_queue = false;
    uint8_t reason = PYRONET_REG_REASON_JOIN;

    pyronetNcpStateLock();
    if (pyronetNcpStateContext.network_state == HOST_NETWORK_STATE_JOINED)
    {
        pyronetNcpStateContext.registration_required = true;
        reason = pyronetNcpStateContext.ever_joined ? PYRONET_REG_REASON_REJOIN : PYRONET_REG_REASON_JOIN;
        should_queue = true;
    }
    pyronetNcpStateUnlock();

    if (reason_out != NULL)
    {
        *reason_out = reason;
    }

    return should_queue;
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
