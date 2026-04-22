from __future__ import annotations

import ipaddress
import struct

from .downlink_request_validation import ConfigUpdateRequest

NN_TABLE_HEADER = struct.Struct("<BBHB")
TIME_SYNC = struct.Struct("<BBI")
CONFIG_UPDATE = struct.Struct("<BBIhHHhHHHHH")


def encode_nn_table_update(*, version: int, target_node_id: int, neighbor_ipv6s: list[str]) -> bytes:
    header = NN_TABLE_HEADER.pack(0x04, version, target_node_id, len(neighbor_ipv6s))
    addresses = b"".join(ipaddress.IPv6Address(ipv6).packed for ipv6 in neighbor_ipv6s)
    return header + addresses


def encode_time_sync(*, version: int, epoch: int) -> bytes:
    return TIME_SYNC.pack(0x05, version, epoch)


def encode_config_update(*, version: int, request: ConfigUpdateRequest) -> bytes:
    return CONFIG_UPDATE.pack(
        0x06,
        version,
        request.config_id,
        request.l2_temp_thresh,
        request.l2_humidity_thresh,
        request.l2_voc_thresh,
        request.l3_temp_thresh,
        request.l3_humidity_thresh,
        request.l3_voc_thresh,
        request.l4_voc_thresh,
        request.l5_voc_thresh,
        request.l5_pm25_thresh,
    )
