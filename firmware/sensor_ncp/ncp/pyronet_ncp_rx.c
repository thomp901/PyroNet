#include "pyronet_ncp_rx.h"

#include <string.h>

#include "../swo_debug.h"

#include "pyronet_mesh_decode.h"
#include "pyronet_ncp_events.h"
#include "pyronet_ncp_state.h"

static int pyronetNcpRxSendResponse(int8_t service_id, sn_coap_hdr_s *request_ptr, uint8_t response_code)
{
    coap_service_response_send(service_id,
                               COAP_REQUEST_OPTIONS_NONE,
                               request_ptr,
                               response_code,
                               COAP_CT_NONE,
                               NULL,
                               0U);
    return 0;
}

static int pyronetNcpRxHandleNeighborTableUpdate(int8_t service_id, sn_coap_hdr_s *request_ptr)
{
    pyronet_mesh_nn_table_update_view_t update;
    pyronet_ncp_neighbor_table_result_t result;

    if (!pyronet_mesh_decode_neighbor_table_update(request_ptr->payload_ptr, request_ptr->payload_len, &update))
    {
        if ((request_ptr->payload_len >= sizeof(pyronet_mesh_nn_table_update_header_v1_t)) &&
            (request_ptr->payload_ptr != NULL))
        {
            pyronet_mesh_nn_table_update_header_v1_t header;

            memcpy(&header, request_ptr->payload_ptr, sizeof(header));
            if ((header.type == PYRONET_PKT_NN_TABLE_UPDATE) && (header.neighbor_count > PYRONET_MAX_NEIGHBORS))
            {
                (void)swoDebugPrintf("PYRONET_NN_TABLE_REJECT count=%u max=%u",
                                     (unsigned int)header.neighbor_count,
                                     (unsigned int)PYRONET_MAX_NEIGHBORS);
            }
        }

        return pyronetNcpRxSendResponse(service_id, request_ptr, COAP_MSG_CODE_RESPONSE_BAD_REQUEST);
    }

    result = pyronet_ncp_state_accept_neighbor_table(&update);
    if (result == PYRONET_NCP_NEIGHBOR_TABLE_REJECTED_TARGET)
    {
        return pyronetNcpRxSendResponse(service_id, request_ptr, COAP_MSG_CODE_RESPONSE_BAD_REQUEST);
    }

    return pyronetNcpRxSendResponse(service_id, request_ptr, COAP_MSG_CODE_RESPONSE_CHANGED);
}

static int pyronetNcpRxHandleTimeSyncUpdate(int8_t service_id, sn_coap_hdr_s *request_ptr)
{
    pyronet_mesh_time_sync_v1_t packet;

    if (!pyronet_mesh_decode_time_sync(request_ptr->payload_ptr, request_ptr->payload_len, &packet))
    {
        return pyronetNcpRxSendResponse(service_id, request_ptr, COAP_MSG_CODE_RESPONSE_BAD_REQUEST);
    }

    pyronet_ncp_events_queue_time_sync_update(packet.unix_time_s);
    (void)swoDebugPrintf("PYRONET_TIME_SYNC_RX unix_time_s=%lu",
                         (unsigned long)packet.unix_time_s);

    return pyronetNcpRxSendResponse(service_id, request_ptr, COAP_MSG_CODE_RESPONSE_CHANGED);
}

static int pyronetNcpRxHandleConfigUpdate(int8_t service_id, sn_coap_hdr_s *request_ptr)
{
    pyronet_mesh_config_update_v1_t packet;
    pyronet_host_config_update_received_v1_t event;

    if (!pyronet_mesh_decode_config_update(request_ptr->payload_ptr, request_ptr->payload_len, &packet))
    {
        return pyronetNcpRxSendResponse(service_id, request_ptr, COAP_MSG_CODE_RESPONSE_BAD_REQUEST);
    }

    event.config_id = packet.config_id;
    event.l2_temp_thresh = packet.l2_temp_thresh;
    event.l2_humidity_thresh = packet.l2_humidity_thresh;
    event.l2_bvoc_ppm_thresh = packet.l2_bvoc_ppm_thresh;
    event.l3_temp_thresh = packet.l3_temp_thresh;
    event.l3_humidity_thresh = packet.l3_humidity_thresh;
    event.l3_bvoc_ppm_thresh = packet.l3_bvoc_ppm_thresh;
    event.l4_bvoc_ppm_thresh = packet.l4_bvoc_ppm_thresh;
    event.l5_bvoc_ppm_thresh = packet.l5_bvoc_ppm_thresh;
    event.l5_pm25_thresh = packet.l5_pm25_thresh;

    pyronet_ncp_events_queue_config_update_rx(&event);
    (void)swoDebugPrintf("PYRONET_CONFIG_UPDATE_RX config_id=%lu",
                         (unsigned long)event.config_id);

    return pyronetNcpRxSendResponse(service_id, request_ptr, COAP_MSG_CODE_RESPONSE_CHANGED);
}

static int pyronetNcpRxHandleInboundNeighborAlert(int8_t service_id, sn_coap_hdr_s *request_ptr)
{
    pyronet_mesh_neighbor_alert_v1_t packet;

    if (!pyronet_mesh_decode_neighbor_alert(request_ptr->payload_ptr, request_ptr->payload_len, &packet))
    {
        return pyronetNcpRxSendResponse(service_id, request_ptr, COAP_MSG_CODE_RESPONSE_BAD_REQUEST);
    }

    pyronet_ncp_events_queue_neighbor_alert_rx(packet.node_id, packet.risk_level, packet.timestamp);
    (void)swoDebugPrintf("PYRONET_NEIGHBOR_ALERT_RX node=%u risk=%u ts=%lu",
                         (unsigned int)packet.node_id,
                         (unsigned int)packet.risk_level,
                         (unsigned long)packet.timestamp);

    return pyronetNcpRxSendResponse(service_id, request_ptr, COAP_MSG_CODE_RESPONSE_CHANGED);
}

int pyronet_ncp_downlink_receive(int8_t service_id,
                                 uint8_t source_address[static 16],
                                 uint16_t source_port,
                                 sn_coap_hdr_s *request_ptr)
{
    (void)source_address;
    (void)source_port;

    if (request_ptr == NULL)
    {
        return -1;
    }

    if (request_ptr->msg_code != COAP_MSG_CODE_REQUEST_POST)
    {
        return pyronetNcpRxSendResponse(service_id, request_ptr, COAP_MSG_CODE_RESPONSE_METHOD_NOT_ALLOWED);
    }

    if ((request_ptr->payload_ptr == NULL) || (request_ptr->payload_len == 0U))
    {
        return pyronetNcpRxSendResponse(service_id, request_ptr, COAP_MSG_CODE_RESPONSE_BAD_REQUEST);
    }

    switch (request_ptr->payload_ptr[0])
    {
        case PYRONET_PKT_NN_TABLE_UPDATE:
            return pyronetNcpRxHandleNeighborTableUpdate(service_id, request_ptr);

        case PYRONET_PKT_TIME_SYNC:
            return pyronetNcpRxHandleTimeSyncUpdate(service_id, request_ptr);

        case PYRONET_PKT_CONFIG_UPDATE:
            return pyronetNcpRxHandleConfigUpdate(service_id, request_ptr);

        case PYRONET_PKT_NEIGHBOR_ALERT:
            return pyronetNcpRxHandleInboundNeighborAlert(service_id, request_ptr);

        default:
            return pyronetNcpRxSendResponse(service_id, request_ptr, COAP_MSG_CODE_RESPONSE_BAD_REQUEST);
    }
}
