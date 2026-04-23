#include "../pyronet_ncp.h"

#include <string.h>

#include "../swo_debug.h"

#include "coap_service_api.h"
#include "mbed-coap/sn_coap_header.h"

#include "pyronet_ncp_events.h"
#include "pyronet_ncp_local.h"
#include "pyronet_ncp_pending_tx.h"
#include "pyronet_ncp_state.h"

static int pyronetNcpCoapResponseCallback(int8_t service_id,
                                          uint8_t source_address[static 16],
                                          uint16_t source_port,
                                          sn_coap_hdr_s *response_ptr)
{
    uint8_t request_type = 0U;
    uint8_t detail = 0U;

    (void)service_id;

    if (!pyronet_ncp_pending_tx_match_and_consume((response_ptr != NULL) ? response_ptr->msg_id : 0U,
                                                  source_address,
                                                  source_port,
                                                  &request_type,
                                                  &detail))
    {
        return 0;
    }

    if (response_ptr == NULL)
    {
        pyronet_ncp_events_queue_tx_result(request_type, PYRONET_TX_STATUS_TIMEOUT, detail);
        return 0;
    }

    if (response_ptr->coap_status == COAP_STATUS_BUILDER_MESSAGE_SENDING_FAILED)
    {
        pyronet_ncp_events_queue_tx_result(request_type, PYRONET_TX_STATUS_TIMEOUT, detail);
        return 0;
    }

    if (response_ptr->coap_status != COAP_STATUS_OK)
    {
        pyronet_ncp_events_queue_tx_result(request_type, PYRONET_TX_STATUS_FAILED, detail);
        return 0;
    }

    if (response_ptr->msg_code < COAP_MSG_CODE_RESPONSE_BAD_REQUEST)
    {
        pyronet_ncp_events_queue_tx_result(request_type, PYRONET_TX_STATUS_ACKED, detail);
    }
    else
    {
        pyronet_ncp_events_queue_tx_result(request_type, PYRONET_TX_STATUS_FAILED, detail);
    }

    return 0;
}

static bool pyronetNcpSendCoapRequest(uint8_t request_type,
                                      const uint8_t destination[static PYRONET_IPV6_ADDR_LEN],
                                      const char *uri,
                                      sn_coap_msg_type_e msg_type,
                                      const uint8_t *payload,
                                      uint16_t payload_len,
                                      uint8_t detail)
{
    uint16_t msg_id;
    int8_t service_id;
    bool confirmable;
    bool has_reserved_slot = false;
    uint8_t reserved_slot = 0U;

    service_id = pyronet_ncp_state_coap_service_id();
    confirmable = (msg_type == COAP_MSG_TYPE_CONFIRMABLE);

    if (service_id < 0)
    {
        pyronet_ncp_events_queue_tx_result(request_type, PYRONET_TX_STATUS_FAILED, detail);
        return false;
    }

    /*
     * Reserve confirmable tracking before submit so any request that reports
     * SENT is guaranteed to have a terminal ACKED/FAILED/TIMEOUT outcome.
     */
    if (confirmable &&
        !pyronet_ncp_pending_tx_reserve(&reserved_slot,
                                        PYRONET_COAP_PORT,
                                        destination,
                                        request_type,
                                        detail))
    {
        (void)swoDebugPrintf("PYRONET_TX_TRACK_EXHAUSTED type=%u detail=%u",
                             (unsigned int)request_type,
                             (unsigned int)detail);
        pyronet_ncp_events_queue_tx_result(request_type, PYRONET_TX_STATUS_FAILED, detail);
        return false;
    }
    has_reserved_slot = confirmable;

    msg_id = coap_service_request_send(service_id,
                                       COAP_REQUEST_OPTIONS_NONE,
                                       destination,
                                       PYRONET_COAP_PORT,
                                       msg_type,
                                       COAP_MSG_CODE_REQUEST_POST,
                                       uri,
                                       COAP_CT_OCTET_STREAM,
                                       payload,
                                       payload_len,
                                       confirmable ? pyronetNcpCoapResponseCallback : NULL);

    if (msg_id == 0U)
    {
        if (has_reserved_slot)
        {
            pyronet_ncp_pending_tx_release(reserved_slot);
        }
        pyronet_ncp_events_queue_tx_result(request_type, PYRONET_TX_STATUS_FAILED, detail);
        return false;
    }

    if (has_reserved_slot && !pyronet_ncp_pending_tx_commit(reserved_slot, msg_id))
    {
        /*
         * This should not happen after a successful reservation. Treat it as a
         * deterministic send failure rather than emitting SENT without a
         * reconcilable terminal result.
         */
        (void)swoDebugPrintf("PYRONET_TX_TRACK_COMMIT_FAILED type=%u detail=%u",
                             (unsigned int)request_type,
                             (unsigned int)detail);
        pyronet_ncp_events_queue_tx_result(request_type, PYRONET_TX_STATUS_FAILED, detail);
        return false;
    }

    pyronet_ncp_events_queue_tx_result(request_type, PYRONET_TX_STATUS_SENT, detail);
    return true;
}

