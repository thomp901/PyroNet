#include "pyronet_protocol.h"

#include <string.h>

#define PYRONET_COAP_VERSION 1U
#define PYRONET_COAP_TYPE_CON 0U
#define PYRONET_COAP_TYPE_NON 1U
#define PYRONET_COAP_TYPE_ACK 2U
#define PYRONET_COAP_METHOD_POST 2U
#define PYRONET_COAP_PAYLOAD_MARKER 0xFFU
#define PYRONET_COAP_URI_PATH_OPTION 11U
#define PYRONET_COAP_TOKEN_LEN 1U

const char *pyronet_host_msg_name(uint8_t type)
{
  switch (type) {
    case PYRONET_HOST_MSG_HELLO:
      return "HELLO";
    case PYRONET_HOST_MSG_HELLO_ACK:
      return "HELLO_ACK";
    case PYRONET_HOST_MSG_PING:
      return "PING";
    case PYRONET_HOST_MSG_PONG:
      return "PONG";
    case PYRONET_HOST_MSG_GET_STATUS:
      return "GET_STATUS";
    case PYRONET_HOST_MSG_STATUS:
      return "STATUS";
    case PYRONET_HOST_MSG_SEND_REGISTRATION:
      return "SEND_REGISTRATION";
    case PYRONET_HOST_MSG_SEND_SENSOR_REPORT:
      return "SEND_SENSOR_REPORT";
    case PYRONET_HOST_MSG_SEND_SENSOR_ALERT:
      return "SEND_SENSOR_ALERT";
    case PYRONET_HOST_MSG_SEND_NEIGHBOR_ALERT:
      return "SEND_NEIGHBOR_ALERT";
    case PYRONET_HOST_MSG_REQUEST_PARENT_UPDATE:
      return "REQUEST_PARENT_UPDATE";
    case PYRONET_HOST_MSG_REGISTRATION_NEEDED:
      return "REGISTRATION_NEEDED";
    case PYRONET_HOST_MSG_PARENT_CHANGED:
      return "PARENT_CHANGED";
    case PYRONET_HOST_MSG_TX_RESULT:
      return "TX_RESULT";
    case PYRONET_HOST_MSG_TIME_SYNC_UPDATE:
      return "TIME_SYNC_UPDATE";
    case PYRONET_HOST_MSG_NEIGHBOR_ALERT_RX:
      return "NEIGHBOR_ALERT_RX";
    case PYRONET_HOST_MSG_CONFIG_UPDATE_RX:
      return "CONFIG_UPDATE_RX";
    default:
      return "UNKNOWN";
  }
}

const char *pyronet_tx_status_name(uint8_t status)
{
  switch (status) {
    case PYRONET_TX_STATUS_ACCEPTED:
      return "ACCEPTED";
    case PYRONET_TX_STATUS_SENT:
      return "SENT";
    case PYRONET_TX_STATUS_ACKED:
      return "ACKED";
    case PYRONET_TX_STATUS_TIMEOUT:
      return "TIMEOUT";
    case PYRONET_TX_STATUS_FAILED:
      return "FAILED";
    default:
      return "UNKNOWN";
  }
}

size_t pyronet_coap_build_post(uint8_t *datagram,
                               size_t datagram_size,
                               const char *uri,
                               bool confirmable,
                               uint16_t message_id,
                               uint8_t token,
                               const void *payload,
                               uint16_t payload_len)
{
  size_t uri_len;
  size_t len;

  if ((datagram == NULL) || (uri == NULL) || (payload == NULL)) {
    return 0U;
  }

  uri_len = strlen(uri);
  if ((uri_len > 12U) || (payload_len == 0U)) {
    return 0U;
  }

  len = 7U + uri_len + payload_len;
  if (datagram_size < len) {
    return 0U;
  }

  datagram[0] = (uint8_t)((PYRONET_COAP_VERSION << 6)
                          | ((confirmable ? PYRONET_COAP_TYPE_CON : PYRONET_COAP_TYPE_NON) << 4)
                          | PYRONET_COAP_TOKEN_LEN);
  datagram[1] = PYRONET_COAP_METHOD_POST;
  datagram[2] = (uint8_t)(message_id >> 8);
  datagram[3] = (uint8_t)(message_id & 0xFFU);
  datagram[4] = token;
  datagram[5] = (uint8_t)((PYRONET_COAP_URI_PATH_OPTION << 4) | uri_len);
  memcpy(&datagram[6], uri, uri_len);
  datagram[6U + uri_len] = PYRONET_COAP_PAYLOAD_MARKER;
  memcpy(&datagram[7U + uri_len], payload, payload_len);

  return len;
}

