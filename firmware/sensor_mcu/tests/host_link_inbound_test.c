#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "transport/host_link.h"

typedef struct {
  bool registration_needed_called;
  bool parent_changed_called;
  bool tx_result_called;
  bool time_sync_called;
  bool neighbor_alert_called;
  bool config_update_called;
  pyronet_host_time_sync_update_v1_t time_sync_event;
  pyronet_host_neighbor_alert_received_v1_t neighbor_alert_event;
  pyronet_host_config_update_received_v1_t config_update_event;
} test_event_state_t;

static uint8_t test_uart_rx[512];
static size_t test_uart_rx_len;
static size_t test_uart_rx_index;
static uint64_t test_now_us;

static void test_queue_frame(uint8_t type,
                             uint8_t flags,
                             uint8_t seq,
                             const void *payload,
                             uint16_t payload_length)
{
  uint8_t frame[HOST_PROTO_MAX_FRAME_LEN];
  size_t frame_length;

  frame_length = host_proto_encode_frame(type,
                                         flags,
                                         seq,
                                         payload,
                                         payload_length,
                                         frame,
                                         sizeof(frame));
  assert(frame_length > 0U);
  assert((test_uart_rx_len + frame_length) <= sizeof(test_uart_rx));
  memcpy(&test_uart_rx[test_uart_rx_len], frame, frame_length);
  test_uart_rx_len += frame_length;
}

bool host_uart_init(void)
{
  return true;
}

bool host_uart_try_read_byte(uint8_t *out_byte)
{
  if ((out_byte == NULL) || (test_uart_rx_index >= test_uart_rx_len)) {
    return false;
  }

  *out_byte = test_uart_rx[test_uart_rx_index++];
  return true;
}

void host_uart_write(const uint8_t *data, size_t length)
{
  (void)data;
  (void)length;
}

uint64_t monotonic_time_now_us(void)
{
  return test_now_us;
}

static void test_on_time_sync(void *context,
                              const pyronet_host_time_sync_update_v1_t *event)
{
  test_event_state_t *state = (test_event_state_t *)context;

  assert(state != NULL);
  assert(event != NULL);
  state->time_sync_called = true;
  state->time_sync_event = *event;
}

static void test_on_neighbor_alert(
  void *context,
  const pyronet_host_neighbor_alert_received_v1_t *event)
{
  test_event_state_t *state = (test_event_state_t *)context;

  assert(state != NULL);
  assert(event != NULL);
  state->neighbor_alert_called = true;
  state->neighbor_alert_event = *event;
}

static void test_on_config_update(
  void *context,
  const pyronet_host_config_update_received_v1_t *event)
{
  test_event_state_t *state = (test_event_state_t *)context;

  assert(state != NULL);
  assert(event != NULL);
  state->config_update_called = true;
  state->config_update_event = *event;
}

static void test_host_link_dispatches_inbound_events(void)
{
  host_link_event_handlers_t handlers;
  host_hello_ack_v1_t hello_ack = {
    .endpoint = HOST_ENDPOINT_NCP,
    .proto_selected = HOST_PROTO_VERSION,
    .capabilities = HOST_CAP_STATUS_SUPPORTED,
    .reserved = 0U,
    .fw_version = 0x01020304UL,
  };
  pyronet_host_time_sync_update_v1_t time_sync = {
    .unix_time_s = 123456789U,
  };
  pyronet_host_neighbor_alert_received_v1_t neighbor_alert = {
    .node_id = 77U,
    .risk_level = 5U,
    .timestamp = 4444U,
  };
  pyronet_host_config_update_received_v1_t config_update = {
    .config_id = 9U,
    .l2_temp_thresh = 35,
    .l2_humidity_thresh = 40U,
    .l2_bvoc_ppm_thresh = 100U,
    .l3_temp_thresh = 45,
    .l3_humidity_thresh = 25U,
    .l3_bvoc_ppm_thresh = 200U,
    .l4_bvoc_ppm_thresh = 300U,
    .l5_bvoc_ppm_thresh = 500U,
    .l5_pm25_thresh = 35U,
  };
  test_event_state_t state = { 0 };

  memset(&handlers, 0, sizeof(handlers));
  handlers.context = &state;
  handlers.on_time_sync_update = test_on_time_sync;
  handlers.on_neighbor_alert_rx = test_on_neighbor_alert;
  handlers.on_config_update_rx = test_on_config_update;

  test_uart_rx_len = 0U;
  test_uart_rx_index = 0U;
  test_now_us = 0U;

  assert(host_link_init(&handlers));

  test_queue_frame(PYRONET_HOST_MSG_HELLO_ACK,
                   HOST_FRAME_FLAG_RESPONSE,
                   0U,
                   &hello_ack,
                   sizeof(hello_ack));
  test_queue_frame(PYRONET_HOST_MSG_TIME_SYNC_UPDATE,
                   HOST_FRAME_FLAG_EVENT,
                   1U,
                   &time_sync,
                   sizeof(time_sync));
  test_queue_frame(PYRONET_HOST_MSG_NEIGHBOR_ALERT_RX,
                   HOST_FRAME_FLAG_EVENT,
                   2U,
                   &neighbor_alert,
                   sizeof(neighbor_alert));
  test_queue_frame(PYRONET_HOST_MSG_CONFIG_UPDATE_RX,
                   HOST_FRAME_FLAG_EVENT,
                   3U,
                   &config_update,
                   sizeof(config_update));

  host_link_poll();

  assert(host_link_is_ready());
  assert(state.time_sync_called);
  assert(state.neighbor_alert_called);
  assert(state.config_update_called);
  assert(state.time_sync_event.unix_time_s == time_sync.unix_time_s);
  assert(state.neighbor_alert_event.node_id == neighbor_alert.node_id);
  assert(state.neighbor_alert_event.risk_level == neighbor_alert.risk_level);
  assert(state.neighbor_alert_event.timestamp == neighbor_alert.timestamp);
  assert(state.config_update_event.config_id == config_update.config_id);
  assert(state.config_update_event.l5_pm25_thresh == config_update.l5_pm25_thresh);
}

int main(void)
{
  test_host_link_dispatches_inbound_events();
  return 0;
}
