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
#define PYRONET_MESH_SCHEMA_VERSION     0x01U

#define HOST_FRAME_FLAG_RESPONSE        0x01U
#define HOST_FRAME_FLAG_ERROR           0x02U
#define HOST_FRAME_FLAG_EVENT           0x04U

#define HOST_ENDPOINT_MCU               0x01U
#define HOST_ENDPOINT_NCP               0x02U
#define HOST_CAP_STATUS_SUPPORTED       0x01U

#if defined(__GNUC__)
#define PYRONET_PACKED __attribute__((packed))
#else
#define PYRONET_PACKED
#endif
#define HOST_PROTO_PACKED PYRONET_PACKED

enum pyronet_mesh_packet_type {
  PYRONET_PKT_REGISTRATION    = 0x01,
  PYRONET_PKT_SENSOR_REPORT   = 0x02,
  PYRONET_PKT_SENSOR_ALERT    = 0x03,
  PYRONET_PKT_NN_TABLE_UPDATE = 0x04,
  PYRONET_PKT_TIME_SYNC       = 0x05,
  PYRONET_PKT_CONFIG_UPDATE   = 0x06,
  PYRONET_PKT_NEIGHBOR_ALERT  = 0x07,
  PYRONET_PKT_PARENT_UPDATE   = 0x08,
};

enum pyronet_host_msg_type {
  PYRONET_HOST_MSG_HELLO                 = 0x01,
  PYRONET_HOST_MSG_HELLO_ACK             = 0x02,
  PYRONET_HOST_MSG_PING                  = 0x03,
  PYRONET_HOST_MSG_PONG                  = 0x04,
  PYRONET_HOST_MSG_GET_STATUS            = 0x05,
  PYRONET_HOST_MSG_STATUS                = 0x06,

  PYRONET_HOST_MSG_SEND_REGISTRATION     = 0x10,
  PYRONET_HOST_MSG_SEND_SENSOR_REPORT    = 0x11,
  PYRONET_HOST_MSG_SEND_SENSOR_ALERT     = 0x12,
  PYRONET_HOST_MSG_SEND_NEIGHBOR_ALERT   = 0x13,
  PYRONET_HOST_MSG_REQUEST_PARENT_UPDATE = 0x14,

  PYRONET_HOST_MSG_REGISTRATION_NEEDED   = 0x50,
  PYRONET_HOST_MSG_PARENT_CHANGED        = 0x51,
  PYRONET_HOST_MSG_TX_RESULT             = 0x52,
  PYRONET_HOST_MSG_TIME_SYNC_UPDATE      = 0x53,
  PYRONET_HOST_MSG_NEIGHBOR_ALERT_RX     = 0x54,
  PYRONET_HOST_MSG_CONFIG_UPDATE_RX      = 0x55,

  PYRONET_HOST_MSG_ERROR                 = 0x7F,
};

enum host_error_code {
  HOST_ERR_UNKNOWN_TYPE = 1,
  HOST_ERR_BAD_LENGTH = 2,
  HOST_ERR_BAD_STATE = 3,
  HOST_ERR_NOT_IMPLEMENTED = 4,
};

enum pyronet_host_registration_reason {
  PYRONET_REG_REASON_JOIN   = 1,
  PYRONET_REG_REASON_REJOIN = 2,
};

enum pyronet_host_parent_change_reason {
  PYRONET_PARENT_CHANGE_PREFERRED_PARENT_CHANGED = 1,
  PYRONET_PARENT_CHANGE_PARENT_LOST              = 2,
};

