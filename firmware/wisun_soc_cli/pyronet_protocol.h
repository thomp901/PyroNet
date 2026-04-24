#ifndef PYRONET_PROTOCOL_H
#define PYRONET_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PYRONET_MESH_SCHEMA_VERSION 0x01U
#define PYRONET_COAP_PORT 5683U
#define PYRONET_COAP_UPLINK_URI "uplink"
#define PYRONET_COAP_DOWNLINK_URI "downlink"
#define PYRONET_COAP_LATERAL_URI PYRONET_COAP_DOWNLINK_URI
#define PYRONET_MAX_NEIGHBORS 32U
#define PYRONET_IPV6_ADDR_LEN 16U
#define PYRONET_COAP_MAX_TOKEN_LEN 8U

#if defined(__GNUC__)
#define PYRONET_PACKED __attribute__((packed))
#else
#define PYRONET_PACKED
#endif

typedef enum {
  PYRONET_PKT_REGISTRATION    = 0x01,
  PYRONET_PKT_SENSOR_REPORT   = 0x02,
  PYRONET_PKT_SENSOR_ALERT    = 0x03,
  PYRONET_PKT_NN_TABLE_UPDATE = 0x04,
  PYRONET_PKT_TIME_SYNC       = 0x05,
  PYRONET_PKT_CONFIG_UPDATE   = 0x06,
  PYRONET_PKT_NEIGHBOR_ALERT  = 0x07,
  PYRONET_PKT_PARENT_UPDATE   = 0x08,
} pyronet_mesh_packet_type_t;

typedef enum {
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
} pyronet_host_msg_type_t;

typedef enum {
  PYRONET_HOST_LINK_STATE_INIT  = 0,
  PYRONET_HOST_LINK_STATE_READY = 1,
} pyronet_host_link_state_t;

typedef enum {
  PYRONET_HOST_NETWORK_STATE_UNKNOWN = 0,
  PYRONET_HOST_NETWORK_STATE_DOWN    = 1,
  PYRONET_HOST_NETWORK_STATE_JOINING = 2,
  PYRONET_HOST_NETWORK_STATE_JOINED  = 3,
} pyronet_host_network_state_t;

typedef enum {
  PYRONET_PARENT_CHANGE_PREFERRED_PARENT_CHANGED = 1,
  PYRONET_PARENT_CHANGE_PARENT_LOST              = 2,
} pyronet_host_parent_change_reason_t;

typedef enum {
  PYRONET_TX_STATUS_ACCEPTED = 0,
  PYRONET_TX_STATUS_SENT     = 1,
  PYRONET_TX_STATUS_ACKED    = 2,
  PYRONET_TX_STATUS_TIMEOUT  = 3,
  PYRONET_TX_STATUS_FAILED   = 4,
} pyronet_host_tx_status_t;

typedef enum {
  PYRONET_TX_DETAIL_NONE               = 0,
  PYRONET_TX_DETAIL_NOT_JOINED         = 1,
  PYRONET_TX_DETAIL_ROUTER_UNAVAILABLE = 2,
  PYRONET_TX_DETAIL_COAP_UNAVAILABLE   = 3,
  PYRONET_TX_DETAIL_SEND_REJECTED      = 4,
  PYRONET_TX_DETAIL_TRACK_EXHAUSTED    = 5,
  PYRONET_TX_DETAIL_TRACK_COMMIT       = 6,
  PYRONET_TX_DETAIL_BAD_COMMAND        = 7,
  PYRONET_TX_DETAIL_ROUTER_INVALID     = 8,
} pyronet_host_tx_detail_t;

typedef struct PYRONET_PACKED {
  uint8_t type;
  uint8_t version;
  uint16_t node_id;
  float latitude;
  float longitude;
  uint16_t fw_version;
  uint8_t battery_pct;
  uint8_t parent_ipv6[PYRONET_IPV6_ADDR_LEN];
} pyronet_mesh_registration_v1_t;

