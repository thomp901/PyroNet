#include "transport/host_link.h"

#include <stdio.h>
#include <string.h>

#include "platform/monotonic_time.h"
#include "transport/host_uart.h"

#define HOST_LINK_HELLO_ACK_TIMEOUT_US   500000ULL
#define HOST_LINK_PING_TIMEOUT_US        200000ULL
#define HOST_LINK_STATUS_TIMEOUT_US      200000ULL
#define HOST_LINK_FW_VERSION             0x20260422UL

typedef enum {
  HOST_LINK_STATE_UART_READY = 0,
  HOST_LINK_STATE_WAIT_HELLO_ACK,
  HOST_LINK_STATE_READY,
} host_link_state_t;

typedef struct {
  bool initialized;
  host_link_state_t state;
  host_link_event_handlers_t event_handlers;
  host_proto_parser_t parser;
  uint8_t next_tx_seq;
  uint64_t hello_deadline_us;
  bool request_active;
  uint8_t expected_type;
  uint8_t expected_seq;
  uint64_t request_deadline_us;
  bool request_done;
  bool request_ok;
  uint8_t response_payload[HOST_PROTO_MAX_PAYLOAD_LEN];
  uint16_t response_length;
} host_link_context_t;

static host_link_context_t host_link_ctx;

static void host_link_send_hello(void);
static void host_link_handle_frame(const host_frame_t *frame);
static bool host_link_handle_event_frame(const host_frame_t *frame);
static bool host_link_send_frame(uint8_t type,
                                 uint8_t flags,
                                 uint8_t seq,
                                 const void *payload,
                                 uint16_t payload_length);
static void host_link_restart_handshake(const char *reason);
static bool host_link_send_command(uint8_t type,
                                   const void *payload,
                                   uint16_t payload_length);

static bool host_link_send_frame(uint8_t type,
                                 uint8_t flags,
                                 uint8_t seq,
                                 const void *payload,
                                 uint16_t payload_length)
{
  uint8_t encoded[HOST_PROTO_MAX_FRAME_LEN];
  size_t encoded_length;

  encoded_length = host_proto_encode_frame(type,
                                           flags,
                                           seq,
                                           payload,
                                           payload_length,
                                           encoded,
                                           sizeof(encoded));
  if (encoded_length == 0U) {
    return false;
  }

  host_uart_write(encoded, encoded_length);
  printf("HOST_TX type=%s seq=%u len=%u\r\n",
         host_proto_type_name(type),
         seq,
         payload_length);
  return true;
}

static void host_link_send_hello(void)
{
  host_hello_v1_t hello = {
    .endpoint = HOST_ENDPOINT_MCU,
    .proto_min = HOST_PROTO_VERSION,
    .proto_max = HOST_PROTO_VERSION,
    .capabilities = HOST_CAP_STATUS_SUPPORTED,
    .fw_version = HOST_LINK_FW_VERSION,
  };
  uint8_t seq = host_link_ctx.next_tx_seq++;

  host_link_send_frame(PYRONET_HOST_MSG_HELLO, 0U, seq, &hello, sizeof(hello));
  host_link_ctx.state = HOST_LINK_STATE_WAIT_HELLO_ACK;
  host_link_ctx.hello_deadline_us = monotonic_time_now_us() + HOST_LINK_HELLO_ACK_TIMEOUT_US;
}

static void host_link_restart_handshake(const char *reason)
{
  if ((reason != NULL) && (host_link_ctx.state == HOST_LINK_STATE_READY)) {
    printf("HOST_LINK_RESET reason=%s\r\n", reason);
  }

  host_link_ctx.state = HOST_LINK_STATE_UART_READY;
  host_link_ctx.request_active = false;
  host_link_ctx.request_done = false;
  host_link_ctx.request_ok = false;
  host_link_ctx.response_length = 0U;
  host_link_send_hello();
}

