#include "../pyronet_ncp.h"

#include <pthread.h>
#include <string.h>

#include "../swo_debug.h"

#include "coap_service_api.h"
#include "ip6string.h"
#include "mesh_system.h"
#include "mbed-coap/sn_coap_header.h"

#include "pyronet_ncp_events.h"
#include "pyronet_ncp_local.h"
#include "pyronet_ncp_pending_tx.h"
#include "pyronet_ncp_state.h"

typedef struct pyronet_ncp_deferred_registration
{
    bool valid;
    pyronet_host_send_registration_v1_t command;
} pyronet_ncp_deferred_registration_t;

static pyronet_ncp_deferred_registration_t pyronetNcpDeferredRegistration;
static pthread_mutex_t pyronetNcpDeferredRegistrationLock;
static bool pyronetNcpDeferredRegistrationLockInitialized;
static uint32_t pyronetNcpDeferredRegistrationWaitPolls;

#define PYRONET_REGISTRATION_WAIT_LOG_INTERVAL 5000U

static uint8_t pyronetNcpRouterAddressDetail(pyronet_ncp_router_address_result_t result)
{
    switch (result)
    {
        case PYRONET_NCP_ROUTER_ADDRESS_NOT_JOINED:
            return PYRONET_TX_DETAIL_NOT_JOINED;
        case PYRONET_NCP_ROUTER_ADDRESS_UNAVAILABLE:
            return PYRONET_TX_DETAIL_ROUTER_UNAVAILABLE;
        case PYRONET_NCP_ROUTER_ADDRESS_INVALID:
            return PYRONET_TX_DETAIL_ROUTER_INVALID;
        case PYRONET_NCP_ROUTER_ADDRESS_READY:
        default:
            return PYRONET_TX_DETAIL_NONE;
    }
}

static const char *pyronetNcpRouterAddressReasonName(pyronet_ncp_router_address_result_t result)
{
    switch (result)
    {
        case PYRONET_NCP_ROUTER_ADDRESS_NOT_JOINED:
            return "not_joined";
        case PYRONET_NCP_ROUTER_ADDRESS_UNAVAILABLE:
            return "router_unavailable";
        case PYRONET_NCP_ROUTER_ADDRESS_INVALID:
            return "router_invalid";
        case PYRONET_NCP_ROUTER_ADDRESS_READY:
        default:
            return "ready";
    }
}

static bool pyronetNcpDeferredRegistrationEnsureInit(void)
{
    if (pyronetNcpDeferredRegistrationLockInitialized)
    {
        return true;
    }

    if (pthread_mutex_init(&pyronetNcpDeferredRegistrationLock, NULL) == 0)
    {
        pyronetNcpDeferredRegistrationLockInitialized = true;
        return true;
    }

    return false;
}

static void pyronetNcpDeferredRegistrationStore(const pyronet_host_send_registration_v1_t *command)
{
    if (command == NULL)
    {
        return;
    }

    if (!pyronetNcpDeferredRegistrationEnsureInit())
    {
        return;
    }
    (void)pthread_mutex_lock(&pyronetNcpDeferredRegistrationLock);
    pyronetNcpDeferredRegistration.command = *command;
    pyronetNcpDeferredRegistration.valid = true;
    (void)pthread_mutex_unlock(&pyronetNcpDeferredRegistrationLock);
}

static bool pyronetNcpDeferredRegistrationRead(pyronet_host_send_registration_v1_t *command_out)
{
    bool valid;

    if (command_out == NULL)
    {
        return false;
    }

    if (!pyronetNcpDeferredRegistrationEnsureInit())
    {
        return false;
    }
    (void)pthread_mutex_lock(&pyronetNcpDeferredRegistrationLock);
    valid = pyronetNcpDeferredRegistration.valid;
    if (valid)
    {
        *command_out = pyronetNcpDeferredRegistration.command;
    }
    (void)pthread_mutex_unlock(&pyronetNcpDeferredRegistrationLock);

    return valid;
}

static void pyronetNcpDeferredRegistrationClear(void)
{
    if (!pyronetNcpDeferredRegistrationEnsureInit())
    {
        return;
    }
    (void)pthread_mutex_lock(&pyronetNcpDeferredRegistrationLock);
    memset(&pyronetNcpDeferredRegistration, 0, sizeof(pyronetNcpDeferredRegistration));
    (void)pthread_mutex_unlock(&pyronetNcpDeferredRegistrationLock);
}