enum pyronet_host_tx_status {
  PYRONET_TX_STATUS_ACCEPTED = 0,
  PYRONET_TX_STATUS_SENT     = 1,
  PYRONET_TX_STATUS_ACKED    = 2,
  PYRONET_TX_STATUS_TIMEOUT  = 3,
  PYRONET_TX_STATUS_FAILED   = 4,
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

/*
 * Mesh packet structs
 * These match the node-facing packet schema.
 */

typedef struct PYRONET_PACKED {
  uint8_t  type;
  uint8_t  version;
  uint16_t node_id;
  float    latitude;
  float    longitude;
  uint16_t fw_version;
  uint8_t  battery_pct;
  uint8_t  parent_ipv6[16];
} pyronet_mesh_registration_v1_t;

typedef struct PYRONET_PACKED {
  uint8_t  type;
  uint8_t  version;
  uint16_t node_id;
  uint32_t timestamp;
  uint8_t  risk_level;
  int16_t  temperature;
  uint16_t humidity;
  uint16_t bvoc_ppm;
  uint16_t pm25;
  uint8_t  battery_pct;
} pyronet_mesh_sensor_report_v1_t;

typedef pyronet_mesh_sensor_report_v1_t pyronet_mesh_sensor_alert_v1_t;

typedef struct PYRONET_PACKED {
  uint8_t  type;
  uint8_t  version;
  uint16_t node_id;
  uint32_t timestamp;
  uint8_t  parent_ipv6[16];
} pyronet_mesh_parent_update_v1_t;

typedef struct PYRONET_PACKED {
  uint8_t  type;
  uint8_t  version;
  uint16_t node_id;
  uint8_t  risk_level;
  uint32_t timestamp;
} pyronet_mesh_neighbor_alert_v1_t;

/*
 * Semantic MCU->NCP payload structs
 * These intentionally omit mesh-owned fields like type/version/parent_ipv6.
 */

typedef struct PYRONET_PACKED {
  uint16_t node_id;
  float    latitude;
  float    longitude;
  uint16_t fw_version;
  uint8_t  battery_pct;
} pyronet_host_send_registration_v1_t;

typedef struct PYRONET_PACKED {
  uint16_t node_id;
  uint32_t timestamp;
  uint8_t  risk_level;
  int16_t  temperature_c_x100;
  uint16_t humidity_pct_x100;
  uint16_t bvoc_ppm;
  uint16_t pm25_ug_m3_x10;
  uint8_t  battery_pct;
} pyronet_host_send_sensor_report_v1_t;

typedef pyronet_host_send_sensor_report_v1_t pyronet_host_send_sensor_alert_v1_t;

typedef struct PYRONET_PACKED {
  uint16_t node_id;
  uint8_t  risk_level;
  uint32_t timestamp;
} pyronet_host_send_neighbor_alert_v1_t;

typedef struct PYRONET_PACKED {
  uint16_t node_id;
  uint32_t timestamp;
} pyronet_host_request_parent_update_v1_t;

/*
 * Semantic NCP->MCU event payload structs
 */

typedef struct PYRONET_PACKED {
  uint8_t reason;
} pyronet_host_registration_needed_v1_t;

typedef struct PYRONET_PACKED {
  uint8_t change_reason;
} pyronet_host_parent_changed_v1_t;

typedef struct PYRONET_PACKED {
  uint8_t request_type;
  uint8_t status;
  uint8_t detail;
  uint8_t reserved;
} pyronet_host_tx_result_v1_t;

typedef struct PYRONET_PACKED {
  uint32_t unix_time_s;
} pyronet_host_time_sync_update_v1_t;

typedef struct PYRONET_PACKED {
  uint16_t node_id;
  uint8_t  risk_level;
  uint32_t timestamp;
} pyronet_host_neighbor_alert_received_v1_t;

typedef struct PYRONET_PACKED {
  uint32_t config_id;
  int16_t  l2_temp_thresh;
  uint16_t l2_humidity_thresh;
  uint16_t l2_bvoc_ppm_thresh;
  int16_t  l3_temp_thresh;
  uint16_t l3_humidity_thresh;
  uint16_t l3_bvoc_ppm_thresh;
  uint16_t l4_bvoc_ppm_thresh;
  uint16_t l5_bvoc_ppm_thresh;
  uint16_t l5_pm25_thresh;
} pyronet_host_config_update_received_v1_t;

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
const char *host_proto_tx_status_name(uint8_t status);

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(float) == 4, "PyroNet protocol assumes float32");
_Static_assert(sizeof(pyronet_mesh_registration_v1_t) == 31, "bad registration size");
_Static_assert(sizeof(pyronet_mesh_sensor_report_v1_t) == 18, "bad report size");
_Static_assert(sizeof(pyronet_mesh_parent_update_v1_t) == 24, "bad parent update size");
_Static_assert(sizeof(pyronet_mesh_neighbor_alert_v1_t) == 9, "bad neighbor alert size");

_Static_assert(sizeof(pyronet_host_send_registration_v1_t) == 13, "bad host registration size");
_Static_assert(sizeof(pyronet_host_send_sensor_report_v1_t) == 16, "bad host report size");
_Static_assert(sizeof(pyronet_host_send_neighbor_alert_v1_t) == 7, "bad host neighbor alert size");
_Static_assert(sizeof(pyronet_host_request_parent_update_v1_t) == 6, "bad host parent update size");
_Static_assert(sizeof(pyronet_host_registration_needed_v1_t) == 1, "bad registration-needed size");
_Static_assert(sizeof(pyronet_host_parent_changed_v1_t) == 1, "bad parent-changed size");
_Static_assert(sizeof(pyronet_host_tx_result_v1_t) == 4, "bad tx-result size");
_Static_assert(sizeof(pyronet_host_time_sync_update_v1_t) == 4, "bad time-sync-update size");
_Static_assert(sizeof(pyronet_host_neighbor_alert_received_v1_t) == 7, "bad neighbor-alert-rx size");
_Static_assert(sizeof(pyronet_host_config_update_received_v1_t) == 22, "bad config-update-rx size");
#endif

#endif
