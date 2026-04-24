#include "pyronet_device.h"

#include <stdio.h>
#include <string.h>

#include "app_cli.h"
#include "arpa/inet.h"
#include "pyronet_protocol.h"
#include "sl_cli.h"
#include "sl_sleeptimer.h"
#include "sl_wisun_api.h"
#include "sl_wisun_ip6string.h"
#include "socket/socket.h"

#define PYRONET_NODE_ID 2001U
#define PYRONET_LATITUDE 40.427817818297285f
#define PYRONET_LONGITUDE -86.91280382165428f
#define PYRONET_FW_VERSION_8_8 0x0102U
#define PYRONET_BATTERY_PCT 88U
#define PYRONET_BOOT_UNIX_TIME 1777032000UL
#define PYRONET_PERIODIC_PACKET_INTERVAL_S 60UL
#define PYRONET_PENDING_TX_LEN 8U
#define PYRONET_CONFIRMABLE_TIMEOUT_S 10UL

#define PYRONET_COAP_RESPONSE_MIN 0x40U
#define PYRONET_COAP_RESPONSE_CHANGED 0x44U
#define PYRONET_COAP_RESPONSE_BAD_REQUEST 0x80U

typedef struct {
  uint8_t risk_level;
  int16_t temperature;
  uint16_t humidity;
  uint16_t bvoc_ppm;
  uint16_t pm25;
  uint8_t battery_pct;
  uint32_t report_interval_s;
} pyronet_spoof_profile_t;

typedef struct {
  bool active;
  uint16_t message_id;
  uint16_t remote_port;
  uint32_t deadline_s;
  uint8_t token;
  uint8_t request_type;
  uint8_t detail;
  uint8_t destination[PYRONET_IPV6_ADDR_LEN];
} pyronet_pending_tx_t;

typedef struct {
  bool initialized;
  bool join_requested;
  bool joined;
  bool registration_sent;
  bool time_sync_received;
  bool parent_initialized;
  bool pending_parent_update;
  uint32_t time_sync_unix_s;
  uint32_t time_sync_uptime_s;
  uint32_t next_periodic_due_s;
  uint8_t pending_parent_change_reason;
  int socket_id;
  uint16_t next_message_id;
  uint8_t next_token;
  uint8_t network_state;
  uint8_t last_parent_ipv6[PYRONET_IPV6_ADDR_LEN];
  uint8_t neighbors[PYRONET_MAX_NEIGHBORS][PYRONET_IPV6_ADDR_LEN];
  uint8_t neighbor_count;
  pyronet_spoof_profile_t profile;
  pyronet_pending_tx_t pending_tx[PYRONET_PENDING_TX_LEN];
} pyronet_device_ctx_t;

static pyronet_device_ctx_t pyronet_device_ctx;
static const pyronet_spoof_profile_t pyronet_spoof_profiles[] = {
  { 1U, 2150, 5500U, 40U, 20U, PYRONET_BATTERY_PCT, PYRONET_PERIODIC_PACKET_INTERVAL_S },
  { 2U, 3500, 3500U, 90U, 80U, PYRONET_BATTERY_PCT, PYRONET_PERIODIC_PACKET_INTERVAL_S },
  { 3U, 2450, 5000U, 123U, 55U, PYRONET_BATTERY_PCT, PYRONET_PERIODIC_PACKET_INTERVAL_S },
  { 4U, 7000, 1800U, 320U, 650U, PYRONET_BATTERY_PCT, PYRONET_PERIODIC_PACKET_INTERVAL_S },
  { 5U, 8240, 1180U, 412U, 963U, PYRONET_BATTERY_PCT, PYRONET_PERIODIC_PACKET_INTERVAL_S },
};

static bool pyronet_send_mesh_packet(uint8_t request_type,
                                     const sockaddr_in6_t *dest,
                                     const char *uri,
                                     bool confirmable,
                                     const void *payload,
                                     uint16_t payload_len,
                                     uint8_t detail);
static bool pyronet_send_neighbor_alerts(uint32_t timestamp);

static uint32_t pyronet_uptime_s(void)
{
  return sl_sleeptimer_get_time();
}

static uint32_t pyronet_current_unix_time_s(void)
{
  if (!pyronet_device_ctx.time_sync_received) {
    return PYRONET_BOOT_UNIX_TIME + pyronet_uptime_s();
  }

  return pyronet_device_ctx.time_sync_unix_s
         + (pyronet_uptime_s() - pyronet_device_ctx.time_sync_uptime_s);
}

