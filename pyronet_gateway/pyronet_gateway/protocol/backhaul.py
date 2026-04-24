from __future__ import annotations

import ipaddress
import struct
from dataclasses import dataclass

TYPE_GATEWAY_REGISTRATION = 0x81
TYPE_NODE_UPLINK_ENVELOPE = 0x82
TYPE_UPLINK_RECEIPT = 0x83
TYPE_DOWNLINK_REQUEST = 0x84
TYPE_DOWNLINK_RESULT = 0x85

RECEIPT_DURABLE_INGEST = 0x00
RECEIPT_PERMANENT_REJECT = 0x01

DOWNLINK_STATUS_DELIVERED = 0x00
DOWNLINK_STATUS_UNKNOWN_NODE = 0x01
DOWNLINK_STATUS_MESH_DELIVERY_FAILED = 0x02
DOWNLINK_STATUS_PERMANENT_REJECT = 0x03

_GATEWAY_REGISTRATION = struct.Struct("<BBHI16sffH")
_NODE_UPLINK_HEADER = struct.Struct("<BBHQI16sH")
_UPLINK_RECEIPT = struct.Struct("<BBHQB")
_DOWNLINK_REQUEST_HEADER = struct.Struct("<BBHQHIH")
_DOWNLINK_RESULT = struct.Struct("<BBHQHBI")


class BackhaulParseError(ValueError):
    """Raised when a backhaul message is invalid."""


@dataclass(frozen=True)
class GatewayRegistration:
    version: int
    gateway_id: int
    timestamp: int
    wisun_ipv6: str
    latitude: float
    longitude: float
    sw_version: int

    def to_bytes(self) -> bytes:
        wisun_ipv6 = ipaddress.IPv6Address(self.wisun_ipv6).packed
        return _GATEWAY_REGISTRATION.pack(
            TYPE_GATEWAY_REGISTRATION,
            self.version,
            self.gateway_id,
            self.timestamp,
            wisun_ipv6,
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
            wisun_ipv6=str(ipaddress.IPv6Address(unpacked[4])),
            latitude=unpacked[5],
            longitude=unpacked[6],
            sw_version=unpacked[7],
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


@dataclass(frozen=True)
class DownlinkRequest:
    version: int
    gateway_id: int
    downlink_id: int
    target_node_id: int
    created_at: int
    payload: bytes

    def to_bytes(self) -> bytes:
        return _DOWNLINK_REQUEST_HEADER.pack(
            TYPE_DOWNLINK_REQUEST,
            self.version,
            self.gateway_id,
            self.downlink_id,
            self.target_node_id,
            self.created_at,
            len(self.payload),
        ) + self.payload

    @classmethod
    def from_bytes(cls, raw: bytes) -> "DownlinkRequest":
        if len(raw) < _DOWNLINK_REQUEST_HEADER.size:
            raise BackhaulParseError("downlink request shorter than header")
        unpacked = _DOWNLINK_REQUEST_HEADER.unpack(raw[: _DOWNLINK_REQUEST_HEADER.size])
        if unpacked[0] != TYPE_DOWNLINK_REQUEST:
            raise BackhaulParseError(f"unexpected message type 0x{unpacked[0]:02x}")
        payload_len = unpacked[6]
        payload = raw[_DOWNLINK_REQUEST_HEADER.size :]
        if len(payload) != payload_len:
            raise BackhaulParseError("payload length does not match downlink request header")
        return cls(
            version=unpacked[1],
            gateway_id=unpacked[2],
            downlink_id=unpacked[3],
            target_node_id=unpacked[4],
            created_at=unpacked[5],
            payload=payload,
        )


@dataclass(frozen=True)
class DownlinkRequestIdentity:
    version: int
    gateway_id: int
    downlink_id: int
    target_node_id: int
    created_at: int


@dataclass(frozen=True)
class DownlinkResult:
    version: int
    gateway_id: int
    downlink_id: int
    target_node_id: int
    status: int
    completed_at: int

    def to_bytes(self) -> bytes:
        return _DOWNLINK_RESULT.pack(
            TYPE_DOWNLINK_RESULT,
            self.version,
            self.gateway_id,
            self.downlink_id,
            self.target_node_id,
            self.status,
            self.completed_at,
        )

    @classmethod
    def from_bytes(cls, raw: bytes) -> "DownlinkResult":
        if len(raw) != _DOWNLINK_RESULT.size:
            raise BackhaulParseError(f"downlink result length {len(raw)} != {_DOWNLINK_RESULT.size}")
        unpacked = _DOWNLINK_RESULT.unpack(raw)
        if unpacked[0] != TYPE_DOWNLINK_RESULT:
            raise BackhaulParseError(f"unexpected message type 0x{unpacked[0]:02x}")
        return cls(
            version=unpacked[1],
            gateway_id=unpacked[2],
            downlink_id=unpacked[3],
            target_node_id=unpacked[4],
            status=unpacked[5],
            completed_at=unpacked[6],
        )


def decode_downlink_request_identity(raw: bytes) -> DownlinkRequestIdentity | None:
    if len(raw) < _DOWNLINK_REQUEST_HEADER.size:
        return None
    unpacked = _DOWNLINK_REQUEST_HEADER.unpack(raw[: _DOWNLINK_REQUEST_HEADER.size])
    if unpacked[0] != TYPE_DOWNLINK_REQUEST:
        return None
    return DownlinkRequestIdentity(
        version=unpacked[1],
        gateway_id=unpacked[2],
        downlink_id=unpacked[3],
        target_node_id=unpacked[4],
        created_at=unpacked[5],
    )


def node_uplink_header_size() -> int:
    return _NODE_UPLINK_HEADER.size


def downlink_request_header_size() -> int:
    return _DOWNLINK_REQUEST_HEADER.size
