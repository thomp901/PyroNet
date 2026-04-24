#include "host_link.h"

#include <string.h>

#include <ti/drivers/dpl/ClockP.h>

#include "../pyronet_ncp.h"
#include "host_uart.h"
#include "../swo_debug.h"

#define HOST_LINK_PROTOCOL_SELECTED   0x01U
#define HOST_LINK_NCP_FW_VERSION      0x00010000UL
#define HOST_LINK_POLL_SLEEP_US       1000U

enum host_link_state
{
    HOST_LINK_STATE_RESET = 0,
    HOST_LINK_STATE_UART_READY,
    HOST_LINK_STATE_WAIT_HELLO,
    HOST_LINK_STATE_READY
};

struct host_link_context
{
    bool initialized;
    enum host_link_state state;
    host_proto_parser_t parser;
    uint8_t next_tx_seq;
    uint32_t tick_period_us;
    uint64_t boot_ticks;
    bool session_sync_pending;
};

static struct host_link_context hostLinkContext;

static uint32_t hostLinkUptimeSeconds(void)
{
    uint64_t elapsed_ticks;
    uint64_t elapsed_us;

    elapsed_ticks = ClockP_getSystemTicks64() - hostLinkContext.boot_ticks;
    elapsed_us = elapsed_ticks * hostLinkContext.tick_period_us;

    return (uint32_t)(elapsed_us / 1000000ULL);
}

static void hostLinkLogRx(const struct host_frame *frame)
{
    (void)swoDebugPrintf("HOST_RX type=%s seq=%u len=%u",
                         host_proto_type_name(frame->type),
                         (unsigned int)frame->seq,
                         (unsigned int)frame->payload_len);
}

static void hostLinkLogTx(uint8_t type, uint8_t seq, uint16_t payload_len)
{
    (void)swoDebugPrintf("HOST_TX type=%s seq=%u len=%u",
                         host_proto_type_name(type),
                         (unsigned int)seq,
                         (unsigned int)payload_len);
}

static bool hostLinkSendFrame(uint8_t type,
                              uint8_t flags,
                              uint8_t seq,
                              const void *payload,
                              uint16_t payload_len)
{
    uint8_t frame[HOST_PROTO_FRAME_MAX_LEN];
    size_t frame_len = 0U;

    if (!host_proto_encode_frame(type, flags, seq, payload, payload_len, frame, sizeof(frame), &frame_len))
    {
        return false;
    }

    if (!host_uart_write(frame, frame_len))
    {
        return false;
    }

    hostLinkLogTx(type, seq, payload_len);
    hostLinkContext.next_tx_seq++;
    return true;
}

static void hostLinkSendError(uint8_t request_type, uint8_t request_seq, uint8_t error_code)
{
    host_error_v1_t error_payload;

    error_payload.failed_type = request_type;
    error_payload.error_code = error_code;
    error_payload.reserved0 = 0U;
    error_payload.reserved1 = 0U;

    (void)hostLinkSendFrame(PYRONET_HOST_MSG_ERROR,
                            HOST_MSG_FLAG_RESPONSE | HOST_MSG_FLAG_ERROR,
                            request_seq,
                            &error_payload,
                            sizeof(error_payload));
}

static void hostLinkSendHelloAck(uint8_t request_seq)
{
    host_hello_ack_v1_t ack_payload;

    ack_payload.endpoint = HOST_ENDPOINT_NCP;
    ack_payload.proto_selected = HOST_LINK_PROTOCOL_SELECTED;
    ack_payload.capabilities = HOST_CAP_STATUS_SUPPORTED;
    ack_payload.reserved = 0U;
    ack_payload.fw_version = HOST_LINK_NCP_FW_VERSION;

    (void)hostLinkSendFrame(PYRONET_HOST_MSG_HELLO_ACK,
                            HOST_MSG_FLAG_RESPONSE,
                            request_seq,
                            &ack_payload,
                            sizeof(ack_payload));
}