static int pyronetNcpCoapResponseCallback(int8_t service_id,
                                          uint8_t source_address[static 16],
                                          uint16_t source_port,
                                          sn_coap_hdr_s *response_ptr)
{
    uint8_t request_type = 0U;
    uint8_t detail = 0U;
    uint16_t msg_id = (response_ptr != NULL) ? response_ptr->msg_id : 0U;
    char source_str[PYRONET_ROUTER_ADDR_STR_LEN];

    (void)service_id;

    memset(source_str, 0, sizeof(source_str));
    if (source_address != NULL)
    {
        ip6tos(source_address, source_str);
    }

    if (!pyronet_ncp_pending_tx_match_and_consume(msg_id,
                                                  source_address,
                                                  source_port,
                                                  &request_type,
                                                  &detail))
    {
        (void)swoDebugPrintf("PYRONET_COAP_ACK_UNMATCHED msg_id=%u src=%s port=%u status=%d code=%u",
                             (unsigned int)msg_id,
                             (source_address != NULL) ? source_str : "-",
                             (unsigned int)source_port,
                             (response_ptr != NULL) ? (int)response_ptr->coap_status : -1,
                             (response_ptr != NULL) ? (unsigned int)response_ptr->msg_code : 0U);
        return 0;
    }

    (void)swoDebugPrintf("PYRONET_COAP_ACK type=%u detail=%u msg_id=%u src=%s port=%u status=%d code=%u",
                         (unsigned int)request_type,
                         (unsigned int)detail,
                         (unsigned int)msg_id,
                         (source_address != NULL) ? source_str : "-",
                         (unsigned int)source_port,
                         (response_ptr != NULL) ? (int)response_ptr->coap_status : -1,
                         (response_ptr != NULL) ? (unsigned int)response_ptr->msg_code : 0U);

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
    char destination_str[PYRONET_ROUTER_ADDR_STR_LEN];

    service_id = pyronet_ncp_state_coap_service_id();
    confirmable = (msg_type == COAP_MSG_TYPE_CONFIRMABLE);
    memset(destination_str, 0, sizeof(destination_str));
    ip6tos(destination, destination_str);

    if (service_id < 0)
    {
        pyronet_ncp_events_queue_tx_result(request_type, PYRONET_TX_STATUS_FAILED, PYRONET_TX_DETAIL_COAP_UNAVAILABLE);
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
                             (unsigned int)PYRONET_TX_DETAIL_TRACK_EXHAUSTED);
        pyronet_ncp_events_queue_tx_result(request_type, PYRONET_TX_STATUS_FAILED, PYRONET_TX_DETAIL_TRACK_EXHAUSTED);
        return false;
    }
    has_reserved_slot = confirmable;

    (void)swoDebugPrintf("PYRONET_COAP_TX type=%u uri=%s confirmable=%u dest=%s payload_len=%u detail=%u service=%d",
                         (unsigned int)request_type,
                         uri,
                         confirmable ? 1U : 0U,
                         destination_str,
                         (unsigned int)payload_len,
                         (unsigned int)detail,
                         (int)service_id);

    nanostack_lock();
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
    nanostack_unlock();

    if (confirmable && (msg_id == 0U))
    {
        if (has_reserved_slot)
        {
            pyronet_ncp_pending_tx_release(reserved_slot);
        }
        (void)swoDebugPrintf("PYRONET_COAP_TX_SUBMIT type=%u status=rejected dest=%s",
                             (unsigned int)request_type,
                             destination_str);
        pyronet_ncp_events_queue_tx_result(request_type, PYRONET_TX_STATUS_FAILED, PYRONET_TX_DETAIL_SEND_REJECTED);
        return false;
    }

    /*
     * Nanostack returns 0 for non-confirmable requests because no response
     * callback is registered and no terminal message ID is needed. The send
     * has already been submitted by coap_service_request_send() at this point.
     */
    if (has_reserved_slot && !pyronet_ncp_pending_tx_commit(reserved_slot, msg_id))
    {
        /*
         * This should not happen after a successful reservation. Treat it as a
         * deterministic send failure rather than emitting SENT without a
         * reconcilable terminal result.
         */
        (void)swoDebugPrintf("PYRONET_TX_TRACK_COMMIT_FAILED type=%u detail=%u",
                             (unsigned int)request_type,
                             (unsigned int)PYRONET_TX_DETAIL_TRACK_COMMIT);
        pyronet_ncp_events_queue_tx_result(request_type, PYRONET_TX_STATUS_FAILED, PYRONET_TX_DETAIL_TRACK_COMMIT);
        return false;
    }

    pyronet_ncp_events_queue_tx_result(request_type, PYRONET_TX_STATUS_SENT, detail);
    (void)swoDebugPrintf("PYRONET_COAP_TX_SUBMIT type=%u status=sent msg_id=%u dest=%s",
                         (unsigned int)request_type,
                         (unsigned int)msg_id,
                         destination_str);
    return true;
}

