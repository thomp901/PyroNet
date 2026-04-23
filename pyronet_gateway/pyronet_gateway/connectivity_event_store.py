from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol


@dataclass(frozen=True)
class ConnectivityEventRecord:
    id: int
    node_id: int
    event_type: str
    created_at: int
    detail: str | None


class ConnectivityEventStore(Protocol):
    def append_event(
        self,
        *,
        node_id: int,
        event_type: str,
        created_at: int,
        detail: str | None,
        connection: object | None = None,
    ) -> int:
        """Persist one connectivity transition event."""


class PostgresConnectivityEventStore:
    def append_event(
        self,
        *,
        node_id: int,
        event_type: str,
        created_at: int,
        detail: str | None,
        connection: object | None = None,
    ) -> int:
        conn = connection or _require_connection()
        row = conn.execute(
            """
            INSERT INTO node_connectivity_events(node_id, event_type, created_at, detail)
            VALUES (%s, %s, %s, %s)
            RETURNING id
            """,
            (node_id, event_type, created_at, detail),
        ).fetchone()
        return int(row[0])


def _require_connection():
    raise RuntimeError("an explicit database connection is required for this operation")