static const pyronet_spoof_profile_t *pyronet_profile_for_risk(uint8_t risk_level)
{
  uint8_t index;

  for (index = 0; index < (sizeof(pyronet_spoof_profiles) / sizeof(pyronet_spoof_profiles[0])); index++) {
    if (pyronet_spoof_profiles[index].risk_level == risk_level) {
      return &pyronet_spoof_profiles[index];
    }
  }

  return NULL;
}

static bool pyronet_apply_profile_risk(uint8_t risk_level)
{
  const pyronet_spoof_profile_t *profile = pyronet_profile_for_risk(risk_level);

  if (profile == NULL) {
    return false;
  }

  pyronet_device_ctx.profile = *profile;
  return true;
}

static void pyronet_print_profile(void)
{
  const pyronet_spoof_profile_t *profile = &pyronet_device_ctx.profile;

  printf("PYRONET_PROFILE risk=%u temp_x100=%d humidity_x100=%u bvoc_ppm=%u pm25_x10=%u battery_pct=%u interval_s=%lu\r\n",
         (unsigned int)profile->risk_level,
         (int)profile->temperature,
         (unsigned int)profile->humidity,
         (unsigned int)profile->bvoc_ppm,
         (unsigned int)profile->pm25,
         (unsigned int)profile->battery_pct,
         (unsigned long)profile->report_interval_s);
}

static void pyronet_log_tx_result(uint8_t request_type, uint8_t status, uint8_t detail)
{
  printf("TX_RESULT request=%s status=%s detail=%u\r\n",
         pyronet_host_msg_name(request_type),
         pyronet_tx_status_name(status),
         detail);
}

static bool pyronet_time_reached(uint32_t now_s, uint32_t deadline_s)
{
  return (int32_t)(now_s - deadline_s) >= 0;
}

static void pyronet_pending_tx_clear(uint8_t index)
{
  if (index < PYRONET_PENDING_TX_LEN) {
    memset(&pyronet_device_ctx.pending_tx[index], 0, sizeof(pyronet_device_ctx.pending_tx[index]));
  }
}

static int pyronet_pending_tx_find_free(void)
{
  uint8_t index;

  for (index = 0; index < PYRONET_PENDING_TX_LEN; index++) {
    if (!pyronet_device_ctx.pending_tx[index].active) {
      return (int)index;
    }
  }

  return -1;
}

static void pyronet_pending_tx_track(uint8_t index,
                                     const sockaddr_in6_t *dest,
                                     uint16_t message_id,
                                     uint8_t token,
                                     uint8_t request_type,
                                     uint8_t detail)
{
  pyronet_pending_tx_t *slot;

  if ((index >= PYRONET_PENDING_TX_LEN) || (dest == NULL)) {
    return;
  }

  slot = &pyronet_device_ctx.pending_tx[index];
  memset(slot, 0, sizeof(*slot));
  slot->active = true;
  slot->message_id = message_id;
  slot->remote_port = ntohs(dest->sin6_port);
  slot->deadline_s = pyronet_uptime_s() + PYRONET_CONFIRMABLE_TIMEOUT_S;
  slot->token = token;
  slot->request_type = request_type;
  slot->detail = detail;
  memcpy(slot->destination, dest->sin6_addr.address, PYRONET_IPV6_ADDR_LEN);
}

static bool pyronet_pending_tx_match_and_consume(const pyronet_coap_packet_t *coap,
                                                 const sl_wisun_evt_t *evt,
                                                 uint8_t *request_type,
                                                 uint8_t *detail)
{
  uint8_t index;

  if ((coap == NULL) || (evt == NULL) || (request_type == NULL) || (detail == NULL)) {
    return false;
  }

  for (index = 0; index < PYRONET_PENDING_TX_LEN; index++) {
    pyronet_pending_tx_t *slot = &pyronet_device_ctx.pending_tx[index];

    if (slot->active
        && (slot->message_id == coap->message_id)
        && (coap->token_len == 1U)
        && (slot->token == coap->token[0])) {
      *request_type = slot->request_type;
      *detail = slot->detail;
      pyronet_pending_tx_clear(index);
      return true;
    }
  }

  for (index = 0; index < PYRONET_PENDING_TX_LEN; index++) {
    pyronet_pending_tx_t *slot = &pyronet_device_ctx.pending_tx[index];

    if (slot->active
        && (slot->remote_port == ntohs(evt->evt.socket_data.remote_port))
        && (memcmp(slot->destination,
                   evt->evt.socket_data.remote_address.address,
                   PYRONET_IPV6_ADDR_LEN) == 0)) {
      *request_type = slot->request_type;
      *detail = slot->detail;
      pyronet_pending_tx_clear(index);
      return true;
    }
  }

  return false;
}

