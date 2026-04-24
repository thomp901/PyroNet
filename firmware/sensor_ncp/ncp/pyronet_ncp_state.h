#ifndef NCP_PYRONET_NCP_STATE_H
#define NCP_PYRONET_NCP_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "wisun_tasklet.h"

#include "pyronet_mesh_decode.h"

typedef enum pyronet_ncp_neighbor_table_result
{
    PYRONET_NCP_NEIGHBOR_TABLE_APPLIED = 0,
    PYRONET_NCP_NEIGHBOR_TABLE_DEFERRED = 1,
    PYRONET_NCP_NEIGHBOR_TABLE_REJECTED_TARGET = 2,
} pyronet_ncp_neighbor_table_result_t;

typedef enum pyronet_ncp_router_address_result
{
    PYRONET_NCP_ROUTER_ADDRESS_READY = 0,
    PYRONET_NCP_ROUTER_ADDRESS_NOT_JOINED = 1,
    PYRONET_NCP_ROUTER_ADDRESS_UNAVAILABLE = 2,
    PYRONET_NCP_ROUTER_ADDRESS_INVALID = 3,
} pyronet_ncp_router_address_result_t;

bool pyronet_ncp_state_init(void);
void pyronet_ncp_state_set_transport(int8_t interface_id, int8_t coap_service_id);
int8_t pyronet_ncp_state_coap_service_id(void);
void pyronet_ncp_state_set_network_state(uint8_t network_state);
uint8_t pyronet_ncp_state_network_state(void);
void pyronet_ncp_state_handle_network_status(mesh_connection_status_t status);
void pyronet_ncp_state_log_connectivity_snapshot(uint8_t host_state);
void pyronet_ncp_state_poll_connectivity(void);
void pyronet_ncp_state_poll_parent(void);
bool pyronet_ncp_state_request_registration_sync(uint8_t *reason_out);

void pyronet_ncp_state_clear_registration_required(void);
void pyronet_ncp_state_remember_local_node_id(uint16_t node_id);
bool pyronet_ncp_state_read_current_parent(uint8_t parent_out[static PYRONET_IPV6_ADDR_LEN]);
bool pyronet_ncp_state_read_current_parent_global(uint8_t parent_out[static PYRONET_IPV6_ADDR_LEN]);
pyronet_ncp_router_address_result_t pyronet_ncp_state_read_router_address(
  uint8_t destination_out[static PYRONET_IPV6_ADDR_LEN]);
uint8_t pyronet_ncp_state_copy_neighbors(uint8_t neighbors_out[PYRONET_MAX_NEIGHBORS][PYRONET_IPV6_ADDR_LEN]);
pyronet_ncp_neighbor_table_result_t pyronet_ncp_state_accept_neighbor_table(
  const pyronet_mesh_nn_table_update_view_t *update);

#endif /* NCP_PYRONET_NCP_STATE_H */