void pyronet_ncp_send_registration(const pyronet_host_send_registration_v1_t *command)
{
    pyronet_mesh_registration_v1_t mesh_packet;
    uint8_t destination[PYRONET_IPV6_ADDR_LEN];

    if (command == NULL)
    {
        pyronet_ncp_events_queue_tx_result(PYRONET_HOST_MSG_SEND_REGISTRATION, PYRONET_TX_STATUS_FAILED, 0U);
        return;
    }

    pyronet_ncp_state_remember_local_node_id(command->node_id);

    memset(&mesh_packet, 0, sizeof(mesh_packet));
    mesh_packet.type = PYRONET_PKT_REGISTRATION;
    mesh_packet.version = PYRONET_MESH_SCHEMA_VERSION;
    mesh_packet.node_id = command->node_id;
    mesh_packet.latitude = command->latitude;
    mesh_packet.longitude = command->longitude;
    mesh_packet.fw_version = command->fw_version;
    mesh_packet.battery_pct = command->battery_pct;
    (void)pyronet_ncp_state_read_current_parent(mesh_packet.parent_ipv6);

    if (!pyronet_ncp_state_read_router_address(destination))
    {
        pyronet_ncp_events_queue_tx_result(PYRONET_HOST_MSG_SEND_REGISTRATION, PYRONET_TX_STATUS_FAILED, 0U);
        return;
    }

    /*
     * Preserve registration_required until the registration request is accepted by
     * the CoAP service. Route lookup and pre-submit failures must not lose the
     * queued registration-needed condition.
     */
    if (pyronetNcpSendCoapRequest(PYRONET_HOST_MSG_SEND_REGISTRATION,
                                  destination,
                                  PYRONET_COAP_UPLINK_URI,
                                  COAP_MSG_TYPE_CONFIRMABLE,
                                  (const uint8_t *)&mesh_packet,
                                  sizeof(mesh_packet),
                                  0U))
    {
        pyronet_ncp_state_clear_registration_required();
    }
}

void pyronet_ncp_send_sensor_report(const pyronet_host_send_sensor_report_v1_t *command)
{
    pyronet_mesh_sensor_report_v1_t mesh_packet;
    uint8_t destination[PYRONET_IPV6_ADDR_LEN];

    if (command == NULL)
    {
        pyronet_ncp_events_queue_tx_result(PYRONET_HOST_MSG_SEND_SENSOR_REPORT, PYRONET_TX_STATUS_FAILED, 0U);
        return;
    }

    pyronet_ncp_state_remember_local_node_id(command->node_id);

    memset(&mesh_packet, 0, sizeof(mesh_packet));
    mesh_packet.type = PYRONET_PKT_SENSOR_REPORT;
    mesh_packet.version = PYRONET_MESH_SCHEMA_VERSION;
    mesh_packet.node_id = command->node_id;
    mesh_packet.timestamp = command->timestamp;
    mesh_packet.risk_level = command->risk_level;
    mesh_packet.temperature = command->temperature_c_x100;
    mesh_packet.humidity = command->humidity_pct_x100;
    mesh_packet.bvoc_ppm = command->bvoc_ppm; /* Preserved as ppm until scaling is finalized in the wiki. */
    mesh_packet.pm25 = command->pm25_ug_m3_x10;
    mesh_packet.battery_pct = command->battery_pct;

    if (!pyronet_ncp_state_read_router_address(destination))
    {
        pyronet_ncp_events_queue_tx_result(PYRONET_HOST_MSG_SEND_SENSOR_REPORT, PYRONET_TX_STATUS_FAILED, 0U);
        return;
    }

    (void)pyronetNcpSendCoapRequest(PYRONET_HOST_MSG_SEND_SENSOR_REPORT,
                                    destination,
                                    PYRONET_COAP_UPLINK_URI,
                                    COAP_MSG_TYPE_NON_CONFIRMABLE,
                                    (const uint8_t *)&mesh_packet,
                                    sizeof(mesh_packet),
                                    0U);
}