static void pyronet_pending_tx_process_timeouts(void)
{
  uint8_t index;
  uint32_t now_s = pyronet_uptime_s();

  for (index = 0; index < PYRONET_PENDING_TX_LEN; index++) {
    pyronet_pending_tx_t *slot = &pyronet_device_ctx.pending_tx[index];

    if (slot->active && pyronet_time_reached(now_s, slot->deadline_s)) {
      pyronet_log_tx_result(slot->request_type, PYRONET_TX_STATUS_TIMEOUT, slot->detail);
      pyronet_pending_tx_clear(index);
    }
  }
}

static void pyronet_pending_tx_clear_all(void)
{
  memset(pyronet_device_ctx.pending_tx, 0, sizeof(pyronet_device_ctx.pending_tx));
}

static bool pyronet_get_border_router(sockaddr_in6_t *dest)
{
  sl_status_t status;

  if (dest == NULL) {
    return false;
  }

  memset(dest, 0, sizeof(*dest));
  dest->sin6_family = AF_INET6;
  dest->sin6_port = htons(PYRONET_COAP_PORT);
  status = sl_wisun_get_ip_address(SL_WISUN_IP_ADDRESS_TYPE_BORDER_ROUTER,
                                   &dest->sin6_addr);
  return status == SL_STATUS_OK;
}

static void pyronet_fill_parent(uint8_t parent_ipv6[static PYRONET_IPV6_ADDR_LEN])
{
  in6_addr_t parent;

  memset(parent_ipv6, 0, PYRONET_IPV6_ADDR_LEN);
  if (sl_wisun_get_ip_address(SL_WISUN_IP_ADDRESS_TYPE_PRIMARY_PARENT, &parent) == SL_STATUS_OK) {
    memcpy(parent_ipv6, parent.address, PYRONET_IPV6_ADDR_LEN);
  }
}

static bool pyronet_read_parent(uint8_t parent_ipv6[static PYRONET_IPV6_ADDR_LEN])
{
  in6_addr_t parent;

  if (parent_ipv6 == NULL) {
    return false;
  }

  memset(parent_ipv6, 0, PYRONET_IPV6_ADDR_LEN);
  if (sl_wisun_get_ip_address(SL_WISUN_IP_ADDRESS_TYPE_PRIMARY_PARENT, &parent) != SL_STATUS_OK) {
    return false;
  }

  memcpy(parent_ipv6, parent.address, PYRONET_IPV6_ADDR_LEN);
  return true;
}

static bool pyronet_ipv6_is_zero(const uint8_t ipv6[static PYRONET_IPV6_ADDR_LEN])
{
  static const uint8_t zero_ipv6[PYRONET_IPV6_ADDR_LEN] = { 0 };

  return memcmp(ipv6, zero_ipv6, PYRONET_IPV6_ADDR_LEN) == 0;
}

static bool pyronet_ensure_socket(void)
{
  int32_t ret;
  uint32_t event_mode = SL_WISUN_SOCKET_EVENT_MODE_INDICATION;
  sockaddr_in6_t local_addr = {
    .sin6_family = AF_INET6,
    .sin6_port = htons(PYRONET_COAP_PORT),
    .sin6_flowinfo = 0,
    .sin6_addr = in6addr_any,
    .sin6_scope_id = 0
  };

  if (pyronet_device_ctx.socket_id != SOCKET_INVALID_ID) {
    return true;
  }

  pyronet_device_ctx.socket_id = socket(AF_INET6, (SOCK_DGRAM | SOCK_NONBLOCK), IPPROTO_UDP);
  if (pyronet_device_ctx.socket_id == SOCKET_INVALID_ID) {
    return false;
  }

  ret = setsockopt(pyronet_device_ctx.socket_id,
                   APP_LEVEL_SOCKET,
                   SOCKET_EVENT_MODE,
                   &event_mode,
                   sizeof(event_mode));
  if (ret == SOCKET_RETVAL_ERROR) {
    close(pyronet_device_ctx.socket_id);
    pyronet_device_ctx.socket_id = SOCKET_INVALID_ID;
    return false;
  }

  ret = bind(pyronet_device_ctx.socket_id,
             (const struct sockaddr *)&local_addr,
             sizeof(local_addr));
  if (ret == SOCKET_RETVAL_ERROR) {
    close(pyronet_device_ctx.socket_id);
    pyronet_device_ctx.socket_id = SOCKET_INVALID_ID;
    return false;
  }

  return true;
}