static void host_link_complete_request(const host_frame_t *frame, bool ok)
{
  host_link_ctx.request_active = false;
  host_link_ctx.request_done = true;
  host_link_ctx.request_ok = ok;
  host_link_ctx.response_length = frame->payload_length;
  if (frame->payload_length > 0U) {
    memcpy(host_link_ctx.response_payload, frame->payload, frame->payload_length);
  }
}

static bool host_link_handle_event_frame(const host_frame_t *frame)
{
  switch (frame->type) {
    case PYRONET_HOST_MSG_REGISTRATION_NEEDED:
      if (frame->payload_length == sizeof(pyronet_host_registration_needed_v1_t)) {
        pyronet_host_registration_needed_v1_t event;

        memcpy(&event, frame->payload, sizeof(event));
        if (host_link_ctx.event_handlers.on_registration_needed != NULL) {
          host_link_ctx.event_handlers.on_registration_needed(
            host_link_ctx.event_handlers.context,
            &event);
        }
      }
      return true;

    case PYRONET_HOST_MSG_PARENT_CHANGED:
      if (frame->payload_length == sizeof(pyronet_host_parent_changed_v1_t)) {
        pyronet_host_parent_changed_v1_t event;

        memcpy(&event, frame->payload, sizeof(event));
        if (host_link_ctx.event_handlers.on_parent_changed != NULL) {
          host_link_ctx.event_handlers.on_parent_changed(
            host_link_ctx.event_handlers.context,
            &event);
        }
      }
      return true;

    case PYRONET_HOST_MSG_TX_RESULT:
      if (frame->payload_length == sizeof(pyronet_host_tx_result_v1_t)) {
        pyronet_host_tx_result_v1_t event;

        memcpy(&event, frame->payload, sizeof(event));
        if (host_link_ctx.event_handlers.on_tx_result != NULL) {
          host_link_ctx.event_handlers.on_tx_result(
            host_link_ctx.event_handlers.context,
            &event);
        }
      }
      return true;

    case PYRONET_HOST_MSG_TIME_SYNC_UPDATE:
      if (frame->payload_length == sizeof(pyronet_host_time_sync_update_v1_t)) {
        pyronet_host_time_sync_update_v1_t event;

        memcpy(&event, frame->payload, sizeof(event));
        if (host_link_ctx.event_handlers.on_time_sync_update != NULL) {
          host_link_ctx.event_handlers.on_time_sync_update(
            host_link_ctx.event_handlers.context,
            &event);
        }
      }
      return true;

    case PYRONET_HOST_MSG_NEIGHBOR_ALERT_RX:
      if (frame->payload_length == sizeof(pyronet_host_neighbor_alert_received_v1_t)) {
        pyronet_host_neighbor_alert_received_v1_t event;

        memcpy(&event, frame->payload, sizeof(event));
        if (host_link_ctx.event_handlers.on_neighbor_alert_rx != NULL) {
          host_link_ctx.event_handlers.on_neighbor_alert_rx(
            host_link_ctx.event_handlers.context,
            &event);
        }
      }
      return true;

    case PYRONET_HOST_MSG_CONFIG_UPDATE_RX:
      if (frame->payload_length == sizeof(pyronet_host_config_update_received_v1_t)) {
        pyronet_host_config_update_received_v1_t event;

        memcpy(&event, frame->payload, sizeof(event));
        if (host_link_ctx.event_handlers.on_config_update_rx != NULL) {
          host_link_ctx.event_handlers.on_config_update_rx(
            host_link_ctx.event_handlers.context,
            &event);
        }
      }
      return true;

    default:
      return false;
  }
}