size_t pyronet_coap_build_response(uint8_t *datagram,
                                   size_t datagram_size,
                                   const pyronet_coap_packet_t *request,
                                   uint8_t response_code)
{
  if ((datagram == NULL)
      || (request == NULL)
      || (request->token_len > PYRONET_COAP_MAX_TOKEN_LEN)
      || (datagram_size < (size_t)(4U + request->token_len))) {
    return 0U;
  }

  datagram[0] = (uint8_t)((PYRONET_COAP_VERSION << 6)
                          | (PYRONET_COAP_TYPE_ACK << 4)
                          | request->token_len);
  datagram[1] = response_code;
  datagram[2] = (uint8_t)(request->message_id >> 8);
  datagram[3] = (uint8_t)(request->message_id & 0xFFU);
  memcpy(&datagram[4], request->token, request->token_len);
  return (size_t)(4U + request->token_len);
}

bool pyronet_coap_parse(const uint8_t *datagram,
                        uint16_t datagram_len,
                        pyronet_coap_packet_t *out_packet)
{
  uint8_t version;
  uint8_t token_len;
  uint16_t index;

  if ((datagram == NULL) || (out_packet == NULL) || (datagram_len < 4U)) {
    return false;
  }

  version = datagram[0] >> 6;
  token_len = datagram[0] & 0x0FU;
  if ((version != PYRONET_COAP_VERSION) || (token_len > PYRONET_COAP_MAX_TOKEN_LEN)
      || (datagram_len < (uint16_t)(4U + token_len))) {
    return false;
  }

  memset(out_packet, 0, sizeof(*out_packet));
  out_packet->type = (datagram[0] >> 4) & 0x03U;
  out_packet->code = datagram[1];
  out_packet->message_id = (uint16_t)(((uint16_t)datagram[2] << 8) | datagram[3]);
  out_packet->token_len = token_len;
  memcpy(out_packet->token, &datagram[4], token_len);

  index = (uint16_t)(4U + token_len);
  while (index < datagram_len) {
    if (datagram[index] == PYRONET_COAP_PAYLOAD_MARKER) {
      index++;
      out_packet->payload = &datagram[index];
      out_packet->payload_len = (uint16_t)(datagram_len - index);
      return out_packet->payload_len > 0U;
    }
    index++;
  }

  return true;
}

static bool pyronet_mesh_decode_fixed_packet(const uint8_t *payload,
                                             uint16_t payload_len,
                                             uint8_t expected_type,
                                             void *out_packet,
                                             uint16_t expected_len)
{
  if ((payload == NULL) || (out_packet == NULL) || (payload_len != expected_len)) {
    return false;
  }

  if ((payload[0] != expected_type) || (payload[1] != PYRONET_MESH_SCHEMA_VERSION)) {
    return false;
  }

  memcpy(out_packet, payload, expected_len);
  return true;
}

bool pyronet_mesh_decode_neighbor_table_update(const uint8_t *payload,
                                               uint16_t payload_len,
                                               pyronet_mesh_nn_table_update_view_t *out_view)
{
  pyronet_mesh_nn_table_update_header_v1_t header;
  uint16_t expected_len;

  if ((payload == NULL) || (out_view == NULL) || (payload_len < sizeof(header))) {
    return false;
  }

  memcpy(&header, payload, sizeof(header));
  if ((header.type != PYRONET_PKT_NN_TABLE_UPDATE)
      || (header.version != PYRONET_MESH_SCHEMA_VERSION)
      || (header.neighbor_count > PYRONET_MAX_NEIGHBORS)) {
    return false;
  }

  expected_len = (uint16_t)(sizeof(header)
                            + ((uint16_t)header.neighbor_count * PYRONET_IPV6_ADDR_LEN));
  if (payload_len != expected_len) {
    return false;
  }

  out_view->header = header;
  out_view->neighbors = &payload[sizeof(header)];
  return true;
}

bool pyronet_mesh_decode_time_sync(const uint8_t *payload,
                                   uint16_t payload_len,
                                   pyronet_mesh_time_sync_v1_t *out_packet)
{
  return pyronet_mesh_decode_fixed_packet(payload,
                                          payload_len,
                                          PYRONET_PKT_TIME_SYNC,
                                          out_packet,
                                          sizeof(*out_packet));
}

bool pyronet_mesh_decode_config_update(const uint8_t *payload,
                                       uint16_t payload_len,
                                       pyronet_mesh_config_update_v1_t *out_packet)
{
  return pyronet_mesh_decode_fixed_packet(payload,
                                          payload_len,
                                          PYRONET_PKT_CONFIG_UPDATE,
                                          out_packet,
                                          sizeof(*out_packet));
}

bool pyronet_mesh_decode_neighbor_alert(const uint8_t *payload,
                                        uint16_t payload_len,
                                        pyronet_mesh_neighbor_alert_v1_t *out_packet)
{
  return pyronet_mesh_decode_fixed_packet(payload,
                                          payload_len,
                                          PYRONET_PKT_NEIGHBOR_ALERT,
                                          out_packet,
                                          sizeof(*out_packet));
}
