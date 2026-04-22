from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol


@dataclass(frozen=True)
class RiskConfigRecord:
    config_id: int
    l2_temp_thresh: int
    l2_humidity_thresh: int
    l2_voc_thresh: int
    l3_temp_thresh: int
    l3_humidity_thresh: int
    l3_voc_thresh: int
    l4_voc_thresh: int
    l5_voc_thresh: int
    l5_pm25_thresh: int
    created_at: int
    created_by: str | None
    source: str | None
    comment: str | None


@dataclass(frozen=True)
class RiskConfigTargetRecord:
    config_id: int
    node_id: int
    desired_state: str
    latest_attempt_status: str | None
    latest_attempt_time: int | None
    latest_result_payload: str | None


class ConfigStore(Protocol):
    def allocate_next_config_id(self, *, connection: object | None = None) -> int:
        """Allocate the next monotonic config revision id."""

    def create_config(
        self,
        config: RiskConfigRecord,
        *,
        connection: object | None = None,
    ) -> None:
        """Persist one immutable risk config revision."""

    def upsert_targets(
        self,
        *,
        config_id: int,
        node_ids: list[int],
        desired_state: str,
        connection: object | None = None,
    ) -> None:
        """Persist the intended target set for one config revision."""

    def update_target_result(
        self,
        *,
        config_id: int,
        node_id: int,
        latest_attempt_status: str,
        latest_attempt_time: int,
        latest_result_payload: str,
        connection: object | None = None,
    ) -> None:
        """Update the latest per-target transport outcome."""


class PostgresConfigStore:
    def allocate_next_config_id(self, *, connection: object | None = None) -> int:
        conn = connection or _require_connection()
        conn.execute(
            """
            INSERT INTO control_sequences(name, next_value)
            VALUES ('config_id', 1)
            ON CONFLICT (name) DO NOTHING
            """
        )
        row = conn.execute(
            """
            SELECT next_value
            FROM control_sequences
            WHERE name = 'config_id'
            FOR UPDATE
            """
        ).fetchone()
        value = int(row[0])
        conn.execute(
            """
            UPDATE control_sequences
            SET next_value = %s
            WHERE name = 'config_id'
            """,
            (value + 1,),
        )
        return value

    def create_config(
        self,
        config: RiskConfigRecord,
        *,
        connection: object | None = None,
    ) -> None:
        conn = connection or _require_connection()
        conn.execute(
            """
            INSERT INTO risk_configs(
                config_id,
                l2_temp_thresh,
                l2_humidity_thresh,
                l2_voc_thresh,
                l3_temp_thresh,
                l3_humidity_thresh,
                l3_voc_thresh,
                l4_voc_thresh,
                l5_voc_thresh,
                l5_pm25_thresh,
                created_at,
                created_by,
                source,
                comment
            )
            VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
            """,
            (
                config.config_id,
                config.l2_temp_thresh,
                config.l2_humidity_thresh,
                config.l2_voc_thresh,
                config.l3_temp_thresh,
                config.l3_humidity_thresh,
                config.l3_voc_thresh,
                config.l4_voc_thresh,
                config.l5_voc_thresh,
                config.l5_pm25_thresh,
                config.created_at,
                config.created_by,
                config.source,
                config.comment,
            ),
        )

    def upsert_targets(
        self,
        *,
        config_id: int,
        node_ids: list[int],
        desired_state: str,
        connection: object | None = None,
    ) -> None:
        conn = connection or _require_connection()
        for node_id in node_ids:
            conn.execute(
                """
                INSERT INTO risk_config_targets(
                    config_id,
                    node_id,
                    desired_state,
                    latest_attempt_status,
                    latest_attempt_time,
                    latest_result_payload
                )
                VALUES (%s, %s, %s, NULL, NULL, NULL)
                ON CONFLICT (config_id, node_id) DO UPDATE SET desired_state = EXCLUDED.desired_state
                """,
                (config_id, node_id, desired_state),
            )

    def update_target_result(
        self,
        *,
        config_id: int,
        node_id: int,
        latest_attempt_status: str,
        latest_attempt_time: int,
        latest_result_payload: str,
        connection: object | None = None,
    ) -> None:
        conn = connection or _require_connection()
        conn.execute(
            """
            UPDATE risk_config_targets
            SET latest_attempt_status = %s,
                latest_attempt_time = %s,
                latest_result_payload = %s
            WHERE config_id = %s AND node_id = %s
            """,
            (
                latest_attempt_status,
                latest_attempt_time,
                latest_result_payload,
                config_id,
                node_id,
            ),
        )


def _require_connection():
    raise RuntimeError("an explicit database connection is required for this operation")
