from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol

from .protocol.backhaul import NodeUplinkEnvelope


@dataclass(frozen=True)
class TopologyEventRecord:
    id: int
    gateway_id: int
    uplink_id: int
    packet_type: int
    node_id: int
    parent_ipv6: str | None
    node_event_time: int | None
    gateway_received_at: int
    raw_payload: bytes
    created_at: int


class TopologyStore(Protocol):
    def append_event(
        self,
        *,
        packet_type: int,
        node_id: int,
        parent_ipv6: str | None,
        node_event_time: int | None,
        envelope: NodeUplinkEnvelope,
        raw_payload: bytes,
        connection: object | None = None,
    ) -> None:
        """Append one topology event derived from registration or parent update."""


class PostgresTopologyStore:
    def append_event(
        self,
        *,
        packet_type: int,
        node_id: int,
        parent_ipv6: str | None,
        node_event_time: int | None,
        envelope: NodeUplinkEnvelope,
        raw_payload: bytes,
        connection: object | None = None,
    ) -> None:
        conn = connection or _require_connection()
        conn.execute(
            """
            INSERT INTO topology_events(
                gateway_id,
                uplink_id,
                packet_type,
                node_id,
                parent_ipv6,
                node_event_time,
                gateway_received_at,
                raw_payload,
                created_at
            )
            VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s)
            ON CONFLICT (gateway_id, uplink_id) DO NOTHING
            """,
            (
                envelope.gateway_id,
                envelope.uplink_id,
                packet_type,
                node_id,
                parent_ipv6,
                node_event_time,
                envelope.received_at,
                raw_payload,
                envelope.received_at,
            ),
        )


def _require_connection():
    raise RuntimeError("an explicit database connection is required for this operation")
