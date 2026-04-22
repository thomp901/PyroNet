from __future__ import annotations

import struct
from dataclasses import dataclass

from .protocol.backhaul import (
    BackhaulParseError,
    GatewayRegistration,
    NodeUplinkEnvelope,
    UplinkReceipt,
    RECEIPT_DURABLE_INGEST,
    RECEIPT_PERMANENT_REJECT,
)
from .protocol.node_packets import NodePacket, PacketParseError, parse_node_packet


SUPPORTED_BACKHAUL_VERSION = 1
SUPPORTED_NODE_PACKET_VERSION = 1

_UPLINK_IDENTITY = struct.Struct("<BBHQ")


@dataclass(frozen=True)
class UplinkIdentity:
    version: int
    gateway_id: int
    uplink_id: int


def parse_gateway_registration(raw: bytes) -> GatewayRegistration:
    registration = GatewayRegistration.from_bytes(raw)
    if registration.version != SUPPORTED_BACKHAUL_VERSION:
        raise BackhaulParseError(f"unsupported gateway registration version {registration.version}")
    return registration


def parse_node_uplink_envelope(raw: bytes) -> NodeUplinkEnvelope:
    envelope = NodeUplinkEnvelope.from_bytes(raw)
    if envelope.version != SUPPORTED_BACKHAUL_VERSION:
        raise BackhaulParseError(f"unsupported node uplink envelope version {envelope.version}")
    return envelope


def parse_embedded_node_packet(raw: bytes) -> NodePacket:
    packet = parse_node_packet(raw)
    if packet.version != SUPPORTED_NODE_PACKET_VERSION:
        raise PacketParseError(f"unsupported node packet version {packet.version}")
    return packet


def recover_uplink_identity(raw: bytes) -> UplinkIdentity | None:
    if len(raw) < _UPLINK_IDENTITY.size:
        return None
    unpacked = _UPLINK_IDENTITY.unpack(raw[: _UPLINK_IDENTITY.size])
    if unpacked[0] != 0x82:
        return None
    return UplinkIdentity(version=unpacked[1], gateway_id=unpacked[2], uplink_id=unpacked[3])


def build_uplink_receipt(*, version: int, gateway_id: int, uplink_id: int, status: int) -> UplinkReceipt:
    return UplinkReceipt(version=version, gateway_id=gateway_id, uplink_id=uplink_id, status=status)


def durable_ingest_receipt(envelope: NodeUplinkEnvelope) -> UplinkReceipt:
    return build_uplink_receipt(
        version=envelope.version,
        gateway_id=envelope.gateway_id,
        uplink_id=envelope.uplink_id,
        status=RECEIPT_DURABLE_INGEST,
    )


def permanent_reject_receipt(*, version: int, gateway_id: int, uplink_id: int) -> UplinkReceipt:
    return build_uplink_receipt(
        version=version,
        gateway_id=gateway_id,
        uplink_id=uplink_id,
        status=RECEIPT_PERMANENT_REJECT,
    )