static bool pyronetNcpTrySendRegistration(const pyronet_host_send_registration_v1_t *command, bool allow_defer)
{
    pyronet_mesh_registration_v1_t mesh_packet;
    uint8_t destination[PYRONET_IPV6_ADDR_LEN];
    pyronet_ncp_router_address_result_t router_result;
    char parent_str[PYRONET_ROUTER_ADDR_STR_LEN];
    bool has_parent_global;

    if (command == NULL)
    {
        pyronet_ncp_events_queue_tx_result(PYRONET_HOST_MSG_SEND_REGISTRATION,
                                           PYRONET_TX_STATUS_FAILED,
                                           PYRONET_TX_DETAIL_BAD_COMMAND);
        return false;
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
    has_parent_global = pyronet_ncp_state_read_current_parent_global(mesh_packet.parent_ipv6);

    router_result = pyronet_ncp_state_read_router_address(destination);
    if (router_result != PYRONET_NCP_ROUTER_ADDRESS_READY)
    {
        if (allow_defer)
        {
            pyronetNcpDeferredRegistrationStore(command);
            (void)swoDebugPrintf("PYRONET_REGISTRATION_DEFER node_id=%u reason=%s",
                                 (unsigned int)command->node_id,
                                 pyronetNcpRouterAddressReasonName(router_result));
            return true;
        }

        pyronet_ncp_events_queue_tx_result(PYRONET_HOST_MSG_SEND_REGISTRATION,
                                           PYRONET_TX_STATUS_FAILED,
                                           pyronetNcpRouterAddressDetail(router_result));
        return false;
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
        memset(parent_str, 0, sizeof(parent_str));
        if (has_parent_global)
        {
            ip6tos(mesh_packet.parent_ipv6, parent_str);
        }
        (void)swoDebugPrintf("PYRONET_REGISTRATION_PARENT node_id=%u parent=%s",
                             (unsigned int)command->node_id,
                             has_parent_global ? parent_str : "-");
        (void)swoDebugPrintf("PYRONET_REGISTRATION_SUBMIT node_id=%u",
                             (unsigned int)command->node_id);
        pyronetNcpDeferredRegistrationClear();
        pyronet_ncp_state_clear_registration_required();
        return true;
    }

    (void)swoDebugPrintf("PYRONET_REGISTRATION_SUBMIT_FAILED node_id=%u",
                         (unsigned int)command->node_id);
    return false;
}

void pyronet_ncp_send_registration(const pyronet_host_send_registration_v1_t *command)
{
    (void)pyronetNcpTrySendRegistration(command, true);
}

void pyronet_ncp_tx_poll(void)
{
    pyronet_host_send_registration_v1_t command;
    uint8_t destination[PYRONET_IPV6_ADDR_LEN];
    pyronet_ncp_router_address_result_t router_result;

    if (!pyronetNcpDeferredRegistrationRead(&command))
    {
        pyronetNcpDeferredRegistrationWaitPolls = 0U;
        return;
    }

    router_result = pyronet_ncp_state_read_router_address(destination);
    if (router_result != PYRONET_NCP_ROUTER_ADDRESS_READY)
    {
        pyronetNcpDeferredRegistrationWaitPolls++;
        if ((pyronetNcpDeferredRegistrationWaitPolls == 1U) ||
            (pyronetNcpDeferredRegistrationWaitPolls >= PYRONET_REGISTRATION_WAIT_LOG_INTERVAL))
        {
            (void)swoDebugPrintf("PYRONET_REGISTRATION_WAIT node_id=%u reason=%s host_state=%u",
                                 (unsigned int)command.node_id,
                                 pyronetNcpRouterAddressReasonName(router_result),
                                 (unsigned int)pyronet_ncp_state_network_state());
            pyronetNcpDeferredRegistrationWaitPolls = 0U;
        }
        return;
    }

    pyronetNcpDeferredRegistrationWaitPolls = 0U;
    (void)swoDebugPrintf("PYRONET_REGISTRATION_ROUTE_READY node_id=%u",
                         (unsigned int)command.node_id);

    if (pyronetNcpTrySendRegistration(&command, true))
    {
        (void)swoDebugPrintf("PYRONET_REGISTRATION_FLUSH node_id=%u",
                             (unsigned int)command.node_id);
    }
}

void pyronet_ncp_send_sensor_report(const pyronet_host_send_sensor_report_v1_t *command)
{
    pyronet_mesh_sensor_report_v1_t mesh_packet;
    uint8_t destination[PYRONET_IPV6_ADDR_LEN];
    pyronet_ncp_router_address_result_t router_result;

    if (command == NULL)
    {
        pyronet_ncp_events_queue_tx_result(PYRONET_HOST_MSG_SEND_SENSOR_REPORT,
                                           PYRONET_TX_STATUS_FAILED,
                                           PYRONET_TX_DETAIL_BAD_COMMAND);
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

    router_result = pyronet_ncp_state_read_router_address(destination);
    if (router_result != PYRONET_NCP_ROUTER_ADDRESS_READY)
    {
        pyronet_ncp_events_queue_tx_result(PYRONET_HOST_MSG_SEND_SENSOR_REPORT,
                                           PYRONET_TX_STATUS_FAILED,
                                           pyronetNcpRouterAddressDetail(router_result));
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
    pyronet_ncp_router_address_result_t router_result;

    if (command == NULL)
    {
        pyronet_ncp_events_queue_tx_result(PYRONET_HOST_MSG_SEND_SENSOR_ALERT,
                                           PYRONET_TX_STATUS_FAILED,
                                           PYRONET_TX_DETAIL_BAD_COMMAND);
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

    router_result = pyronet_ncp_state_read_router_address(destination);
    if (router_result != PYRONET_NCP_ROUTER_ADDRESS_READY)
    {
        pyronet_ncp_events_queue_tx_result(PYRONET_HOST_MSG_SEND_SENSOR_ALERT,
                                           PYRONET_TX_STATUS_FAILED,
                                           pyronetNcpRouterAddressDetail(router_result));
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
        pyronet_ncp_events_queue_tx_result(PYRONET_HOST_MSG_SEND_NEIGHBOR_ALERT,
                                           PYRONET_TX_STATUS_FAILED,
                                           PYRONET_TX_DETAIL_BAD_COMMAND);
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
        pyronet_ncp_events_queue_tx_result(PYRONET_HOST_MSG_SEND_NEIGHBOR_ALERT,
                                           PYRONET_TX_STATUS_FAILED,
                                           PYRONET_TX_DETAIL_ROUTER_UNAVAILABLE);
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
    pyronet_ncp_router_address_result_t router_result;
    char parent_str[PYRONET_ROUTER_ADDR_STR_LEN];
    bool has_parent_global;

    if (command == NULL)
    {
        pyronet_ncp_events_queue_tx_result(PYRONET_HOST_MSG_REQUEST_PARENT_UPDATE,
                                           PYRONET_TX_STATUS_FAILED,
                                           PYRONET_TX_DETAIL_BAD_COMMAND);
        return;
    }

    pyronet_ncp_state_remember_local_node_id(command->node_id);

    memset(&mesh_packet, 0, sizeof(mesh_packet));
    mesh_packet.type = PYRONET_PKT_PARENT_UPDATE;
    mesh_packet.version = PYRONET_MESH_SCHEMA_VERSION;
    mesh_packet.node_id = command->node_id;
    mesh_packet.timestamp = command->timestamp;
    has_parent_global = pyronet_ncp_state_read_current_parent_global(mesh_packet.parent_ipv6);

    router_result = pyronet_ncp_state_read_router_address(destination);
    if (router_result != PYRONET_NCP_ROUTER_ADDRESS_READY)
    {
        pyronet_ncp_events_queue_tx_result(PYRONET_HOST_MSG_REQUEST_PARENT_UPDATE,
                                           PYRONET_TX_STATUS_FAILED,
                                           pyronetNcpRouterAddressDetail(router_result));
        return;
    }

    (void)pyronetNcpSendCoapRequest(PYRONET_HOST_MSG_REQUEST_PARENT_UPDATE,
                                    destination,
                                    PYRONET_COAP_UPLINK_URI,
                                    COAP_MSG_TYPE_CONFIRMABLE,
                                    (const uint8_t *)&mesh_packet,
                                    sizeof(mesh_packet),
                                    0U);
    memset(parent_str, 0, sizeof(parent_str));
    if (has_parent_global)
    {
        ip6tos(mesh_packet.parent_ipv6, parent_str);
    }
    (void)swoDebugPrintf("PYRONET_PARENT_UPDATE_PARENT node_id=%u parent=%s",
                         (unsigned int)command->node_id,
                         has_parent_global ? parent_str : "-");
}
