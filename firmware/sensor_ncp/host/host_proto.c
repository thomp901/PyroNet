#include "host_proto.h"

#include <string.h>

static void hostProtoDiscard(struct host_proto_parser *parser, size_t count)
{
    if (count >= parser->length)
    {
        parser->length = 0U;
        return;
    }

    memmove(parser->buffer, &parser->buffer[count], parser->length - count);
    parser->length -= count;
}

static void hostProtoSeekNextSof(struct host_proto_parser *parser)
{
    size_t index;

    for (index = 0; (index + 1U) < parser->length; ++index)
    {
        if ((parser->buffer[index] == HOST_PROTO_SOF0) && (parser->buffer[index + 1U] == HOST_PROTO_SOF1))
        {
            hostProtoDiscard(parser, index);
            return;
        }
    }

    if ((parser->length > 0U) && (parser->buffer[parser->length - 1U] == HOST_PROTO_SOF0))
    {
        parser->buffer[0] = HOST_PROTO_SOF0;
        parser->length = 1U;
    }
    else
    {
        parser->length = 0U;
    }
}

uint16_t host_proto_crc16_ccitt_false(const uint8_t *data, size_t length)
{
    size_t index;
    uint16_t crc = 0xFFFFU;

    if ((data == NULL) && (length > 0U))
    {
        return crc;
    }

    for (index = 0; index < length; ++index)
    {
        uint8_t bit;

        crc ^= (uint16_t)data[index] << 8;
        for (bit = 0; bit < 8U; ++bit)
        {
            if ((crc & 0x8000U) != 0U)
            {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}

bool host_proto_encode_frame(uint8_t type,
                             uint8_t flags,
                             uint8_t seq,
                             const void *payload,
                             uint16_t payload_len,
                             uint8_t *out_frame,
                             size_t out_frame_size,
                             size_t *out_frame_len)
{
    uint16_t crc;
    size_t frame_len;

    if ((payload_len > HOST_PROTO_MAX_PAYLOAD_LEN) || (out_frame == NULL))
    {
        return false;
    }

    if ((payload_len > 0U) && (payload == NULL))
    {
        return false;
    }

    frame_len = (size_t)HOST_PROTO_FRAME_MIN_LEN + payload_len;
    if (out_frame_size < frame_len)
    {
        return false;
    }

    out_frame[0] = HOST_PROTO_SOF0;
    out_frame[1] = HOST_PROTO_SOF1;
    out_frame[2] = HOST_PROTO_VERSION;
    out_frame[3] = type;
    out_frame[4] = flags;
    out_frame[5] = seq;
    out_frame[6] = (uint8_t)(payload_len & 0xFFU);
    out_frame[7] = (uint8_t)((payload_len >> 8) & 0xFFU);

    if (payload_len > 0U)
    {
        memcpy(&out_frame[8], payload, payload_len);
    }

    crc = host_proto_crc16_ccitt_false(&out_frame[2], 6U + payload_len);
    out_frame[8U + payload_len] = (uint8_t)(crc & 0xFFU);
    out_frame[9U + payload_len] = (uint8_t)((crc >> 8) & 0xFFU);

    if (out_frame_len != NULL)
    {
        *out_frame_len = frame_len;
    }

    return true;
}

void host_proto_parser_init(struct host_proto_parser *parser)
{
    if (parser == NULL)
    {
        return;
    }

    parser->length = 0U;
}

bool host_proto_parser_push(struct host_proto_parser *parser, uint8_t byte, struct host_frame *out_frame)
{
    if ((parser == NULL) || (out_frame == NULL))
    {
        return false;
    }

    if (parser->length >= sizeof(parser->buffer))
    {
        hostProtoSeekNextSof(parser);
    }

    if (parser->length >= sizeof(parser->buffer))
    {
        parser->length = 0U;
    }

    parser->buffer[parser->length++] = byte;

    for (;;)
    {
        uint16_t payload_len;
        size_t frame_len;
        uint16_t expected_crc;
        uint16_t actual_crc;

        if (parser->length < 2U)
        {
            return false;
        }

        if ((parser->buffer[0] != HOST_PROTO_SOF0) || (parser->buffer[1] != HOST_PROTO_SOF1))
        {
            hostProtoSeekNextSof(parser);
            if (parser->length < 2U)
            {
                return false;
            }
        }

        if (parser->length < HOST_PROTO_FRAME_MIN_LEN)
        {
            return false;
        }

        if (parser->buffer[2] != HOST_PROTO_VERSION)
        {
            hostProtoDiscard(parser, 1U);
            continue;
        }

        payload_len = (uint16_t)parser->buffer[6] | ((uint16_t)parser->buffer[7] << 8);
        if (payload_len > HOST_PROTO_MAX_PAYLOAD_LEN)
        {
            hostProtoDiscard(parser, 1U);
            continue;
        }

        frame_len = (size_t)HOST_PROTO_FRAME_MIN_LEN + payload_len;
        if (parser->length < frame_len)
        {
            return false;
        }

        expected_crc = (uint16_t)parser->buffer[8U + payload_len] | ((uint16_t)parser->buffer[9U + payload_len] << 8);
        actual_crc = host_proto_crc16_ccitt_false(&parser->buffer[2], 6U + payload_len);
        if (expected_crc != actual_crc)
        {
            hostProtoDiscard(parser, 1U);
            continue;
        }

        out_frame->type = parser->buffer[3];
        out_frame->flags = parser->buffer[4];
        out_frame->seq = parser->buffer[5];
        out_frame->payload_len = payload_len;

        if (payload_len > 0U)
        {
            memcpy(out_frame->payload, &parser->buffer[8], payload_len);
        }

        hostProtoDiscard(parser, frame_len);
        return true;
    }
}

const char *host_proto_type_name(uint8_t type)
{
    switch (type)
    {
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
