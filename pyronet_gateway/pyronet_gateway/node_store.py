from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol

from .protocol.backhaul import NodeUplinkEnvelope
from .protocol.node_packets import ParentUpdatePacket, RegistrationPacket, SensorPacket


@dataclass(frozen=True)
class NodeRecord:
    node_id: int
    current_ipv6: str | None
    latitude: float | None
    longitude: float | None
    firmware_version: int | None
    latest_battery: int | None
    latest_risk_level: int | None
    latest_temperature: int | None
    latest_humidity: int | None
    latest_voc: int | None
    latest_pm25: int | None
    last_seen: int | None
    last_gateway_id: int | None
    latest_parent_ipv6: str | None
    created_at: int
    updated_at: int
    connectivity_state: str | None = None


class NodeStore(Protocol):
    def upsert_registration(
        self,
        packet: RegistrationPacket,
        *,
        envelope: NodeUplinkEnvelope,
        connection: object | None = None,
    ) -> None:
        """Project a registration into the current node state."""

    def upsert_sensor_state(
        self,
        packet: SensorPacket,
        *,
        envelope: NodeUplinkEnvelope,
        connection: object | None = None,
    ) -> None:
        """Project the latest sensor values into the current node state."""

    def upsert_parent_update(
        self,
        packet: ParentUpdatePacket,
        *,
        envelope: NodeUplinkEnvelope,
        connection: object | None = None,
    ) -> None:
        """Project the latest preferred-parent state into the current node state."""

    def get(self, node_id: int, *, connection: object | None = None) -> NodeRecord | None:
        """Load current CSP-side node state."""

    def list_with_coordinates(self, *, connection: object | None = None) -> list[NodeRecord]:
        """List nodes that have valid coordinates."""

    def list_recently_seen(self, min_last_seen: int, *, connection: object | None = None) -> list[NodeRecord]:
        """List nodes seen recently enough for active management."""

    def list_for_connectivity(self, *, connection: object | None = None) -> list[NodeRecord]:
        """List nodes relevant to connectivity monitoring."""

    def set_connectivity_state(
        self,
        node_id: int,
        *,
        connectivity_state: str,
        updated_at: int,
        connection: object | None = None,
    ) -> None:
        """Persist the current connectivity state for a node."""


