#include "transport/host_proto.h"

#include <string.h>

enum {
  HOST_PARSER_STATE_WAIT_SOF0 = 0,
  HOST_PARSER_STATE_WAIT_SOF1,
  HOST_PARSER_STATE_READ_VER,
  HOST_PARSER_STATE_READ_TYPE,
  HOST_PARSER_STATE_READ_FLAGS,
  HOST_PARSER_STATE_READ_SEQ,
  HOST_PARSER_STATE_READ_LEN_L,
  HOST_PARSER_STATE_READ_LEN_H,
  HOST_PARSER_STATE_READ_PAYLOAD,
  HOST_PARSER_STATE_READ_CRC_L,
  HOST_PARSER_STATE_READ_CRC_H,
};

static uint16_t host_crc16_update(uint16_t crc, uint8_t byte)
{
  uint8_t bit_index;

  crc ^= (uint16_t)byte << 8;
  for (bit_index = 0U; bit_index < 8U; bit_index++) {
    if ((crc & 0x8000U) != 0U) {
      crc = (uint16_t)((crc << 1) ^ 0x1021U);
    } else {
      crc <<= 1;
    }
  }

  return crc;
}

static void host_proto_parser_reset(host_proto_parser_t *parser)
{
  parser->state = HOST_PARSER_STATE_WAIT_SOF0;
  parser->type = 0U;
  parser->flags = 0U;
  parser->seq = 0U;
  parser->payload_length = 0U;
  parser->payload_index = 0U;
  parser->crc = 0xFFFFU;
  parser->received_crc = 0U;
}

static void host_proto_parser_resync(host_proto_parser_t *parser, uint8_t byte)
{
  host_proto_parser_reset(parser);
  if (byte == HOST_FRAME_SOF0) {
    parser->state = HOST_PARSER_STATE_WAIT_SOF1;
  }
}

uint16_t host_crc16_ccitt_false(const uint8_t *data, size_t length)
{
  uint16_t crc = 0xFFFFU;
  size_t index;

  for (index = 0U; index < length; index++) {
    crc = host_crc16_update(crc, data[index]);
  }

  return crc;
}

size_t host_proto_encode_frame(uint8_t type,
                               uint8_t flags,
                               uint8_t seq,
                               const void *payload,
                               uint16_t payload_length,
                               uint8_t *out_frame,
                               size_t out_frame_size)
{
  const uint8_t *payload_bytes = (const uint8_t *)payload;
  uint16_t crc;
  size_t frame_length = 10U + payload_length;

  if ((payload_length > HOST_PROTO_MAX_PAYLOAD_LEN)
      || (out_frame == NULL)
      || (out_frame_size < frame_length)
      || ((payload_length > 0U) && (payload == NULL))) {
    return 0U;
  }

  out_frame[0] = HOST_FRAME_SOF0;
  out_frame[1] = HOST_FRAME_SOF1;
  out_frame[2] = HOST_PROTO_VERSION;
  out_frame[3] = type;
  out_frame[4] = flags;
  out_frame[5] = seq;
  out_frame[6] = (uint8_t)(payload_length & 0xFFU);
  out_frame[7] = (uint8_t)(payload_length >> 8);

  if (payload_length > 0U) {
    memcpy(&out_frame[8], payload_bytes, payload_length);
  }

  crc = host_crc16_ccitt_false(&out_frame[2], 6U + payload_length);
  out_frame[8U + payload_length] = (uint8_t)(crc & 0xFFU);
  out_frame[9U + payload_length] = (uint8_t)(crc >> 8);

  return frame_length;
}

void host_proto_parser_init(host_proto_parser_t *parser)
{
  memset(parser, 0, sizeof(*parser));
  host_proto_parser_reset(parser);
}

