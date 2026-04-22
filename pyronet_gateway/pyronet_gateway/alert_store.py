from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol

from .protocol.backhaul import NodeUplinkEnvelope
from .protocol.node_packets import SensorPacket


@dataclass(frozen=True)
class AlertRecord:
    id: int
    gateway_id: int
    uplink_id: int
    node_id: int
    node_event_time: int
    gateway_received_at: int
    risk_level: int
    battery_pct: int
    raw_payload: bytes
    created_at: int


class AlertStore(Protocol):
    def append_alert(
        self,
        packet: SensorPacket,
        *,
        envelope: NodeUplinkEnvelope,
        connection: object | None = None,
    ) -> None:
        """Persist a critical alert/event row derived from a 0x03 packet."""


class PostgresAlertStore:
    def append_alert(
        self,
        packet: SensorPacket,
        *,
        envelope: NodeUplinkEnvelope,
        connection: object | None = None,
    ) -> None:
        conn = connection or _require_connection()
        conn.execute(
            """
            INSERT INTO alerts(
                gateway_id,
                uplink_id,
                node_id,
                node_event_time,
                gateway_received_at,
                risk_level,
                battery_pct,
                raw_payload,
                created_at
            )
            VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s)
            ON CONFLICT (gateway_id, uplink_id) DO NOTHING
            """,
            (
                envelope.gateway_id,
                envelope.uplink_id,
                packet.node_id,
                packet.timestamp,
                envelope.received_at,
                packet.risk_level,
                packet.battery_pct,
                envelope.payload,
                envelope.received_at,
            ),
        )


def _require_connection():
    raise RuntimeError("an explicit database connection is required for this operation")