class PostgresNodeStore:
    def __init__(self, database) -> None:
        self._database = database

    def upsert_registration(
        self,
        packet: RegistrationPacket,
        *,
        envelope: NodeUplinkEnvelope,
        connection: object | None = None,
    ) -> None:
        conn = connection or _require_connection()
        conn.execute(
            """
            INSERT INTO nodes(
                node_id,
                current_ipv6,
                latitude,
                longitude,
                firmware_version,
                latest_battery,
                latest_risk_level,
                latest_temperature,
                latest_humidity,
                latest_voc,
                latest_pm25,
                last_seen,
                last_gateway_id,
                latest_parent_ipv6,
                created_at,
                updated_at
            )
            VALUES (%s, %s, %s, %s, %s, %s, NULL, NULL, NULL, NULL, NULL, %s, %s, %s, %s, %s)
            ON CONFLICT (node_id) DO UPDATE SET
                current_ipv6 = EXCLUDED.current_ipv6,
                latitude = EXCLUDED.latitude,
                longitude = EXCLUDED.longitude,
                firmware_version = EXCLUDED.firmware_version,
                latest_battery = EXCLUDED.latest_battery,
                last_seen = EXCLUDED.last_seen,
                last_gateway_id = EXCLUDED.last_gateway_id,
                latest_parent_ipv6 = EXCLUDED.latest_parent_ipv6,
                updated_at = EXCLUDED.updated_at
            """,
            (
                packet.node_id,
                envelope.observed_src_ipv6,
                packet.latitude,
                packet.longitude,
                packet.fw_version,
                packet.battery_pct,
                envelope.received_at,
                envelope.gateway_id,
                packet.parent_ipv6,
                envelope.received_at,
                envelope.received_at,
            ),
        )

    def upsert_sensor_state(
        self,
        packet: SensorPacket,
        *,
        envelope: NodeUplinkEnvelope,
        connection: object | None = None,
    ) -> None:
        conn = connection or _require_connection()
        conn.execute(
            """
            INSERT INTO nodes(
                node_id,
                current_ipv6,
                latest_battery,
                latest_risk_level,
                latest_temperature,
                latest_humidity,
                latest_voc,
                latest_pm25,
                last_seen,
                last_gateway_id,
                created_at,
                updated_at
            )
            VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
            ON CONFLICT (node_id) DO UPDATE SET
                current_ipv6 = EXCLUDED.current_ipv6,
                latest_battery = EXCLUDED.latest_battery,
                latest_risk_level = EXCLUDED.latest_risk_level,
                latest_temperature = EXCLUDED.latest_temperature,
                latest_humidity = EXCLUDED.latest_humidity,
                latest_voc = EXCLUDED.latest_voc,
                latest_pm25 = EXCLUDED.latest_pm25,
                last_seen = EXCLUDED.last_seen,
                last_gateway_id = EXCLUDED.last_gateway_id,
                updated_at = EXCLUDED.updated_at
            """,
            (
                packet.node_id,
                envelope.observed_src_ipv6,
                packet.battery_pct,
                packet.risk_level,
                packet.temperature,
                packet.humidity,
                packet.voc_iaq,
                packet.pm25,
                envelope.received_at,
                envelope.gateway_id,
                envelope.received_at,
                envelope.received_at,
            ),
        )

    def upsert_parent_update(
        self,
        packet: ParentUpdatePacket,
        *,
        envelope: NodeUplinkEnvelope,
        connection: object | None = None,
    ) -> None:
        conn = connection or _require_connection()
        conn.execute(
            """
            INSERT INTO nodes(
                node_id,
                current_ipv6,
                last_seen,
                last_gateway_id,
                latest_parent_ipv6,
                created_at,
                updated_at
            )
            VALUES (%s, %s, %s, %s, %s, %s, %s)
            ON CONFLICT (node_id) DO UPDATE SET
                current_ipv6 = EXCLUDED.current_ipv6,
                last_seen = EXCLUDED.last_seen,
                last_gateway_id = EXCLUDED.last_gateway_id,
                latest_parent_ipv6 = EXCLUDED.latest_parent_ipv6,
                updated_at = EXCLUDED.updated_at
            """,
            (
                packet.node_id,
                envelope.observed_src_ipv6,
                envelope.received_at,
                envelope.gateway_id,
                packet.parent_ipv6,
                envelope.received_at,
                envelope.received_at,
            ),
        )

    def get(self, node_id: int, *, connection: object | None = None) -> NodeRecord | None:
        def _load(conn):
            row = conn.execute(
                """
                SELECT
                    node_id,
                    current_ipv6,
                    latitude,
                    longitude,
                    firmware_version,
                    latest_battery,
                    latest_risk_level,
                    latest_temperature,
                    latest_humidity,
                    latest_voc,
                    latest_pm25,
                    last_seen,
                    last_gateway_id,
                    latest_parent_ipv6,
                    created_at,
                    updated_at,
                    connectivity_state
                FROM nodes
                WHERE node_id = %s
                """,
                (node_id,),
            ).fetchone()
            if row is None:
                return None
            return NodeRecord(
                node_id=int(row[0]),
                current_ipv6=row[1],
                latitude=row[2],
                longitude=row[3],
                firmware_version=row[4],
                latest_battery=row[5],
                latest_risk_level=row[6],
                latest_temperature=row[7],
                latest_humidity=row[8],
                latest_voc=row[9],
                latest_pm25=row[10],
                last_seen=row[11],
                last_gateway_id=row[12],
                latest_parent_ipv6=row[13],
                created_at=int(row[14]),
                updated_at=int(row[15]),
                connectivity_state=row[16],
            )

        if connection is not None:
            return _load(connection)
        with self._database.connection() as conn:
            return _load(conn)

    def list_with_coordinates(self, *, connection: object | None = None) -> list[NodeRecord]:
        return self._list_by_query(
            """
            SELECT
                node_id,
                current_ipv6,
                latitude,
                longitude,
                firmware_version,
                latest_battery,
                latest_risk_level,
                latest_temperature,
                latest_humidity,
                latest_voc,
                latest_pm25,
                last_seen,
                last_gateway_id,
                latest_parent_ipv6,
                created_at,
                updated_at,
                connectivity_state
            FROM nodes
            WHERE latitude IS NOT NULL AND longitude IS NOT NULL
            ORDER BY node_id
            """,
            (),
            connection=connection,
        )

    def list_recently_seen(self, min_last_seen: int, *, connection: object | None = None) -> list[NodeRecord]:
        return self._list_by_query(
            """
            SELECT
                node_id,
                current_ipv6,
                latitude,
                longitude,
                firmware_version,
                latest_battery,
                latest_risk_level,
                latest_temperature,
                latest_humidity,
                latest_voc,
                latest_pm25,
                last_seen,
                last_gateway_id,
                latest_parent_ipv6,
                created_at,
                updated_at,
                connectivity_state
            FROM nodes
            WHERE last_seen IS NOT NULL AND last_seen >= %s
            ORDER BY node_id
            """,
            (min_last_seen,),
            connection=connection,
        )

    def list_for_connectivity(self, *, connection: object | None = None) -> list[NodeRecord]:
        return self._list_by_query(
            """
            SELECT
                node_id,
                current_ipv6,
                latitude,
                longitude,
                firmware_version,
                latest_battery,
                latest_risk_level,
                latest_temperature,
                latest_humidity,
                latest_voc,
                latest_pm25,
                last_seen,
                last_gateway_id,
                latest_parent_ipv6,
                created_at,
                updated_at,
                connectivity_state
            FROM nodes
            WHERE last_seen IS NOT NULL
            ORDER BY node_id
            """,
            (),
            connection=connection,
        )

    def set_connectivity_state(
        self,
        node_id: int,
        *,
        connectivity_state: str,
        updated_at: int,
        connection: object | None = None,
    ) -> None:
        conn = connection or _require_connection()
        conn.execute(
            """
            UPDATE nodes
            SET connectivity_state = %s, updated_at = %s
            WHERE node_id = %s
            """,
            (connectivity_state, updated_at, node_id),
        )

    def _list_by_query(self, sql: str, params: tuple, *, connection: object | None = None) -> list[NodeRecord]:
        def _load_many(conn):
            rows = conn.execute(sql, params).fetchall()
            return [
                NodeRecord(
                    node_id=int(row[0]),
                    current_ipv6=row[1],
                    latitude=row[2],
                    longitude=row[3],
                    firmware_version=row[4],
                    latest_battery=row[5],
                    latest_risk_level=row[6],
                    latest_temperature=row[7],
                    latest_humidity=row[8],
                    latest_voc=row[9],
                    latest_pm25=row[10],
                    last_seen=row[11],
                    last_gateway_id=row[12],
                    latest_parent_ipv6=row[13],
                    created_at=int(row[14]),
                    updated_at=int(row[15]),
                    connectivity_state=row[16],
                )
                for row in rows
            ]

        if connection is not None:
            return _load_many(connection)
        with self._database.connection() as conn:
            return _load_many(conn)


def _require_connection():
    raise RuntimeError("an explicit database connection is required for this operation")