bool host_proto_parser_consume(host_proto_parser_t *parser,
                               uint8_t byte,
                               host_frame_t *out_frame)
{
  switch (parser->state) {
    case HOST_PARSER_STATE_WAIT_SOF0:
      if (byte == HOST_FRAME_SOF0) {
        parser->state = HOST_PARSER_STATE_WAIT_SOF1;
      }
      break;

    case HOST_PARSER_STATE_WAIT_SOF1:
      if (byte == HOST_FRAME_SOF1) {
        parser->state = HOST_PARSER_STATE_READ_VER;
        parser->crc = 0xFFFFU;
      } else if (byte != HOST_FRAME_SOF0) {
        parser->state = HOST_PARSER_STATE_WAIT_SOF0;
      }
      break;

    case HOST_PARSER_STATE_READ_VER:
      if (byte != HOST_PROTO_VERSION) {
        host_proto_parser_resync(parser, byte);
        break;
      }

      parser->crc = host_crc16_update(parser->crc, byte);
      parser->state = HOST_PARSER_STATE_READ_TYPE;
      break;

    case HOST_PARSER_STATE_READ_TYPE:
      parser->type = byte;
      parser->crc = host_crc16_update(parser->crc, byte);
      parser->state = HOST_PARSER_STATE_READ_FLAGS;
      break;

    case HOST_PARSER_STATE_READ_FLAGS:
      parser->flags = byte;
      parser->crc = host_crc16_update(parser->crc, byte);
      parser->state = HOST_PARSER_STATE_READ_SEQ;
      break;

    case HOST_PARSER_STATE_READ_SEQ:
      parser->seq = byte;
      parser->crc = host_crc16_update(parser->crc, byte);
      parser->state = HOST_PARSER_STATE_READ_LEN_L;
      break;

    case HOST_PARSER_STATE_READ_LEN_L:
      parser->payload_length = byte;
      parser->crc = host_crc16_update(parser->crc, byte);
      parser->state = HOST_PARSER_STATE_READ_LEN_H;
      break;

    case HOST_PARSER_STATE_READ_LEN_H:
      parser->payload_length |= (uint16_t)byte << 8;
      parser->crc = host_crc16_update(parser->crc, byte);
      if (parser->payload_length > HOST_PROTO_MAX_PAYLOAD_LEN) {
        host_proto_parser_resync(parser, byte);
      } else if (parser->payload_length == 0U) {
        parser->state = HOST_PARSER_STATE_READ_CRC_L;
      } else {
        parser->payload_index = 0U;
        parser->state = HOST_PARSER_STATE_READ_PAYLOAD;
      }
      break;

    case HOST_PARSER_STATE_READ_PAYLOAD:
      parser->payload[parser->payload_index++] = byte;
      parser->crc = host_crc16_update(parser->crc, byte);
      if (parser->payload_index >= parser->payload_length) {
        parser->state = HOST_PARSER_STATE_READ_CRC_L;
      }
      break;

    case HOST_PARSER_STATE_READ_CRC_L:
      parser->received_crc = byte;
      parser->state = HOST_PARSER_STATE_READ_CRC_H;
      break;

    case HOST_PARSER_STATE_READ_CRC_H:
      parser->received_crc |= (uint16_t)byte << 8;
      if (parser->received_crc == parser->crc) {
        out_frame->type = parser->type;
        out_frame->flags = parser->flags;
        out_frame->seq = parser->seq;
        out_frame->payload_length = parser->payload_length;
        if (parser->payload_length > 0U) {
          memcpy(out_frame->payload, parser->payload, parser->payload_length);
        }
        host_proto_parser_reset(parser);
        return true;
      }

      host_proto_parser_resync(parser, byte);
      break;

    default:
      host_proto_parser_reset(parser);
      break;
  }

  return false;
}

const char *host_proto_type_name(uint8_t type)
{
  switch (type) {
    case HOST_MSG_HELLO:
      return "HELLO";
    case HOST_MSG_HELLO_ACK:
      return "HELLO_ACK";
    case HOST_MSG_PING:
      return "PING";
    case HOST_MSG_PONG:
      return "PONG";
    case HOST_MSG_GET_STATUS:
      return "GET_STATUS";
    case HOST_MSG_STATUS:
      return "STATUS";
    case HOST_MSG_ERROR:
      return "ERROR";
    default:
      return "UNKNOWN";
  }
}
