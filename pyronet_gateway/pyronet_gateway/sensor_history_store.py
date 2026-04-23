from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol

from .protocol.backhaul import NodeUplinkEnvelope
from .protocol.node_packets import SensorPacket


@dataclass(frozen=True)
class SensorReadingRecord:
    id: int
    gateway_id: int
    uplink_id: int
    packet_type: int
    node_id: int
    node_event_time: int
    gateway_received_at: int
    observed_src_ipv6: str
    temperature: int
    humidity: int
    voc: int
    pm25: int
    risk_level: int
    battery_pct: int
    raw_payload: bytes
    created_at: int


class SensorHistoryStore(Protocol):
    def append_reading(
        self,
        packet: SensorPacket,
        *,
        envelope: NodeUplinkEnvelope,
        connection: object | None = None,
    ) -> None:
        """Append one telemetry or alert reading row."""


class PostgresSensorHistoryStore:
    def append_reading(
        self,
        packet: SensorPacket,
        *,
        envelope: NodeUplinkEnvelope,
        connection: object | None = None,
    ) -> None:
        conn = connection or _require_connection()
        conn.execute(
            """
            INSERT INTO sensor_readings(
                gateway_id,
                uplink_id,
                packet_type,
                node_id,
                node_event_time,
                gateway_received_at,
                observed_src_ipv6,
                temperature,
                humidity,
                voc,
                pm25,
                risk_level,
                battery_pct,
                raw_payload,
                created_at
            )
            VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
            ON CONFLICT (gateway_id, uplink_id) DO NOTHING
            """,
            (
                envelope.gateway_id,
                envelope.uplink_id,
                packet.packet_type,
                packet.node_id,
                packet.timestamp,
                envelope.received_at,
                envelope.observed_src_ipv6,
                packet.temperature,
                packet.humidity,
                packet.voc_iaq,
                packet.pm25,
                packet.risk_level,
                packet.battery_pct,
                envelope.payload,
                envelope.received_at,
            ),
        )


def _require_connection():
    raise RuntimeError("an explicit database connection is required for this operation")
