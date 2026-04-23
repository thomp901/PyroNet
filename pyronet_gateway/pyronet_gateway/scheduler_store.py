from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol


@dataclass(frozen=True)
class SchedulerRunRecord:
    id: int
    job_type: str
    started_at: int
    completed_at: int | None
    status: str
    details: str | None


class SchedulerStore(Protocol):
    def start_run(
        self,
        *,
        job_type: str,
        started_at: int,
        details: str | None,
        connection: object | None = None,
    ) -> int:
        """Persist the start of one scheduler run and return its id."""

    def complete_run(
        self,
        *,
        run_id: int,
        completed_at: int,
        status: str,
        details: str | None,
        connection: object | None = None,
    ) -> None:
        """Persist the terminal state of one scheduler run."""

    def latest_completed_run(
        self,
        job_type: str,
        *,
        connection: object | None = None,
    ) -> SchedulerRunRecord | None:
        """Return the most recent completed run for one job type."""


class PostgresSchedulerStore:
    def start_run(
        self,
        *,
        job_type: str,
        started_at: int,
        details: str | None,
        connection: object | None = None,
    ) -> int:
        conn = connection or _require_connection()
        row = conn.execute(
            """
            INSERT INTO scheduler_runs(job_type, started_at, completed_at, status, details)
            VALUES (%s, %s, NULL, 'running', %s)
            RETURNING id
            """,
            (job_type, started_at, details),
        ).fetchone()
        return int(row[0])

    def complete_run(
        self,
        *,
        run_id: int,
        completed_at: int,
        status: str,
        details: str | None,
        connection: object | None = None,
    ) -> None:
        conn = connection or _require_connection()
        conn.execute(
            """
            UPDATE scheduler_runs
            SET completed_at = %s,
                status = %s,
                details = %s
            WHERE id = %s
            """,
            (completed_at, status, details, run_id),
        )

    def latest_completed_run(
        self,
        job_type: str,
        *,
        connection: object | None = None,
    ) -> SchedulerRunRecord | None:
        conn = connection or _require_connection()
        row = conn.execute(
            """
            SELECT id, job_type, started_at, completed_at, status, details
            FROM scheduler_runs
            WHERE job_type = %s AND completed_at IS NOT NULL
            ORDER BY completed_at DESC, id DESC
            LIMIT 1
            """,
            (job_type,),
        ).fetchone()
        if row is None:
            return None
        return SchedulerRunRecord(
            id=int(row[0]),
            job_type=row[1],
            started_at=int(row[2]),
            completed_at=int(row[3]) if row[3] is not None else None,
            status=row[4],
            details=row[5],
        )


def _require_connection():
    raise RuntimeError("an explicit database connection is required for this operation")