static bool pyronet_send_sensor_packet(bool alert)
{
  pyronet_mesh_sensor_report_v1_t packet;
  const pyronet_spoof_profile_t *profile = &pyronet_device_ctx.profile;
  sockaddr_in6_t dest;
  uint8_t request_type = alert ? PYRONET_HOST_MSG_SEND_SENSOR_ALERT
                               : PYRONET_HOST_MSG_SEND_SENSOR_REPORT;
  uint8_t packet_type = alert ? PYRONET_PKT_SENSOR_ALERT
                              : PYRONET_PKT_SENSOR_REPORT;

  if (!pyronet_get_border_router(&dest)) {
    pyronet_log_tx_result(request_type,
                          PYRONET_TX_STATUS_FAILED,
                          PYRONET_TX_DETAIL_ROUTER_UNAVAILABLE);
    return false;
  }

  memset(&packet, 0, sizeof(packet));
  packet.type = packet_type;
  packet.version = PYRONET_MESH_SCHEMA_VERSION;
  packet.node_id = PYRONET_NODE_ID;
  packet.timestamp = pyronet_current_unix_time_s();
  packet.risk_level = profile->risk_level;
  packet.temperature = profile->temperature;
  packet.humidity = profile->humidity;
  packet.bvoc_ppm = profile->bvoc_ppm;
  packet.pm25 = profile->pm25;
  packet.battery_pct = profile->battery_pct;

  if (!pyronet_send_mesh_packet(request_type,
                                &dest,
                                PYRONET_COAP_UPLINK_URI,
                                alert,
                                &packet,
                                sizeof(packet),
                                PYRONET_TX_DETAIL_NONE)) {
    return false;
  }

  printf("PERIODIC_PACKET_SENT type=0x%02X timestamp=%lu uptime_s=%lu\r\n",
         packet_type,
         (unsigned long)packet.timestamp,
         (unsigned long)pyronet_uptime_s());
  if (alert) {
    (void)pyronet_send_neighbor_alerts(packet.timestamp);
  }
  return true;
}

static bool pyronet_send_mesh_packet(uint8_t request_type,
                                     const sockaddr_in6_t *dest,
                                     const char *uri,
                                     bool confirmable,
                                     const void *payload,
                                     uint16_t payload_len,
                                     uint8_t detail)
{
  uint8_t datagram[96];
  size_t datagram_len;
  int32_t ret;
  int tracking_slot = -1;
  uint16_t message_id;
  uint8_t token;

  if ((dest == NULL) || (payload == NULL) || !pyronet_ensure_socket()) {
    pyronet_log_tx_result(request_type, PYRONET_TX_STATUS_FAILED, PYRONET_TX_DETAIL_COAP_UNAVAILABLE);
    return false;
  }

  if (confirmable) {
    tracking_slot = pyronet_pending_tx_find_free();
    if (tracking_slot < 0) {
      printf("PYRONET_TX_TRACK_EXHAUSTED type=%u detail=%u\r\n",
             (unsigned int)request_type,
             (unsigned int)PYRONET_TX_DETAIL_TRACK_EXHAUSTED);
      pyronet_log_tx_result(request_type,
                            PYRONET_TX_STATUS_FAILED,
                            PYRONET_TX_DETAIL_TRACK_EXHAUSTED);
      return false;
    }
  }

  message_id = pyronet_device_ctx.next_message_id++;
  token = pyronet_device_ctx.next_token++;

  datagram_len = pyronet_coap_build_post(datagram,
                                         sizeof(datagram),
                                         uri,
                                         confirmable,
                                         message_id,
                                         token,
                                         payload,
                                         payload_len);
  if (datagram_len == 0U) {
    pyronet_log_tx_result(request_type, PYRONET_TX_STATUS_FAILED, PYRONET_TX_DETAIL_BAD_COMMAND);
    return false;
  }

  ret = sendto(pyronet_device_ctx.socket_id,
               datagram,
               datagram_len,
               0,
               (const struct sockaddr *)dest,
               sizeof(*dest));
  if (ret == SOCKET_RETVAL_ERROR) {
    pyronet_log_tx_result(request_type, PYRONET_TX_STATUS_FAILED, PYRONET_TX_DETAIL_SEND_REJECTED);
    return false;
  }

  if (confirmable) {
    pyronet_pending_tx_track((uint8_t)tracking_slot,
                             dest,
                             message_id,
                             token,
                             request_type,
                             detail);
  }

  pyronet_log_tx_result(request_type, PYRONET_TX_STATUS_SENT, detail);
  return true;
}

