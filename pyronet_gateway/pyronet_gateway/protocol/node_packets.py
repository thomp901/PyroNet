from __future__ import annotations

import ipaddress
import struct
from dataclasses import dataclass

TYPE_REGISTRATION = 0x01
TYPE_SENSOR_REPORT = 0x02
TYPE_SENSOR_ALERT = 0x03
TYPE_NEIGHBOR_ALERT = 0x07
TYPE_PARENT_UPDATE = 0x08

_REGISTRATION = struct.Struct("<BBHffHB16s")
_REPORT = struct.Struct("<BBHIBhHHHB")
_PARENT_UPDATE = struct.Struct("<BBHI16s")


class PacketParseError(ValueError):
    """Raised when a packed node packet is invalid."""


@dataclass(frozen=True)
class NodePacket:
    packet_type: int
    version: int
    node_id: int


@dataclass(frozen=True)
class RegistrationPacket(NodePacket):
    latitude: float
    longitude: float
    fw_version: int
    battery_pct: int
    parent_ipv6: str | None


@dataclass(frozen=True)
class SensorPacket(NodePacket):
    timestamp: int
    risk_level: int
    temperature: int
    humidity: int
    voc_iaq: int
    pm25: int
    battery_pct: int


@dataclass(frozen=True)
class ParentUpdatePacket(NodePacket):
    timestamp: int
    parent_ipv6: str | None


def packet_type(raw_packet: bytes) -> int:
    if len(raw_packet) < 1:
        raise PacketParseError("packet is empty")
    return raw_packet[0]
def parse_node_packet(raw_packet: bytes) -> NodePacket:
    msg_type = packet_type(raw_packet)

    if msg_type == TYPE_REGISTRATION:
        if len(raw_packet) != _REGISTRATION.size:
            raise PacketParseError(f"registration length {len(raw_packet)} != {_REGISTRATION.size}")
        unpacked = _REGISTRATION.unpack(raw_packet)
        parent = _decode_optional_ipv6(unpacked[7])
        return RegistrationPacket(
            packet_type=unpacked[0],
            version=unpacked[1],
            node_id=unpacked[2],
            latitude=unpacked[3],
            longitude=unpacked[4],
            fw_version=unpacked[5],
            battery_pct=unpacked[6],
            parent_ipv6=parent,
        )

    if msg_type in {TYPE_SENSOR_REPORT, TYPE_SENSOR_ALERT}:
        if len(raw_packet) != _REPORT.size:
            raise PacketParseError(f"sensor packet length {len(raw_packet)} != {_REPORT.size}")
        unpacked = _REPORT.unpack(raw_packet)
        return SensorPacket(
            packet_type=unpacked[0],
            version=unpacked[1],
            node_id=unpacked[2],
            timestamp=unpacked[3],
            risk_level=unpacked[4],
            temperature=unpacked[5],
            humidity=unpacked[6],
            voc_iaq=unpacked[7],
            pm25=unpacked[8],
            battery_pct=unpacked[9],
        )

    if msg_type == TYPE_PARENT_UPDATE:
        if len(raw_packet) != _PARENT_UPDATE.size:
            raise PacketParseError(f"parent update length {len(raw_packet)} != {_PARENT_UPDATE.size}")
        unpacked = _PARENT_UPDATE.unpack(raw_packet)
        return ParentUpdatePacket(
            packet_type=unpacked[0],
            version=unpacked[1],
            node_id=unpacked[2],
            timestamp=unpacked[3],
            parent_ipv6=_decode_optional_ipv6(unpacked[4]),
        )

    if msg_type == TYPE_NEIGHBOR_ALERT:
        raise PacketParseError("neighbor alert is lateral mesh traffic")

    raise PacketParseError(f"unsupported packet type 0x{msg_type:02x}")


def _decode_optional_ipv6(raw: bytes) -> str | None:
    if raw == b"\x00" * 16:
        return None
    return str(ipaddress.IPv6Address(raw))