static void host_link_handle_frame(const host_frame_t *frame)
{
  printf("HOST_RX type=%s seq=%u len=%u\r\n",
         host_proto_type_name(frame->type),
         frame->seq,
         frame->payload_length);

  if (frame->type == PYRONET_HOST_MSG_HELLO_ACK) {
    host_hello_ack_v1_t hello_ack;

    if (((frame->flags & HOST_FRAME_FLAG_RESPONSE) == 0U)
        || (frame->payload_length != sizeof(hello_ack))) {
      return;
    }

    memcpy(&hello_ack, frame->payload, sizeof(hello_ack));
    if ((hello_ack.endpoint != HOST_ENDPOINT_NCP)
        || (hello_ack.proto_selected != HOST_PROTO_VERSION)) {
      return;
    }

    if (host_link_ctx.state != HOST_LINK_STATE_READY) {
      host_link_ctx.state = HOST_LINK_STATE_READY;
      host_link_ctx.request_active = false;
      host_link_ctx.request_done = false;
      host_link_ctx.request_ok = false;
      printf("HOST_LINK_READY\r\n");
    }
    return;
  }

  if (host_link_handle_event_frame(frame)) {
    return;
  }

  if (host_link_ctx.state != HOST_LINK_STATE_READY) {
    return;
  }

  if ((!host_link_ctx.request_active)
      || (frame->seq != host_link_ctx.expected_seq)) {
    return;
  }

  if (frame->type == PYRONET_HOST_MSG_ERROR) {
    host_error_v1_t error;

    if (frame->payload_length == sizeof(error)) {
      memcpy(&error, frame->payload, sizeof(error));
      printf("HOST_ERROR failed_type=%s code=%u\r\n",
             host_proto_type_name(error.failed_type),
             error.error_code);
    }
    host_link_complete_request(frame, false);
    return;
  }

  if (((frame->flags & HOST_FRAME_FLAG_RESPONSE) == 0U)
      || (frame->type != host_link_ctx.expected_type)) {
    return;
  }

  host_link_complete_request(frame, true);
}

static bool host_link_send_command(uint8_t type,
                                   const void *payload,
                                   uint16_t payload_length)
{
  uint8_t seq;

  if ((host_link_ctx.state != HOST_LINK_STATE_READY)
      || ((payload_length > 0U) && (payload == NULL))) {
    return false;
  }

  seq = host_link_ctx.next_tx_seq++;
  return host_link_send_frame(type, 0U, seq, payload, payload_length);
}

static bool host_link_wait_for_response(uint8_t request_type,
                                        uint8_t expected_type,
                                        const void *payload,
                                        uint16_t payload_length,
                                        uint64_t timeout_us)
{
  uint8_t seq;
  uint64_t now_us;

  if ((host_link_ctx.state != HOST_LINK_STATE_READY) || host_link_ctx.request_active) {
    return false;
  }

  seq = host_link_ctx.next_tx_seq++;
  host_link_ctx.request_active = true;
  host_link_ctx.expected_type = expected_type;
  host_link_ctx.expected_seq = seq;
  host_link_ctx.request_done = false;
  host_link_ctx.request_ok = false;
  host_link_ctx.response_length = 0U;

  if (!host_link_send_frame(request_type, 0U, seq, payload, payload_length)) {
    host_link_ctx.request_active = false;
    return false;
  }

  host_link_ctx.request_deadline_us = monotonic_time_now_us() + timeout_us;

  while (true) {
    host_link_poll();

    if (host_link_ctx.request_done) {
      return host_link_ctx.request_ok;
    }

    now_us = monotonic_time_now_us();
    if (now_us >= host_link_ctx.request_deadline_us) {
      break;
    }
  }

  host_link_ctx.request_active = false;
  host_link_ctx.request_done = false;
  host_link_ctx.request_ok = false;
  host_link_ctx.response_length = 0U;
  return false;
}

bool host_link_init(const host_link_event_handlers_t *event_handlers)
{
  memset(&host_link_ctx, 0, sizeof(host_link_ctx));
  host_proto_parser_init(&host_link_ctx.parser);
  if (event_handlers != NULL) {
    host_link_ctx.event_handlers = *event_handlers;
  }

  if (!host_uart_init()) {
    return false;
  }

  host_link_ctx.initialized = true;
  host_link_ctx.state = HOST_LINK_STATE_UART_READY;
  printf("HOST_UART_READY baud=%u\r\n", HOST_UART_BAUDRATE);
  host_link_send_hello();
  return true;
}