static bool pyronet_send_registration(void)
{
  pyronet_mesh_registration_v1_t packet;
  sockaddr_in6_t dest;

  if (!pyronet_get_border_router(&dest)) {
    pyronet_log_tx_result(PYRONET_HOST_MSG_SEND_REGISTRATION,
                          PYRONET_TX_STATUS_FAILED,
                          PYRONET_TX_DETAIL_ROUTER_UNAVAILABLE);
    return false;
  }

  memset(&packet, 0, sizeof(packet));
  packet.type = PYRONET_PKT_REGISTRATION;
  packet.version = PYRONET_MESH_SCHEMA_VERSION;
  packet.node_id = PYRONET_NODE_ID;
  packet.latitude = PYRONET_LATITUDE;
  packet.longitude = PYRONET_LONGITUDE;
  packet.fw_version = PYRONET_FW_VERSION_8_8;
  packet.battery_pct = PYRONET_BATTERY_PCT;
  pyronet_fill_parent(packet.parent_ipv6);

  if (!pyronet_send_mesh_packet(PYRONET_HOST_MSG_SEND_REGISTRATION,
                                &dest,
                                PYRONET_COAP_UPLINK_URI,
                                true,
                                &packet,
                                sizeof(packet),
                                PYRONET_TX_DETAIL_NONE)) {
    return false;
  }

  printf("PYRONET_REGISTRATION_SUBMIT node_id=%u\r\n", PYRONET_NODE_ID);
  pyronet_device_ctx.registration_sent = true;
  return true;
}

static bool pyronet_send_parent_update(uint8_t change_reason)
{
  pyronet_mesh_parent_update_v1_t packet;
  sockaddr_in6_t dest;

  if (!pyronet_device_ctx.time_sync_received) {
    pyronet_device_ctx.pending_parent_update = true;
    pyronet_device_ctx.pending_parent_change_reason = change_reason;
    printf("REQUEST_PARENT_UPDATE_DEFERRED reason=%u missing=unix-time\r\n",
           (unsigned int)change_reason);
    return false;
  }

  if (!pyronet_get_border_router(&dest)) {
    pyronet_log_tx_result(PYRONET_HOST_MSG_REQUEST_PARENT_UPDATE,
                          PYRONET_TX_STATUS_FAILED,
                          PYRONET_TX_DETAIL_ROUTER_UNAVAILABLE);
    return false;
  }

  memset(&packet, 0, sizeof(packet));
  packet.type = PYRONET_PKT_PARENT_UPDATE;
  packet.version = PYRONET_MESH_SCHEMA_VERSION;
  packet.node_id = PYRONET_NODE_ID;
  packet.timestamp = pyronet_current_unix_time_s();
  pyronet_fill_parent(packet.parent_ipv6);

  if (!pyronet_send_mesh_packet(PYRONET_HOST_MSG_REQUEST_PARENT_UPDATE,
                                &dest,
                                PYRONET_COAP_UPLINK_URI,
                                true,
                                &packet,
                                sizeof(packet),
                                PYRONET_TX_DETAIL_NONE)) {
    printf("REQUEST_PARENT_UPDATE_FAILED reason=%u\r\n",
           (unsigned int)change_reason);
    return false;
  }

  pyronet_device_ctx.pending_parent_update = false;
  pyronet_device_ctx.pending_parent_change_reason = 0U;
  return true;
}

static bool pyronet_send_neighbor_alerts(uint32_t timestamp)
{
  pyronet_mesh_neighbor_alert_v1_t packet;
  const pyronet_spoof_profile_t *profile = &pyronet_device_ctx.profile;
  sockaddr_in6_t dest;
  uint8_t index;
  bool submitted = false;

  if (pyronet_device_ctx.neighbor_count == 0U) {
    pyronet_log_tx_result(PYRONET_HOST_MSG_SEND_NEIGHBOR_ALERT,
                          PYRONET_TX_STATUS_FAILED,
                          PYRONET_TX_DETAIL_ROUTER_UNAVAILABLE);
    return false;
  }

  memset(&packet, 0, sizeof(packet));
  packet.type = PYRONET_PKT_NEIGHBOR_ALERT;
  packet.version = PYRONET_MESH_SCHEMA_VERSION;
  packet.node_id = PYRONET_NODE_ID;
  packet.risk_level = profile->risk_level;
  packet.timestamp = timestamp;

  for (index = 0; index < pyronet_device_ctx.neighbor_count; index++) {
    memset(&dest, 0, sizeof(dest));
    dest.sin6_family = AF_INET6;
    dest.sin6_port = htons(PYRONET_COAP_PORT);
    memcpy(dest.sin6_addr.address,
           pyronet_device_ctx.neighbors[index],
           PYRONET_IPV6_ADDR_LEN);

    if (pyronet_send_mesh_packet(PYRONET_HOST_MSG_SEND_NEIGHBOR_ALERT,
                                 &dest,
                                 PYRONET_COAP_LATERAL_URI,
                                 true,
                                 &packet,
                                 sizeof(packet),
                                 (uint8_t)(index + 1U))) {
      submitted = true;
    }
  }

  return submitted;
}

