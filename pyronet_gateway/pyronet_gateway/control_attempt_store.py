from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol


@dataclass(frozen=True)
class ControlAttemptRecord:
    id: int
    request_type: str
    target_node_id: int
    request_body: str
    gateway_url: str
    gateway_id: int | None
    scheduler_run_id: int | None
    response_status: int | None
    response_body: str | None
    delivery_result: str
    created_at: int
    completed_at: int


class ControlAttemptStore(Protocol):
    def record_attempt(
        self,
        *,
        request_type: str,
        target_node_id: int,
        request_body: str,
        gateway_url: str,
        gateway_id: int | None,
        scheduler_run_id: int | None,
        response_status: int | None,
        response_body: str | None,
        delivery_result: str,
        created_at: int,
        completed_at: int,
        connection: object | None = None,
    ) -> int:
        """Persist one terminal control attempt row."""


class PostgresControlAttemptStore:
    def record_attempt(
        self,
        *,
        request_type: str,
        target_node_id: int,
        request_body: str,
        gateway_url: str,
        gateway_id: int | None,
        scheduler_run_id: int | None,
        response_status: int | None,
        response_body: str | None,
        delivery_result: str,
        created_at: int,
        completed_at: int,
        connection: object | None = None,
    ) -> int:
        conn = connection or _require_connection()
        row = conn.execute(
            """
            INSERT INTO control_attempts(
                request_type,
                target_node_id,
                request_body,
                gateway_url,
                gateway_id,
                scheduler_run_id,
                response_status,
                response_body,
                delivery_result,
                created_at,
                completed_at
            )
            VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
            RETURNING id
            """,
            (
                request_type,
                target_node_id,
                request_body,
                gateway_url,
                gateway_id,
                scheduler_run_id,
                response_status,
                response_body,
                delivery_result,
                created_at,
                completed_at,
            ),
        ).fetchone()
        return int(row[0])


def _require_connection():
    raise RuntimeError("an explicit database connection is required for this operation")
