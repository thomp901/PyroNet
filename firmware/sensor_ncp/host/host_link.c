#include "host_link.h"

#include <string.h>

#include <ti/drivers/dpl/ClockP.h>

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
    struct host_proto_parser parser;
    uint8_t next_tx_seq;
    uint32_t tick_period_us;
    uint64_t boot_ticks;
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
    struct host_error_v1 error_payload;

    error_payload.failed_type = request_type;
    error_payload.error_code = error_code;
    error_payload.reserved0 = 0U;
    error_payload.reserved1 = 0U;

    (void)hostLinkSendFrame(HOST_MSG_ERROR,
                            HOST_MSG_FLAG_RESPONSE | HOST_MSG_FLAG_ERROR,
                            request_seq,
                            &error_payload,
                            sizeof(error_payload));
}

static void hostLinkSendHelloAck(uint8_t request_seq)
{
    struct host_hello_ack_v1 ack_payload;

    ack_payload.endpoint = HOST_ENDPOINT_NCP;
    ack_payload.proto_selected = HOST_LINK_PROTOCOL_SELECTED;
    ack_payload.capabilities = HOST_CAP_STATUS_SUPPORTED;
    ack_payload.reserved = 0U;
    ack_payload.fw_version = HOST_LINK_NCP_FW_VERSION;

    (void)hostLinkSendFrame(HOST_MSG_HELLO_ACK,
                            HOST_MSG_FLAG_RESPONSE,
                            request_seq,
                            &ack_payload,
                            sizeof(ack_payload));
}

static void hostLinkSendPong(uint8_t request_seq, const struct host_ping_v1 *ping)
{
    struct host_pong_v1 pong_payload;

    pong_payload.token = ping->token;

    (void)hostLinkSendFrame(HOST_MSG_PONG,
                            HOST_MSG_FLAG_RESPONSE,
                            request_seq,
                            &pong_payload,
                            sizeof(pong_payload));
}

static void hostLinkSendStatus(uint8_t request_seq)
{
    struct host_status_v1 status_payload;

    status_payload.link_state = HOST_LINK_STATE_VALUE_READY;
    status_payload.network_state = HOST_NETWORK_STATE_UNKNOWN;
    status_payload.reserved0 = 0U;
    status_payload.reserved1 = 0U;
    status_payload.uptime_s = hostLinkUptimeSeconds();

    (void)hostLinkSendFrame(HOST_MSG_STATUS,
                            HOST_MSG_FLAG_RESPONSE,
                            request_seq,
                            &status_payload,
                            sizeof(status_payload));
}

static void hostLinkHandleHello(const struct host_frame *frame)
{
    struct host_hello_v1 hello_payload;
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
    hostLinkSendHelloAck(frame->seq);

    if (!was_ready)
    {
        (void)swoDebugWriteLine("HOST_LINK_READY");
    }
}

static void hostLinkHandlePing(const struct host_frame *frame)
{
    struct host_ping_v1 ping_payload;

    if (frame->payload_len != sizeof(ping_payload))
    {
        hostLinkSendError(frame->type, frame->seq, HOST_ERR_BAD_LENGTH);
        return;
    }

    memcpy(&ping_payload, frame->payload, sizeof(ping_payload));
    hostLinkSendPong(frame->seq, &ping_payload);
}

static void hostLinkHandleGetStatus(const struct host_frame *frame)
{
    if (frame->payload_len != 0U)
    {
        hostLinkSendError(frame->type, frame->seq, HOST_ERR_BAD_LENGTH);
        return;
    }

    hostLinkSendStatus(frame->seq);
}

static void hostLinkHandleFrame(const struct host_frame *frame)
{
    hostLinkLogRx(frame);

    if ((hostLinkContext.state != HOST_LINK_STATE_READY) && (frame->type != HOST_MSG_HELLO))
    {
        return;
    }

    switch (frame->type)
    {
        case HOST_MSG_HELLO:
            hostLinkHandleHello(frame);
            break;

        case HOST_MSG_PING:
            hostLinkHandlePing(frame);
            break;

        case HOST_MSG_GET_STATUS:
            hostLinkHandleGetStatus(frame);
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
    struct host_frame frame;
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

    ClockP_usleep(HOST_LINK_POLL_SLEEP_US);
}

bool host_link_is_ready(void)
{
    return (hostLinkContext.state == HOST_LINK_STATE_READY);
}