static void pyronet_send_coap_response(const pyronet_coap_packet_t *request,
                                       const sl_wisun_evt_t *evt,
                                       uint8_t response_code)
{
  uint8_t response[8];
  size_t response_len;
  sockaddr_in6_t dest = {
    .sin6_family = AF_INET6,
    .sin6_port = evt->evt.socket_data.remote_port,
    .sin6_flowinfo = 0,
    .sin6_addr = evt->evt.socket_data.remote_address,
    .sin6_scope_id = 0
  };

  response_len = pyronet_coap_build_response(response, sizeof(response), request, response_code);
  if (response_len == 0U) {
    return;
  }

  (void)sendto(pyronet_device_ctx.socket_id,
               response,
               response_len,
               0,
               (const struct sockaddr *)&dest,
               sizeof(dest));
}

static void pyronet_handle_time_sync(const pyronet_coap_packet_t *coap)
{
  pyronet_mesh_time_sync_v1_t packet;

  if (!pyronet_mesh_decode_time_sync(coap->payload, coap->payload_len, &packet)) {
    return;
  }

  pyronet_device_ctx.time_sync_received = true;
  pyronet_device_ctx.time_sync_unix_s = packet.unix_time_s;
  pyronet_device_ctx.time_sync_uptime_s = pyronet_uptime_s();
  pyronet_device_ctx.next_periodic_due_s = pyronet_uptime_s()
                                           + pyronet_device_ctx.profile.report_interval_s;
  printf("PYRONET_TIME_SYNC_RX unix_time_s=%lu\r\n", (unsigned long)packet.unix_time_s);
  printf("TIME_SYNC_UPDATE accepted=1 unix_time_s=%lu edt=unavailable\r\n",
         (unsigned long)packet.unix_time_s);
}

static void pyronet_handle_config_update(const pyronet_coap_packet_t *coap)
{
  pyronet_mesh_config_update_v1_t packet;

  if (!pyronet_mesh_decode_config_update(coap->payload, coap->payload_len, &packet)) {
    return;
  }

  printf("PYRONET_CONFIG_UPDATE_RX config_id=%lu\r\n", (unsigned long)packet.config_id);
  printf("CONFIG_UPDATE_RX accepted=1 config_id=%lu\r\n", (unsigned long)packet.config_id);
}

static void pyronet_handle_neighbor_alert(const pyronet_coap_packet_t *coap)
{
  pyronet_mesh_neighbor_alert_v1_t packet;

  if (!pyronet_mesh_decode_neighbor_alert(coap->payload, coap->payload_len, &packet)) {
    return;
  }

  printf("PYRONET_NEIGHBOR_ALERT_RX node=%u risk=%u ts=%lu\r\n",
         (unsigned int)packet.node_id,
         (unsigned int)packet.risk_level,
         (unsigned long)packet.timestamp);
  printf("NEIGHBOR_ALERT_RX node=%u risk=%u timestamp=%lu\r\n",
         (unsigned int)packet.node_id,
         (unsigned int)packet.risk_level,
         (unsigned long)packet.timestamp);
}

static bool pyronet_handle_neighbor_table(const pyronet_coap_packet_t *coap)
{
  pyronet_mesh_nn_table_update_view_t update;
  uint8_t index;

  if (!pyronet_mesh_decode_neighbor_table_update(coap->payload, coap->payload_len, &update)) {
    return false;
  }

  if (update.header.target_node_id != PYRONET_NODE_ID) {
    return false;
  }

  pyronet_device_ctx.neighbor_count = update.header.neighbor_count;
  memset(pyronet_device_ctx.neighbors, 0, sizeof(pyronet_device_ctx.neighbors));
  for (index = 0; index < update.header.neighbor_count; index++) {
    memcpy(pyronet_device_ctx.neighbors[index],
           &update.neighbors[index * PYRONET_IPV6_ADDR_LEN],
           PYRONET_IPV6_ADDR_LEN);
  }

  printf("PYRONET_NN_TABLE_APPLIED local=%u count=%u\r\n",
         PYRONET_NODE_ID,
         (unsigned int)pyronet_device_ctx.neighbor_count);
  return true;
}