static void hostLinkSendPong(uint8_t request_seq, const host_ping_v1_t *ping)
{
    host_pong_v1_t pong_payload;

    pong_payload.token = ping->token;

    (void)hostLinkSendFrame(PYRONET_HOST_MSG_PONG,
                            HOST_MSG_FLAG_RESPONSE,
                            request_seq,
                            &pong_payload,
                            sizeof(pong_payload));
}

static void hostLinkSendStatus(uint8_t request_seq)
{
    host_status_v1_t status_payload;

    status_payload.link_state = HOST_LINK_STATE_VALUE_READY;
    status_payload.network_state = pyronet_ncp_network_state();
    status_payload.reserved0 = 0U;
    status_payload.reserved1 = 0U;
    status_payload.uptime_s = hostLinkUptimeSeconds();

    (void)hostLinkSendFrame(PYRONET_HOST_MSG_STATUS,
                            HOST_MSG_FLAG_RESPONSE,
                            request_seq,
                            &status_payload,
                            sizeof(status_payload));
}

static void hostLinkSendTxResultEvent(uint8_t request_type, uint8_t status, uint8_t detail)
{
    pyronet_host_tx_result_v1_t result_payload;

    result_payload.request_type = request_type;
    result_payload.status = status;
    result_payload.detail = detail;
    result_payload.reserved = 0U;

    (void)hostLinkSendFrame(PYRONET_HOST_MSG_TX_RESULT,
                            HOST_MSG_FLAG_EVENT,
                            hostLinkContext.next_tx_seq,
                            &result_payload,
                            sizeof(result_payload));
}

static void hostLinkDrainNcpEvents(void)
{
    pyronet_ncp_event_t event;

    if (hostLinkContext.session_sync_pending)
    {
        return;
    }

    while ((hostLinkContext.state == HOST_LINK_STATE_READY) && pyronet_ncp_next_event(&event))
    {
        switch (event.msg_type)
        {
            case PYRONET_HOST_MSG_REGISTRATION_NEEDED:
                (void)hostLinkSendFrame(event.msg_type,
                                        HOST_MSG_FLAG_EVENT,
                                        hostLinkContext.next_tx_seq,
                                        &event.payload.registration_needed,
                                        sizeof(event.payload.registration_needed));
                break;

            case PYRONET_HOST_MSG_PARENT_CHANGED:
                (void)hostLinkSendFrame(event.msg_type,
                                        HOST_MSG_FLAG_EVENT,
                                        hostLinkContext.next_tx_seq,
                                        &event.payload.parent_changed,
                                        sizeof(event.payload.parent_changed));
                break;

            case PYRONET_HOST_MSG_TX_RESULT:
                (void)hostLinkSendFrame(event.msg_type,
                                        HOST_MSG_FLAG_EVENT,
                                        hostLinkContext.next_tx_seq,
                                        &event.payload.tx_result,
                                        sizeof(event.payload.tx_result));
                break;

            case PYRONET_HOST_MSG_TIME_SYNC_UPDATE:
                (void)hostLinkSendFrame(event.msg_type,
                                        HOST_MSG_FLAG_EVENT,
                                        hostLinkContext.next_tx_seq,
                                        &event.payload.time_sync_update,
                                        sizeof(event.payload.time_sync_update));
                break;

            case PYRONET_HOST_MSG_NEIGHBOR_ALERT_RX:
                (void)hostLinkSendFrame(event.msg_type,
                                        HOST_MSG_FLAG_EVENT,
                                        hostLinkContext.next_tx_seq,
                                        &event.payload.neighbor_alert_rx,
                                        sizeof(event.payload.neighbor_alert_rx));
                break;

            case PYRONET_HOST_MSG_CONFIG_UPDATE_RX:
                (void)hostLinkSendFrame(event.msg_type,
                                        HOST_MSG_FLAG_EVENT,
                                        hostLinkContext.next_tx_seq,
                                        &event.payload.config_update_rx,
                                        sizeof(event.payload.config_update_rx));
                break;

            default:
                break;
        }
    }
}

