#include "pyronet_mesh_decode.h"

#include <string.h>

typedef struct PYRONET_PACKED
{
    uint8_t type;
    uint8_t version;
} pyronet_mesh_header_v1_t;

static bool pyronetMeshDecodeFixedPacket(const uint8_t *payload,
                                         uint16_t payload_len,
                                         uint8_t expected_type,
                                         void *out_packet,
                                         uint16_t expected_len)
{
    pyronet_mesh_header_v1_t header;

    if ((payload == NULL) || (out_packet == NULL) || (payload_len != expected_len))
    {
        return false;
    }

    memcpy(&header, payload, sizeof(header));
    if (header.type != expected_type)
    {
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

    if ((payload == NULL) || (out_view == NULL) || (payload_len < sizeof(header)))
    {
        return false;
    }

    memcpy(&header, payload, sizeof(header));
    if (header.type != PYRONET_PKT_NN_TABLE_UPDATE)
    {
        return false;
    }

    if (header.neighbor_count > PYRONET_MAX_NEIGHBORS)
    {
        return false;
    }

    expected_len = (uint16_t)(sizeof(header) + ((uint16_t)header.neighbor_count * PYRONET_IPV6_ADDR_LEN));
    if (payload_len != expected_len)
    {
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
    return pyronetMeshDecodeFixedPacket(payload,
                                        payload_len,
                                        PYRONET_PKT_TIME_SYNC,
                                        out_packet,
                                        sizeof(*out_packet));
}

bool pyronet_mesh_decode_config_update(const uint8_t *payload,
                                       uint16_t payload_len,
                                       pyronet_mesh_config_update_v1_t *out_packet)
{
    return pyronetMeshDecodeFixedPacket(payload,
                                        payload_len,
                                        PYRONET_PKT_CONFIG_UPDATE,
                                        out_packet,
                                        sizeof(*out_packet));
}

bool pyronet_mesh_decode_neighbor_alert(const uint8_t *payload,
                                        uint16_t payload_len,
                                        pyronet_mesh_neighbor_alert_v1_t *out_packet)
{
    return pyronetMeshDecodeFixedPacket(payload,
                                        payload_len,
                                        PYRONET_PKT_NEIGHBOR_ALERT,
                                        out_packet,
                                        sizeof(*out_packet));
}
