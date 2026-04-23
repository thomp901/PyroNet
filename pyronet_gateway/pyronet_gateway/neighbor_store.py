from __future__ import annotations

import json
from dataclasses import dataclass
from typing import Protocol


@dataclass(frozen=True)
class NeighborSetRecord:
    node_id: int
    computed_at: int
    radius_meters: float
    max_neighbors: int | None
    neighbor_node_ids: list[int]


class NeighborStore(Protocol):
    def get_neighbor_set(self, node_id: int, *, connection: object | None = None) -> NeighborSetRecord | None:
        """Load the currently persisted neighbor set for one node."""

    def upsert_neighbor_set(
        self,
        *,
        node_id: int,
        neighbor_node_ids: list[int],
        computed_at: int,
        radius_meters: float,
        max_neighbors: int | None,
        connection: object | None = None,
    ) -> None:
        """Persist the effective neighbor set and normalized membership rows."""


class PostgresNeighborStore:
    def __init__(self, database) -> None:
        self._database = database

    def get_neighbor_set(self, node_id: int, *, connection: object | None = None) -> NeighborSetRecord | None:
        def _load(conn):
            row = conn.execute(
                """
                SELECT node_id, computed_at, radius_meters, max_neighbors, neighbor_node_ids
                FROM neighbor_sets
                WHERE node_id = %s
                """,
                (node_id,),
            ).fetchone()
            if row is None:
                return None
            return NeighborSetRecord(
                node_id=int(row[0]),
                computed_at=int(row[1]),
                radius_meters=float(row[2]),
                max_neighbors=row[3],
                neighbor_node_ids=list(json.loads(row[4])),
            )

        if connection is not None:
            return _load(connection)
        with self._database.connection() as conn:
            return _load(conn)

    def upsert_neighbor_set(
        self,
        *,
        node_id: int,
        neighbor_node_ids: list[int],
        computed_at: int,
        radius_meters: float,
        max_neighbors: int | None,
        connection: object | None = None,
    ) -> None:
        conn = connection or _require_connection()
        encoded_neighbors = json.dumps(neighbor_node_ids, sort_keys=False)
        conn.execute(
            """
            INSERT INTO neighbor_sets(
                node_id,
                computed_at,
                radius_meters,
                max_neighbors,
                neighbor_node_ids
            )
            VALUES (%s, %s, %s, %s, %s)
            ON CONFLICT (node_id) DO UPDATE SET
                computed_at = EXCLUDED.computed_at,
                radius_meters = EXCLUDED.radius_meters,
                max_neighbors = EXCLUDED.max_neighbors,
                neighbor_node_ids = EXCLUDED.neighbor_node_ids
            """,
            (node_id, computed_at, radius_meters, max_neighbors, encoded_neighbors),
        )
        conn.execute("DELETE FROM neighbor_set_members WHERE node_id = %s", (node_id,))
        for neighbor_node_id in neighbor_node_ids:
            conn.execute(
                """
                INSERT INTO neighbor_set_members(node_id, neighbor_node_id, computed_at)
                VALUES (%s, %s, %s)
                """,
                (node_id, neighbor_node_id, computed_at),
            )


def _require_connection():
    raise RuntimeError("an explicit database connection is required for this operation")