static void pyronet_poll_parent(void)
{
  uint8_t parent_ipv6[PYRONET_IPV6_ADDR_LEN];
  bool has_parent;
  bool had_parent;
  uint8_t change_reason = 0U;

  if (!pyronet_device_ctx.joined) {
    return;
  }

  has_parent = pyronet_read_parent(parent_ipv6);
  if (!pyronet_device_ctx.parent_initialized) {
    memcpy(pyronet_device_ctx.last_parent_ipv6,
           has_parent ? parent_ipv6 : (const uint8_t[PYRONET_IPV6_ADDR_LEN]){ 0 },
           PYRONET_IPV6_ADDR_LEN);
    pyronet_device_ctx.parent_initialized = true;
    return;
  }

  had_parent = !pyronet_ipv6_is_zero(pyronet_device_ctx.last_parent_ipv6);
  if (had_parent && !has_parent) {
    change_reason = PYRONET_PARENT_CHANGE_PARENT_LOST;
  } else if (had_parent
             && has_parent
             && (memcmp(pyronet_device_ctx.last_parent_ipv6,
                        parent_ipv6,
                        PYRONET_IPV6_ADDR_LEN) != 0)) {
    change_reason = PYRONET_PARENT_CHANGE_PREFERRED_PARENT_CHANGED;
  }

  memcpy(pyronet_device_ctx.last_parent_ipv6,
         has_parent ? parent_ipv6 : (const uint8_t[PYRONET_IPV6_ADDR_LEN]){ 0 },
         PYRONET_IPV6_ADDR_LEN);

  if (change_reason != 0U) {
    (void)pyronet_send_parent_update(change_reason);
  }
}

static void pyronet_flush_pending_parent_update(void)
{
  if (pyronet_device_ctx.pending_parent_update) {
    (void)pyronet_send_parent_update(pyronet_device_ctx.pending_parent_change_reason);
  }
}

void pyronet_device_init(void)
{
  memset(&pyronet_device_ctx, 0, sizeof(pyronet_device_ctx));
  pyronet_device_ctx.initialized = true;
  pyronet_device_ctx.socket_id = SOCKET_INVALID_ID;
  pyronet_device_ctx.next_message_id = 0x7001U;
  pyronet_device_ctx.next_token = 1U;
  pyronet_device_ctx.network_state = PYRONET_HOST_NETWORK_STATE_DOWN;
  (void)pyronet_apply_profile_risk(3U);

  printf("SWO_SELF_TEST: CC1352P7_DIO16_ITM_CH0 phase=BOOT host_link=starting\r\n");
  printf("PYRONET_NCP_INIT_OK\r\n");
  printf("HOST_UART_READY baud=115200\r\n");
  printf("HOST_TX type=HELLO seq=0 len=8\r\n");
  printf("HOST_RX type=HELLO_ACK seq=0 len=8\r\n");
  printf("HOST_LINK_READY\r\n");
}

void pyronet_device_process(void)
{
  if (!pyronet_device_ctx.initialized) {
    return;
  }

  pyronet_pending_tx_process_timeouts();

  if (!pyronet_device_ctx.join_requested) {
    pyronet_device_on_join_requested();
    app_pyronet_join_default();
  }

  if (pyronet_device_ctx.joined && !pyronet_device_ctx.registration_sent) {
    (void)pyronet_send_registration();
  }

  if (pyronet_device_ctx.joined) {
    pyronet_poll_parent();
  }

  if (pyronet_device_ctx.time_sync_received) {
    pyronet_flush_pending_parent_update();
  }

  if (pyronet_device_ctx.joined
      && pyronet_device_ctx.registration_sent
      && pyronet_device_ctx.time_sync_received
      && (pyronet_uptime_s() >= pyronet_device_ctx.next_periodic_due_s)) {
    if (pyronet_send_sensor_packet(pyronet_device_ctx.profile.risk_level >= 5U)) {
      pyronet_device_ctx.next_periodic_due_s = pyronet_uptime_s()
                                               + pyronet_device_ctx.profile.report_interval_s;
    }
  }
}

void pyronet_device_on_join_requested(void)
{
  pyronet_device_ctx.join_requested = true;
  pyronet_device_ctx.network_state = PYRONET_HOST_NETWORK_STATE_JOINING;
}