typedef struct PYRONET_PACKED {
  uint8_t type;
  uint8_t version;
  uint16_t node_id;
  uint32_t timestamp;
  uint8_t risk_level;
  int16_t temperature;
  uint16_t humidity;
  uint16_t bvoc_ppm;
  uint16_t pm25;
  uint8_t battery_pct;
} pyronet_mesh_sensor_report_v1_t;

typedef pyronet_mesh_sensor_report_v1_t pyronet_mesh_sensor_alert_v1_t;

typedef struct PYRONET_PACKED {
  uint8_t type;
  uint8_t version;
  uint16_t node_id;
  uint32_t timestamp;
  uint8_t parent_ipv6[PYRONET_IPV6_ADDR_LEN];
} pyronet_mesh_parent_update_v1_t;

typedef struct PYRONET_PACKED {
  uint8_t type;
  uint8_t version;
  uint16_t node_id;
  uint8_t risk_level;
  uint32_t timestamp;
} pyronet_mesh_neighbor_alert_v1_t;

typedef struct PYRONET_PACKED {
  uint8_t type;
  uint8_t version;
  uint16_t target_node_id;
  uint8_t neighbor_count;
} pyronet_mesh_nn_table_update_header_v1_t;

typedef struct PYRONET_PACKED {
  uint8_t type;
  uint8_t version;
  uint32_t unix_time_s;
} pyronet_mesh_time_sync_v1_t;

typedef struct PYRONET_PACKED {
  uint8_t type;
  uint8_t version;
  uint32_t config_id;
  int16_t l2_temp_thresh;
  uint16_t l2_humidity_thresh;
  uint16_t l2_bvoc_ppm_thresh;
  int16_t l3_temp_thresh;
  uint16_t l3_humidity_thresh;
  uint16_t l3_bvoc_ppm_thresh;
  uint16_t l4_bvoc_ppm_thresh;
  uint16_t l5_bvoc_ppm_thresh;
  uint16_t l5_pm25_thresh;
} pyronet_mesh_config_update_v1_t;

typedef struct {
  pyronet_mesh_nn_table_update_header_v1_t header;
  const uint8_t *neighbors;
} pyronet_mesh_nn_table_update_view_t;

typedef struct {
  uint8_t type;
  uint8_t code;
  uint16_t message_id;
  uint8_t token_len;
  uint8_t token[PYRONET_COAP_MAX_TOKEN_LEN];
  const uint8_t *payload;
  uint16_t payload_len;
} pyronet_coap_packet_t;

const char *pyronet_host_msg_name(uint8_t type);
const char *pyronet_tx_status_name(uint8_t status);

size_t pyronet_coap_build_post(uint8_t *datagram,
                               size_t datagram_size,
                               const char *uri,
                               bool confirmable,
                               uint16_t message_id,
                               uint8_t token,
                               const void *payload,
                               uint16_t payload_len);

size_t pyronet_coap_build_response(uint8_t *datagram,
                                   size_t datagram_size,
                                   const pyronet_coap_packet_t *request,
                                   uint8_t response_code);

bool pyronet_coap_parse(const uint8_t *datagram,
                        uint16_t datagram_len,
                        pyronet_coap_packet_t *out_packet);

bool pyronet_mesh_decode_neighbor_table_update(const uint8_t *payload,
                                               uint16_t payload_len,
                                               pyronet_mesh_nn_table_update_view_t *out_view);
bool pyronet_mesh_decode_time_sync(const uint8_t *payload,
                                   uint16_t payload_len,
                                   pyronet_mesh_time_sync_v1_t *out_packet);
bool pyronet_mesh_decode_config_update(const uint8_t *payload,
                                       uint16_t payload_len,
                                       pyronet_mesh_config_update_v1_t *out_packet);
bool pyronet_mesh_decode_neighbor_alert(const uint8_t *payload,
                                        uint16_t payload_len,
                                        pyronet_mesh_neighbor_alert_v1_t *out_packet);

#endif