static void hostLinkHandleHello(const struct host_frame *frame)
{
    host_hello_v1_t hello_payload;
    bool was_ready;

    if (frame->payload_len != sizeof(hello_payload))
    {
        hostLinkSendError(frame->type, frame->seq, HOST_ERR_BAD_LENGTH);
        return;
    }

    memcpy(&hello_payload, frame->payload, sizeof(hello_payload));

    if (hello_payload.endpoint != HOST_ENDPOINT_MCU)
    {
        hostLinkSendError(frame->type, frame->seq, HOST_ERR_BAD_STATE);
        return;
    }

    if ((hello_payload.proto_min > HOST_LINK_PROTOCOL_SELECTED) || (hello_payload.proto_max < HOST_LINK_PROTOCOL_SELECTED))
    {
        hostLinkSendError(frame->type, frame->seq, HOST_ERR_NOT_IMPLEMENTED);
        return;
    }

    was_ready = (hostLinkContext.state == HOST_LINK_STATE_READY);

    hostLinkContext.state = HOST_LINK_STATE_READY;
    hostLinkContext.session_sync_pending = true;
    hostLinkSendHelloAck(frame->seq);

    (void)swoDebugPrintf("HOST_LINK_HELLO was_ready=%u sync_pending=%u",
                         was_ready ? 1U : 0U,
                         hostLinkContext.session_sync_pending ? 1U : 0U);

    if (!was_ready)
    {
        (void)swoDebugWriteLine("HOST_LINK_READY");
    }
}

static void hostLinkHandlePing(const struct host_frame *frame)
{
    host_ping_v1_t ping_payload;

    if (frame->payload_len != sizeof(ping_payload))
    {
        hostLinkSendError(frame->type, frame->seq, HOST_ERR_BAD_LENGTH);
        return;
    }

    memcpy(&ping_payload, frame->payload, sizeof(ping_payload));
    hostLinkSendPong(frame->seq, &ping_payload);
}

static void hostLinkHandleSendRegistration(const struct host_frame *frame)
{
    pyronet_host_send_registration_v1_t command_payload;

    if (frame->payload_len != sizeof(command_payload))
    {
        hostLinkSendError(frame->type, frame->seq, HOST_ERR_BAD_LENGTH);
        return;
    }

    memcpy(&command_payload, frame->payload, sizeof(command_payload));
    hostLinkSendTxResultEvent(frame->type, PYRONET_TX_STATUS_ACCEPTED, 0U);
    pyronet_ncp_send_registration(&command_payload);
}

static void hostLinkHandleSendSensorReport(const struct host_frame *frame)
{
    pyronet_host_send_sensor_report_v1_t command_payload;

    if (frame->payload_len != sizeof(command_payload))
    {
        hostLinkSendError(frame->type, frame->seq, HOST_ERR_BAD_LENGTH);
        return;
    }

    memcpy(&command_payload, frame->payload, sizeof(command_payload));
    hostLinkSendTxResultEvent(frame->type, PYRONET_TX_STATUS_ACCEPTED, 0U);
    pyronet_ncp_send_sensor_report(&command_payload);
}

static void hostLinkHandleSendSensorAlert(const struct host_frame *frame)
{
    pyronet_host_send_sensor_alert_v1_t command_payload;

    if (frame->payload_len != sizeof(command_payload))
    {
        hostLinkSendError(frame->type, frame->seq, HOST_ERR_BAD_LENGTH);
        return;
    }

    memcpy(&command_payload, frame->payload, sizeof(command_payload));
    hostLinkSendTxResultEvent(frame->type, PYRONET_TX_STATUS_ACCEPTED, 0U);
    pyronet_ncp_send_sensor_alert(&command_payload);
}

static void hostLinkHandleSendNeighborAlert(const struct host_frame *frame)
{
    pyronet_host_send_neighbor_alert_v1_t command_payload;

    if (frame->payload_len != sizeof(command_payload))
    {
        hostLinkSendError(frame->type, frame->seq, HOST_ERR_BAD_LENGTH);
        return;
    }

    memcpy(&command_payload, frame->payload, sizeof(command_payload));
    hostLinkSendTxResultEvent(frame->type, PYRONET_TX_STATUS_ACCEPTED, 0U);
    pyronet_ncp_send_neighbor_alert(&command_payload);
}

