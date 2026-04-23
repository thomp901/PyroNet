#ifndef PYRONET_NCP_H
#define PYRONET_NCP_H

#include <stdbool.h>
#include <stdint.h>

#include "host/host_proto.h"

typedef struct pyronet_ncp_event
{
    uint8_t msg_type;
    uint8_t payload_len;
    union
    {
        pyronet_host_registration_needed_v1_t registration_needed;
        pyronet_host_parent_changed_v1_t parent_changed;
        pyronet_host_tx_result_v1_t tx_result;
        pyronet_host_time_sync_update_v1_t time_sync_update;
        pyronet_host_neighbor_alert_received_v1_t neighbor_alert_rx;
        pyronet_host_config_update_received_v1_t config_update_rx;
    } payload;
} pyronet_ncp_event_t;

bool pyronet_ncp_init(void);
void pyronet_ncp_poll(void);
uint8_t pyronet_ncp_network_state(void);
bool pyronet_ncp_next_event(pyronet_ncp_event_t *out_event);

void pyronet_ncp_send_registration(const pyronet_host_send_registration_v1_t *command);
void pyronet_ncp_send_sensor_report(const pyronet_host_send_sensor_report_v1_t *command);
void pyronet_ncp_send_sensor_alert(const pyronet_host_send_sensor_alert_v1_t *command);
void pyronet_ncp_send_neighbor_alert(const pyronet_host_send_neighbor_alert_v1_t *command);
void pyronet_ncp_send_parent_update(const pyronet_host_request_parent_update_v1_t *command);

#endif /* PYRONET_NCP_H */