void host_link_poll(void)
{
  uint8_t rx_byte;
  host_frame_t frame;

  if (!host_link_ctx.initialized) {
    return;
  }

  while (host_uart_try_read_byte(&rx_byte)) {
    if (host_proto_parser_consume(&host_link_ctx.parser, rx_byte, &frame)) {
      host_link_handle_frame(&frame);
    }
  }

  if ((host_link_ctx.state != HOST_LINK_STATE_READY)
      && (monotonic_time_now_us() >= host_link_ctx.hello_deadline_us)) {
    host_link_send_hello();
  }
}

bool host_link_is_ready(void)
{
  return host_link_ctx.state == HOST_LINK_STATE_READY;
}

bool host_link_ping(uint32_t token)
{
  host_ping_v1_t ping = { .token = token };
  host_pong_v1_t pong;

  if (!host_link_wait_for_response(PYRONET_HOST_MSG_PING,
                                   PYRONET_HOST_MSG_PONG,
                                   &ping,
                                   sizeof(ping),
                                   HOST_LINK_PING_TIMEOUT_US)) {
    printf("HOST_PING_TIMEOUT token=0x%08lx\r\n", (unsigned long)token);
    host_link_restart_handshake("ping_timeout");
    return false;
  }

  if (host_link_ctx.response_length != sizeof(pong)) {
    printf("HOST_PING_BAD_LENGTH len=%u\r\n", host_link_ctx.response_length);
    host_link_restart_handshake("ping_bad_length");
    return false;
  }

  memcpy(&pong, host_link_ctx.response_payload, sizeof(pong));
  if (pong.token != token) {
    printf("HOST_PING_MISMATCH expect=0x%08lx got=0x%08lx\r\n",
           (unsigned long)token,
           (unsigned long)pong.token);
    host_link_restart_handshake("ping_mismatch");
    return false;
  }

  return true;
}

bool host_link_get_status(host_status_v1_t *out_status)
{
  if ((out_status == NULL) || !host_link_is_ready()) {
    return false;
  }

  if (!host_link_wait_for_response(PYRONET_HOST_MSG_GET_STATUS,
                                   PYRONET_HOST_MSG_STATUS,
                                   NULL,
                                   0U,
                                   HOST_LINK_STATUS_TIMEOUT_US)) {
    printf("HOST_STATUS_TIMEOUT\r\n");
    return false;
  }

  if (host_link_ctx.response_length != sizeof(*out_status)) {
    printf("HOST_STATUS_BAD_LENGTH len=%u\r\n", host_link_ctx.response_length);
    return false;
  }

  memcpy(out_status, host_link_ctx.response_payload, sizeof(*out_status));
  return true;
}

bool host_link_send_registration(
  const pyronet_host_send_registration_v1_t *payload)
{
  return (payload != NULL)
           ? host_link_send_command(PYRONET_HOST_MSG_SEND_REGISTRATION,
                                    payload,
                                    sizeof(*payload))
           : false;
}

bool host_link_send_sensor_report(
  const pyronet_host_send_sensor_report_v1_t *payload)
{
  return (payload != NULL)
           ? host_link_send_command(PYRONET_HOST_MSG_SEND_SENSOR_REPORT,
                                    payload,
                                    sizeof(*payload))
           : false;
}

bool host_link_send_sensor_alert(
  const pyronet_host_send_sensor_alert_v1_t *payload)
{
  return (payload != NULL)
           ? host_link_send_command(PYRONET_HOST_MSG_SEND_SENSOR_ALERT,
                                    payload,
                                    sizeof(*payload))
           : false;
}

bool host_link_send_neighbor_alert(
  const pyronet_host_send_neighbor_alert_v1_t *payload)
{
  return (payload != NULL)
           ? host_link_send_command(PYRONET_HOST_MSG_SEND_NEIGHBOR_ALERT,
                                    payload,
                                    sizeof(*payload))
           : false;
}

bool host_link_request_parent_update(
  const pyronet_host_request_parent_update_v1_t *payload)
{
  return (payload != NULL)
           ? host_link_send_command(PYRONET_HOST_MSG_REQUEST_PARENT_UPDATE,
                                    payload,
                                    sizeof(*payload))
           : false;
}
