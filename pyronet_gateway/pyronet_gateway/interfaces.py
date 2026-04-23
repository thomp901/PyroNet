from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol

from .protocol.backhaul import GatewayRegistration, NodeUplinkEnvelope, UplinkReceipt
from .protocol.node_packets import NodePacket


@dataclass(frozen=True)
class OutboxRecord:
    envelope: NodeUplinkEnvelope
    attempt_count: int
    next_attempt_at: int
    last_attempt_at: int | None
    last_error: str | None
    created_at: int


class PacketIntake(Protocol):
    def submit(self, src_ipv6: str, udp_payload: bytes, received_at: int | None = None) -> object:
        """Terminate node-facing CoAP and pass accepted node payloads onward."""


class NodeMappingStore(Protocol):
    def update_from_packet(
        self,
        packet: NodePacket,
        *,
        observed_src_ipv6: str,
        received_at: int,
    ) -> None:
        """Update current IPv6 and liveness from an accepted node packet."""


class DurableOutbox(Protocol):
    def enqueue(
        self,
        *,
        gateway_id: int,
        observed_src_ipv6: str,
        payload_type: int,
        received_at: int,
        payload: bytes,
        version: int,
    ) -> NodeUplinkEnvelope:
        """Persist an envelope before any backhaul transmission."""

    def list_due(self, now: int, *, limit: int = 100) -> list[OutboxRecord]:
        """Return pending envelopes eligible for transmission."""

    def mark_retry(self, uplink_id: int, *, attempted_at: int, next_attempt_at: int, error: str) -> None:
        """Record a non-terminal failure and schedule the next retry."""

    def delete(self, uplink_id: int) -> None:
        """Delete a durably ingested outbox record."""

    def move_to_dead_letter(self, uplink_id: int, *, receipt_status: int, reason: str, finalized_at: int) -> None:
        """Stop retrying and move an outbox record into dead-letter storage."""


class BackhaulClient(Protocol):
    def send_gateway_registration(self, registration: GatewayRegistration) -> None:
        """Send 0x81 and succeed only on HTTP 2xx."""

    def send_uplink(self, envelope: NodeUplinkEnvelope) -> UplinkReceipt:
        """Send 0x82 and return a valid terminal 0x83 receipt."""
