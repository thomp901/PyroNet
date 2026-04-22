#ifndef TRANSPORT_HOST_PROTO_H
#define TRANSPORT_HOST_PROTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HOST_FRAME_SOF0                 0xA5U
#define HOST_FRAME_SOF1                 0x5AU
#define HOST_PROTO_VERSION              0x01U
#define HOST_PROTO_MAX_PAYLOAD_LEN      64U
#define HOST_PROTO_MAX_FRAME_LEN        (10U + HOST_PROTO_MAX_PAYLOAD_LEN)

#define HOST_FRAME_FLAG_RESPONSE        0x01U
#define HOST_FRAME_FLAG_ERROR           0x02U
#define HOST_FRAME_FLAG_EVENT           0x04U

#define HOST_ENDPOINT_MCU               0x01U
#define HOST_ENDPOINT_NCP               0x02U
#define HOST_CAP_STATUS_SUPPORTED       0x01U

#if defined(__GNUC__)
#define HOST_PROTO_PACKED __attribute__((packed))
#else
#define HOST_PROTO_PACKED
#endif

enum pyronet_host_msg_type {
  HOST_MSG_HELLO = 0x01,
  HOST_MSG_HELLO_ACK = 0x02,
  HOST_MSG_PING = 0x03,
  HOST_MSG_PONG = 0x04,
  HOST_MSG_GET_STATUS = 0x05,
  HOST_MSG_STATUS = 0x06,
  HOST_MSG_ERROR = 0x7F,
};

enum host_error_code {
  HOST_ERR_UNKNOWN_TYPE = 1,
  HOST_ERR_BAD_LENGTH = 2,
  HOST_ERR_BAD_STATE = 3,
  HOST_ERR_NOT_IMPLEMENTED = 4,
};

typedef struct HOST_PROTO_PACKED {
  uint8_t endpoint;
  uint8_t proto_min;
  uint8_t proto_max;
  uint8_t capabilities;
  uint32_t fw_version;
} host_hello_v1_t;

typedef struct HOST_PROTO_PACKED {
  uint8_t endpoint;
  uint8_t proto_selected;
  uint8_t capabilities;
  uint8_t reserved;
  uint32_t fw_version;
} host_hello_ack_v1_t;

typedef struct HOST_PROTO_PACKED {
  uint32_t token;
} host_ping_v1_t;

typedef struct HOST_PROTO_PACKED {
  uint32_t token;
} host_pong_v1_t;

typedef struct HOST_PROTO_PACKED {
  uint8_t link_state;
  uint8_t network_state;
  uint8_t reserved0;
  uint8_t reserved1;
  uint32_t uptime_s;
} host_status_v1_t;

typedef struct HOST_PROTO_PACKED {
  uint8_t failed_type;
  uint8_t error_code;
  uint8_t reserved0;
  uint8_t reserved1;
} host_error_v1_t;

typedef struct {
  uint8_t type;
  uint8_t flags;
  uint8_t seq;
  uint16_t payload_length;
  uint8_t payload[HOST_PROTO_MAX_PAYLOAD_LEN];
} host_frame_t;

typedef struct {
  uint8_t state;
  uint8_t type;
  uint8_t flags;
  uint8_t seq;
  uint16_t payload_length;
  uint16_t payload_index;
  uint16_t crc;
  uint16_t received_crc;
  uint8_t payload[HOST_PROTO_MAX_PAYLOAD_LEN];
} host_proto_parser_t;

uint16_t host_crc16_ccitt_false(const uint8_t *data, size_t length);
size_t host_proto_encode_frame(uint8_t type,
                               uint8_t flags,
                               uint8_t seq,
                               const void *payload,
                               uint16_t payload_length,
                               uint8_t *out_frame,
                               size_t out_frame_size);
void host_proto_parser_init(host_proto_parser_t *parser);
bool host_proto_parser_consume(host_proto_parser_t *parser,
                               uint8_t byte,
                               host_frame_t *out_frame);
const char *host_proto_type_name(uint8_t type);

#endif
