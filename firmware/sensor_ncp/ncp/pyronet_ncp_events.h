#ifndef NCP_PYRONET_NCP_EVENTS_H
#define NCP_PYRONET_NCP_EVENTS_H

#include <stdbool.h>
#include <stdint.h>

#include "../pyronet_ncp.h"

bool pyronet_ncp_events_init(void);
bool pyronet_ncp_events_next(pyronet_ncp_event_t *out_event);

void pyronet_ncp_events_queue_registration_needed(uint8_t reason);
void pyronet_ncp_events_queue_parent_changed(uint8_t change_reason);
void pyronet_ncp_events_queue_tx_result(uint8_t request_type, uint8_t status, uint8_t detail);
void pyronet_ncp_events_queue_time_sync_update(uint32_t unix_time_s);
void pyronet_ncp_events_queue_neighbor_alert_rx(uint16_t node_id, uint8_t risk_level, uint32_t timestamp);
void pyronet_ncp_events_queue_config_update_rx(const pyronet_host_config_update_received_v1_t *config);

#endif /* NCP_PYRONET_NCP_EVENTS_H */