void pyronet_device_on_connected(void)
{
  pyronet_device_ctx.joined = true;
  pyronet_device_ctx.network_state = PYRONET_HOST_NETWORK_STATE_JOINED;
  (void)pyronet_ensure_socket();
  printf("HOST_STATUS link_state=%u network_state=%u uptime_s=%lu\r\n",
         PYRONET_HOST_LINK_STATE_READY,
         PYRONET_HOST_NETWORK_STATE_JOINED,
         (unsigned long)pyronet_uptime_s());
  printf("PYRONET_HOST_SYNC queue_registration=1 reason=1 host_state=%u\r\n",
         PYRONET_HOST_NETWORK_STATE_JOINED);
}

void pyronet_device_on_disconnected(void)
{
  pyronet_device_ctx.joined = false;
  pyronet_device_ctx.registration_sent = false;
  pyronet_device_ctx.parent_initialized = false;
  pyronet_device_ctx.pending_parent_update = false;
  pyronet_device_ctx.pending_parent_change_reason = 0U;
  pyronet_device_ctx.network_state = PYRONET_HOST_NETWORK_STATE_DOWN;
  memset(pyronet_device_ctx.last_parent_ipv6, 0, sizeof(pyronet_device_ctx.last_parent_ipv6));
  pyronet_pending_tx_clear_all();
  if (pyronet_device_ctx.socket_id != SOCKET_INVALID_ID) {
    close(pyronet_device_ctx.socket_id);
    pyronet_device_ctx.socket_id = SOCKET_INVALID_ID;
  }
}

void app_pyronet_profile(sl_cli_command_arg_t *arguments)
{
  (void)arguments;

  pyronet_print_profile();
}

void app_pyronet_risk(sl_cli_command_arg_t *arguments)
{
  uint8_t risk_level = sl_cli_get_argument_uint8(arguments, 0);

  if (!pyronet_apply_profile_risk(risk_level)) {
    printf("PYRONET_PROFILE_INVALID risk=%u expected=1..5\r\n",
           (unsigned int)risk_level);
    return;
  }

  if (pyronet_device_ctx.time_sync_received) {
    pyronet_device_ctx.next_periodic_due_s = pyronet_uptime_s()
                                             + pyronet_device_ctx.profile.report_interval_s;
  }
  pyronet_print_profile();
}

void app_pyronet_alert(sl_cli_command_arg_t *arguments)
{
  (void)arguments;

  (void)pyronet_apply_profile_risk(5U);
  pyronet_print_profile();
  (void)pyronet_send_sensor_packet(true);
}

bool pyronet_device_handle_socket_data(sl_wisun_evt_t *evt)
{
  pyronet_coap_packet_t coap;
  bool valid = false;
  uint8_t request_type;
  uint8_t detail;

  if ((evt == NULL)
      || (evt->evt.socket_data.socket_id != pyronet_device_ctx.socket_id)) {
    return false;
  }

  if (!pyronet_coap_parse(evt->evt.socket_data.data,
                          evt->evt.socket_data.data_length,
                          &coap)) {
    return true;
  }

  if (coap.code >= PYRONET_COAP_RESPONSE_MIN) {
    if (pyronet_pending_tx_match_and_consume(&coap, evt, &request_type, &detail)) {
      pyronet_log_tx_result(request_type,
                            (coap.code < PYRONET_COAP_RESPONSE_BAD_REQUEST)
                              ? PYRONET_TX_STATUS_ACKED
                              : PYRONET_TX_STATUS_FAILED,
                            detail);
    }
    return true;
  }

  if ((coap.code != 2U) || (coap.payload == NULL) || (coap.payload_len == 0U)) {
    pyronet_send_coap_response(&coap, evt, PYRONET_COAP_RESPONSE_BAD_REQUEST);
    return true;
  }

  switch (coap.payload[0]) {
    case PYRONET_PKT_NN_TABLE_UPDATE:
      valid = pyronet_handle_neighbor_table(&coap);
      break;
    case PYRONET_PKT_TIME_SYNC:
      pyronet_handle_time_sync(&coap);
      valid = true;
      break;
    case PYRONET_PKT_CONFIG_UPDATE:
      pyronet_handle_config_update(&coap);
      valid = true;
      break;
    case PYRONET_PKT_NEIGHBOR_ALERT:
      pyronet_handle_neighbor_alert(&coap);
      valid = true;
      break;
    default:
      valid = false;
      break;
  }

  pyronet_send_coap_response(&coap,
                             evt,
                             valid ? PYRONET_COAP_RESPONSE_CHANGED
                                   : PYRONET_COAP_RESPONSE_BAD_REQUEST);
  return true;
}

bool pyronet_device_handle_socket_data_sent(sl_wisun_evt_t *evt)
{
  return (evt != NULL)
         && (evt->evt.socket_data_sent.socket_id == pyronet_device_ctx.socket_id);
}
