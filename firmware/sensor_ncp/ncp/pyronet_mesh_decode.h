#ifndef NCP_PYRONET_MESH_DECODE_H
#define NCP_PYRONET_MESH_DECODE_H

#include <stdbool.h>
#include <stdint.h>

#include "../host/host_proto.h"
#include "pyronet_ncp_local.h"

typedef struct PYRONET_PACKED
{
    uint8_t type;
    uint8_t version;
    uint16_t target_node_id;
    uint8_t neighbor_count;
} pyronet_mesh_nn_table_update_header_v1_t;

typedef struct PYRONET_PACKED
{
    uint8_t type;
    uint8_t version;
    uint32_t unix_time_s;
} pyronet_mesh_time_sync_v1_t;

typedef struct PYRONET_PACKED
{
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

typedef struct pyronet_mesh_nn_table_update_view
{
    pyronet_mesh_nn_table_update_header_v1_t header;
    const uint8_t *neighbors;
} pyronet_mesh_nn_table_update_view_t;

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

#endif /* NCP_PYRONET_MESH_DECODE_H */
