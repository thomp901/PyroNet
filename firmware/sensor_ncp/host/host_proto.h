#ifndef HOST_HOST_PROTO_H
#define HOST_HOST_PROTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HOST_PROTO_SOF0                  0xA5U
#define HOST_PROTO_SOF1                  0x5AU
#define HOST_PROTO_VERSION               0x01U
#define HOST_PROTO_MAX_PAYLOAD_LEN       64U
#define HOST_PROTO_HEADER_LEN            8U
#define HOST_PROTO_CRC_LEN               2U
#define HOST_PROTO_FRAME_MIN_LEN         (HOST_PROTO_HEADER_LEN + HOST_PROTO_CRC_LEN)
#define HOST_PROTO_FRAME_MAX_LEN         (HOST_PROTO_FRAME_MIN_LEN + HOST_PROTO_MAX_PAYLOAD_LEN)
#define HOST_PROTO_PARSER_BUFFER_LEN     160U

#define HOST_MSG_FLAG_RESPONSE           (1U << 0)
#define HOST_MSG_FLAG_ERROR              (1U << 1)
#define HOST_MSG_FLAG_EVENT              (1U << 2)

#define HOST_CAP_STATUS_SUPPORTED        (1U << 0)

#define HOST_ENDPOINT_MCU                0x01U
#define HOST_ENDPOINT_NCP                0x02U

enum pyronet_host_msg_type
{
    HOST_MSG_HELLO      = 0x01,
    HOST_MSG_HELLO_ACK  = 0x02,
    HOST_MSG_PING       = 0x03,
    HOST_MSG_PONG       = 0x04,
    HOST_MSG_GET_STATUS = 0x05,
    HOST_MSG_STATUS     = 0x06,
    HOST_MSG_ERROR      = 0x7F
};

enum host_error_code
{
    HOST_ERR_UNKNOWN_TYPE    = 1,
    HOST_ERR_BAD_LENGTH      = 2,
    HOST_ERR_BAD_STATE       = 3,
    HOST_ERR_NOT_IMPLEMENTED = 4
};

enum host_link_state_value
{
    HOST_LINK_STATE_VALUE_INIT  = 0,
    HOST_LINK_STATE_VALUE_READY = 1
};

enum host_network_state_value
{
    HOST_NETWORK_STATE_UNKNOWN = 0,
    HOST_NETWORK_STATE_DOWN    = 1,
    HOST_NETWORK_STATE_JOINING = 2,
    HOST_NETWORK_STATE_JOINED  = 3
};

struct host_hello_v1
{
    uint8_t endpoint;
    uint8_t proto_min;
    uint8_t proto_max;
    uint8_t capabilities;
    uint32_t fw_version;
} __attribute__((packed));

struct host_hello_ack_v1
{
    uint8_t endpoint;
    uint8_t proto_selected;
    uint8_t capabilities;
    uint8_t reserved;
    uint32_t fw_version;
} __attribute__((packed));

struct host_ping_v1
{
    uint32_t token;
} __attribute__((packed));

struct host_pong_v1
{
    uint32_t token;
} __attribute__((packed));

struct host_status_v1
{
    uint8_t link_state;
    uint8_t network_state;
    uint8_t reserved0;
    uint8_t reserved1;
    uint32_t uptime_s;
} __attribute__((packed));

struct host_error_v1
{
    uint8_t failed_type;
    uint8_t error_code;
    uint8_t reserved0;
    uint8_t reserved1;
} __attribute__((packed));

struct host_frame
{
    uint8_t type;
    uint8_t flags;
    uint8_t seq;
    uint16_t payload_len;
    uint8_t payload[HOST_PROTO_MAX_PAYLOAD_LEN];
};

struct host_proto_parser
{
    uint8_t buffer[HOST_PROTO_PARSER_BUFFER_LEN];
    size_t length;
};

uint16_t host_proto_crc16_ccitt_false(const uint8_t *data, size_t length);
bool host_proto_encode_frame(uint8_t type,
                             uint8_t flags,
                             uint8_t seq,
                             const void *payload,
                             uint16_t payload_len,
                             uint8_t *out_frame,
                             size_t out_frame_size,
                             size_t *out_frame_len);
void host_proto_parser_init(struct host_proto_parser *parser);
bool host_proto_parser_push(struct host_proto_parser *parser, uint8_t byte, struct host_frame *out_frame);
const char *host_proto_type_name(uint8_t type);

#endif /* HOST_HOST_PROTO_H */
