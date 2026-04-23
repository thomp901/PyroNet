from __future__ import annotations

import ipaddress
import struct
from dataclasses import dataclass

TYPE_NN_TABLE_UPDATE = 0x04
TYPE_TIME_SYNC = 0x05
TYPE_CONFIG_UPDATE = 0x06

NN_TABLE_HEADER = struct.Struct("<BBHB")
TIME_SYNC = struct.Struct("<BBI")
CONFIG_UPDATE = struct.Struct("<BBIhHHhHHHHH")


@dataclass(frozen=True)
class NodeDownlinkPayloadInfo:
    payload_type: int
    version: int
    target_node_id: int | None = None


@dataclass(frozen=True)
class ConfigUpdate:
    config_id: int
    l2_temp_thresh: int
    l2_humidity_thresh: int
    l2_voc_thresh: int
    l3_temp_thresh: int
    l3_humidity_thresh: int
    l3_voc_thresh: int
    l4_voc_thresh: int
    l5_voc_thresh: int
    l5_pm25_thresh: int


def encode_nn_table_update(*, version: int, target_node_id: int, neighbor_ipv6s: list[str]) -> bytes:
    header = NN_TABLE_HEADER.pack(TYPE_NN_TABLE_UPDATE, version, target_node_id, len(neighbor_ipv6s))
    addresses = b"".join(ipaddress.IPv6Address(ipv6).packed for ipv6 in neighbor_ipv6s)
    return header + addresses


def encode_time_sync(*, version: int, epoch: int) -> bytes:
    return TIME_SYNC.pack(TYPE_TIME_SYNC, version, epoch)


def encode_config_update(*, version: int, request: ConfigUpdate) -> bytes:
    return CONFIG_UPDATE.pack(
        TYPE_CONFIG_UPDATE,
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


def validate_node_downlink_payload(payload: bytes) -> NodeDownlinkPayloadInfo:
    if not payload:
        raise ValueError("downlink payload is empty")

    payload_type = payload[0]
    if payload_type == TYPE_NN_TABLE_UPDATE:
        if len(payload) < NN_TABLE_HEADER.size:
            raise ValueError("NN table payload shorter than header")
        decoded_type, version, target_node_id, nn_count = NN_TABLE_HEADER.unpack(payload[: NN_TABLE_HEADER.size])
        expected_len = NN_TABLE_HEADER.size + (16 * nn_count)
        if len(payload) != expected_len:
            raise ValueError(f"NN table payload length {len(payload)} != {expected_len}")
        return NodeDownlinkPayloadInfo(payload_type=decoded_type, version=version, target_node_id=target_node_id)

    if payload_type == TYPE_TIME_SYNC:
        if len(payload) != TIME_SYNC.size:
            raise ValueError(f"time sync payload length {len(payload)} != {TIME_SYNC.size}")
        decoded_type, version, _epoch = TIME_SYNC.unpack(payload)
        return NodeDownlinkPayloadInfo(payload_type=decoded_type, version=version)

    if payload_type == TYPE_CONFIG_UPDATE:
        if len(payload) != CONFIG_UPDATE.size:
            raise ValueError(f"config update payload length {len(payload)} != {CONFIG_UPDATE.size}")
        decoded_type, version, *_rest = CONFIG_UPDATE.unpack(payload)
        return NodeDownlinkPayloadInfo(payload_type=decoded_type, version=version)

    raise ValueError(f"unsupported downlink payload type 0x{payload_type:02x}")
