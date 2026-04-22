from __future__ import annotations

import ipaddress
import struct
from dataclasses import dataclass

TYPE_GATEWAY_REGISTRATION = 0x81
TYPE_NODE_UPLINK_ENVELOPE = 0x82
TYPE_UPLINK_RECEIPT = 0x83

RECEIPT_DURABLE_INGEST = 0x00
RECEIPT_PERMANENT_REJECT = 0x01

_GATEWAY_REGISTRATION = struct.Struct("<BBHIffH")
_NODE_UPLINK_HEADER = struct.Struct("<BBHQI16sH")
_UPLINK_RECEIPT = struct.Struct("<BBHQB")


class BackhaulParseError(ValueError):
    """Raised when a backhaul message is invalid."""


@dataclass(frozen=True)
class GatewayRegistration:
    version: int
    gateway_id: int
    timestamp: int
    latitude: float
    longitude: float
    sw_version: int

    def to_bytes(self) -> bytes:
        return _GATEWAY_REGISTRATION.pack(
            TYPE_GATEWAY_REGISTRATION,
            self.version,
            self.gateway_id,
            self.timestamp,
            self.latitude,
            self.longitude,
            self.sw_version,
        )

    @classmethod
    def from_bytes(cls, raw: bytes) -> "GatewayRegistration":
        if len(raw) != _GATEWAY_REGISTRATION.size:
            raise BackhaulParseError(
                f"gateway registration length {len(raw)} != {_GATEWAY_REGISTRATION.size}"
            )
        unpacked = _GATEWAY_REGISTRATION.unpack(raw)
        if unpacked[0] != TYPE_GATEWAY_REGISTRATION:
            raise BackhaulParseError(f"unexpected message type 0x{unpacked[0]:02x}")
        return cls(
            version=unpacked[1],
            gateway_id=unpacked[2],
            timestamp=unpacked[3],
            latitude=unpacked[4],
            longitude=unpacked[5],
            sw_version=unpacked[6],
        )


@dataclass(frozen=True)
class NodeUplinkEnvelope:
    version: int
    gateway_id: int
    uplink_id: int
    received_at: int
    observed_src_ipv6: str
    payload: bytes

    def to_bytes(self) -> bytes:
        src_ipv6 = ipaddress.IPv6Address(self.observed_src_ipv6).packed
        header = _NODE_UPLINK_HEADER.pack(
            TYPE_NODE_UPLINK_ENVELOPE,
            self.version,
            self.gateway_id,
            self.uplink_id,
            self.received_at,
            src_ipv6,
            len(self.payload),
        )
        return header + self.payload

    @classmethod
    def from_bytes(cls, raw: bytes) -> "NodeUplinkEnvelope":
        if len(raw) < _NODE_UPLINK_HEADER.size:
            raise BackhaulParseError("uplink envelope shorter than header")
        header = raw[: _NODE_UPLINK_HEADER.size]
        unpacked = _NODE_UPLINK_HEADER.unpack(header)
        if unpacked[0] != TYPE_NODE_UPLINK_ENVELOPE:
            raise BackhaulParseError(f"unexpected message type 0x{unpacked[0]:02x}")
        payload_len = unpacked[6]
        payload = raw[_NODE_UPLINK_HEADER.size :]
        if len(payload) != payload_len:
            raise BackhaulParseError("payload length does not match envelope header")
        return cls(
            version=unpacked[1],
            gateway_id=unpacked[2],
            uplink_id=unpacked[3],
            received_at=unpacked[4],
            observed_src_ipv6=str(ipaddress.IPv6Address(unpacked[5])),
            payload=payload,
        )


@dataclass(frozen=True)
class UplinkReceipt:
    version: int
    gateway_id: int
    uplink_id: int
    status: int

    def to_bytes(self) -> bytes:
        return _UPLINK_RECEIPT.pack(
            TYPE_UPLINK_RECEIPT,
            self.version,
            self.gateway_id,
            self.uplink_id,
            self.status,
        )

    @classmethod
    def from_bytes(cls, raw: bytes) -> "UplinkReceipt":
        if len(raw) != _UPLINK_RECEIPT.size:
            raise BackhaulParseError(f"uplink receipt length {len(raw)} != {_UPLINK_RECEIPT.size}")
        unpacked = _UPLINK_RECEIPT.unpack(raw)
        if unpacked[0] != TYPE_UPLINK_RECEIPT:
            raise BackhaulParseError(f"unexpected message type 0x{unpacked[0]:02x}")
        return cls(
            version=unpacked[1],
            gateway_id=unpacked[2],
            uplink_id=unpacked[3],
            status=unpacked[4],
        )


def node_uplink_header_size() -> int:
    return _NODE_UPLINK_HEADER.size