static void hostLinkHandleRequestParentUpdate(const struct host_frame *frame)
{
    pyronet_host_request_parent_update_v1_t command_payload;

    if (frame->payload_len != sizeof(command_payload))
    {
        hostLinkSendError(frame->type, frame->seq, HOST_ERR_BAD_LENGTH);
        return;
    }

    memcpy(&command_payload, frame->payload, sizeof(command_payload));
    hostLinkSendTxResultEvent(frame->type, PYRONET_TX_STATUS_ACCEPTED, 0U);
    pyronet_ncp_send_parent_update(&command_payload);
}

static void hostLinkHandleGetStatus(const struct host_frame *frame)
{
    if (frame->payload_len != 0U)
    {
        hostLinkSendError(frame->type, frame->seq, HOST_ERR_BAD_LENGTH);
        return;
    }

    hostLinkSendStatus(frame->seq);

    if (hostLinkContext.session_sync_pending)
    {
        (void)swoDebugWriteLine("HOST_LINK_SYNC_AFTER_STATUS");
        hostLinkContext.session_sync_pending = false;
        pyronet_ncp_host_session_ready();
    }
}

static void hostLinkHandleFrame(const struct host_frame *frame)
{
    hostLinkLogRx(frame);

    if ((hostLinkContext.state != HOST_LINK_STATE_READY) && (frame->type != PYRONET_HOST_MSG_HELLO))
    {
        return;
    }

    switch (frame->type)
    {
        case PYRONET_HOST_MSG_HELLO:
            hostLinkHandleHello(frame);
            break;

        case PYRONET_HOST_MSG_PING:
            hostLinkHandlePing(frame);
            break;

        case PYRONET_HOST_MSG_GET_STATUS:
            hostLinkHandleGetStatus(frame);
            break;

        case PYRONET_HOST_MSG_SEND_REGISTRATION:
            hostLinkHandleSendRegistration(frame);
            break;

        case PYRONET_HOST_MSG_SEND_SENSOR_REPORT:
            hostLinkHandleSendSensorReport(frame);
            break;

        case PYRONET_HOST_MSG_SEND_SENSOR_ALERT:
            hostLinkHandleSendSensorAlert(frame);
            break;

        case PYRONET_HOST_MSG_SEND_NEIGHBOR_ALERT:
            hostLinkHandleSendNeighborAlert(frame);
            break;

        case PYRONET_HOST_MSG_REQUEST_PARENT_UPDATE:
            hostLinkHandleRequestParentUpdate(frame);
            break;

        default:
            hostLinkSendError(frame->type, frame->seq, HOST_ERR_UNKNOWN_TYPE);
            break;
    }
}

void host_link_init(void)
{
    memset(&hostLinkContext, 0, sizeof(hostLinkContext));
    host_proto_parser_init(&hostLinkContext.parser);

    if (!host_uart_init())
    {
        return;
    }

    hostLinkContext.tick_period_us = ClockP_getSystemTickPeriod();
    hostLinkContext.boot_ticks = ClockP_getSystemTicks64();
    hostLinkContext.state = HOST_LINK_STATE_UART_READY;
    hostLinkContext.initialized = true;

    (void)swoDebugPrintf("HOST_UART_READY baud=%u", HOST_UART_BAUD_RATE);

    hostLinkContext.state = HOST_LINK_STATE_WAIT_HELLO;
}

void host_link_poll(void)
{
    host_frame_t frame;
    uint8_t rx_byte;

    if (!hostLinkContext.initialized)
    {
        ClockP_usleep(HOST_LINK_POLL_SLEEP_US);
        return;
    }

    while (host_uart_read_byte(&rx_byte))
    {
        if (host_proto_parser_push(&hostLinkContext.parser, rx_byte, &frame))
        {
            hostLinkHandleFrame(&frame);
        }
    }

    hostLinkDrainNcpEvents();
    ClockP_usleep(HOST_LINK_POLL_SLEEP_US);
}

bool host_link_is_ready(void)
{
    return (hostLinkContext.state == HOST_LINK_STATE_READY);
}