void pyronet_ncp_send_sensor_alert(const pyronet_host_send_sensor_alert_v1_t *command)
{
    pyronet_mesh_sensor_alert_v1_t mesh_packet;
    uint8_t destination[PYRONET_IPV6_ADDR_LEN];

    if (command == NULL)
    {
        pyronet_ncp_events_queue_tx_result(PYRONET_HOST_MSG_SEND_SENSOR_ALERT, PYRONET_TX_STATUS_FAILED, 0U);
        return;
    }

    pyronet_ncp_state_remember_local_node_id(command->node_id);

    memset(&mesh_packet, 0, sizeof(mesh_packet));
    mesh_packet.type = PYRONET_PKT_SENSOR_ALERT;
    mesh_packet.version = PYRONET_MESH_SCHEMA_VERSION;
    mesh_packet.node_id = command->node_id;
    mesh_packet.timestamp = command->timestamp;
    mesh_packet.risk_level = command->risk_level;
    mesh_packet.temperature = command->temperature_c_x100;
    mesh_packet.humidity = command->humidity_pct_x100;
    mesh_packet.bvoc_ppm = command->bvoc_ppm; /* Preserved as ppm until scaling is finalized in the wiki. */
    mesh_packet.pm25 = command->pm25_ug_m3_x10;
    mesh_packet.battery_pct = command->battery_pct;

    if (!pyronet_ncp_state_read_router_address(destination))
    {
        pyronet_ncp_events_queue_tx_result(PYRONET_HOST_MSG_SEND_SENSOR_ALERT, PYRONET_TX_STATUS_FAILED, 0U);
        return;
    }

    (void)pyronetNcpSendCoapRequest(PYRONET_HOST_MSG_SEND_SENSOR_ALERT,
                                    destination,
                                    PYRONET_COAP_UPLINK_URI,
                                    COAP_MSG_TYPE_CONFIRMABLE,
                                    (const uint8_t *)&mesh_packet,
                                    sizeof(mesh_packet),
                                    0U);
}

void pyronet_ncp_send_neighbor_alert(const pyronet_host_send_neighbor_alert_v1_t *command)
{
    pyronet_mesh_neighbor_alert_v1_t mesh_packet;
    uint8_t neighbors[PYRONET_MAX_NEIGHBORS][PYRONET_IPV6_ADDR_LEN];
    uint8_t neighbor_count;
    uint8_t index;

    if (command == NULL)
    {
        pyronet_ncp_events_queue_tx_result(PYRONET_HOST_MSG_SEND_NEIGHBOR_ALERT, PYRONET_TX_STATUS_FAILED, 0U);
        return;
    }

    pyronet_ncp_state_remember_local_node_id(command->node_id);

    memset(&mesh_packet, 0, sizeof(mesh_packet));
    mesh_packet.type = PYRONET_PKT_NEIGHBOR_ALERT;
    mesh_packet.version = PYRONET_MESH_SCHEMA_VERSION;
    mesh_packet.node_id = command->node_id;
    mesh_packet.risk_level = command->risk_level;
    mesh_packet.timestamp = command->timestamp;

    neighbor_count = pyronet_ncp_state_copy_neighbors(neighbors);
    if (neighbor_count == 0U)
    {
        pyronet_ncp_events_queue_tx_result(PYRONET_HOST_MSG_SEND_NEIGHBOR_ALERT, PYRONET_TX_STATUS_FAILED, 0U);
        return;
    }

    for (index = 0; index < neighbor_count; ++index)
    {
        (void)pyronetNcpSendCoapRequest(PYRONET_HOST_MSG_SEND_NEIGHBOR_ALERT,
                                        neighbors[index],
                                        PYRONET_COAP_LATERAL_URI,
                                        COAP_MSG_TYPE_CONFIRMABLE,
                                        (const uint8_t *)&mesh_packet,
                                        sizeof(mesh_packet),
                                        (uint8_t)(index + 1U));
    }
}

void pyronet_ncp_send_parent_update(const pyronet_host_request_parent_update_v1_t *command)
{
    pyronet_mesh_parent_update_v1_t mesh_packet;
    uint8_t destination[PYRONET_IPV6_ADDR_LEN];

    if (command == NULL)
    {
        pyronet_ncp_events_queue_tx_result(PYRONET_HOST_MSG_REQUEST_PARENT_UPDATE, PYRONET_TX_STATUS_FAILED, 0U);
        return;
    }

    pyronet_ncp_state_remember_local_node_id(command->node_id);

    memset(&mesh_packet, 0, sizeof(mesh_packet));
    mesh_packet.type = PYRONET_PKT_PARENT_UPDATE;
    mesh_packet.version = PYRONET_MESH_SCHEMA_VERSION;
    mesh_packet.node_id = command->node_id;
    mesh_packet.timestamp = command->timestamp;
    (void)pyronet_ncp_state_read_current_parent(mesh_packet.parent_ipv6);

    if (!pyronet_ncp_state_read_router_address(destination))
    {
        pyronet_ncp_events_queue_tx_result(PYRONET_HOST_MSG_REQUEST_PARENT_UPDATE, PYRONET_TX_STATUS_FAILED, 0U);
        return;
    }

    (void)pyronetNcpSendCoapRequest(PYRONET_HOST_MSG_REQUEST_PARENT_UPDATE,
                                    destination,
                                    PYRONET_COAP_UPLINK_URI,
                                    COAP_MSG_TYPE_CONFIRMABLE,
                                    (const uint8_t *)&mesh_packet,
                                    sizeof(mesh_packet),
                                    0U);
}
