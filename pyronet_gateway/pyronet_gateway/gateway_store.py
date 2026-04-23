from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol

from .protocol.backhaul import GatewayRegistration


@dataclass(frozen=True)
class GatewayRecord:
    gateway_id: int
    latitude: float | None
    longitude: float | None
    sw_version: int | None
    last_registered_at: int | None
    created_at: int
    updated_at: int


@dataclass(frozen=True)
class GatewayRegistrationRecord:
    gateway_id: int
    timestamp: int
    raw_payload: bytes
    created_at: int


class GatewayStore(Protocol):
    def ensure_placeholder(
        self,
        gateway_id: int,
        *,
        seen_at: int,
        connection: object | None = None,
    ) -> None:
        """Ensure a gateway row exists even if no prior 0x81 has been seen."""

    def upsert_registration(
        self,
        registration: GatewayRegistration,
        *,
        raw_payload: bytes,
        now: int,
        connection: object | None = None,
    ) -> None:
        """Upsert gateway metadata and append a registration history row."""


class PostgresGatewayStore:
    def ensure_placeholder(
        self,
        gateway_id: int,
        *,
        seen_at: int,
        connection: object | None = None,
    ) -> None:
        conn = connection or _require_connection()
        conn.execute(
            """
            INSERT INTO gateways(
                gateway_id,
                latitude,
                longitude,
                sw_version,
                last_registered_at,
                created_at,
                updated_at
            )
            VALUES (%s, NULL, NULL, NULL, NULL, %s, %s)
            ON CONFLICT (gateway_id) DO UPDATE SET updated_at = EXCLUDED.updated_at
            """,
            (gateway_id, seen_at, seen_at),
        )

    def upsert_registration(
        self,
        registration: GatewayRegistration,
        *,
        raw_payload: bytes,
        now: int,
        connection: object | None = None,
    ) -> None:
        conn = connection or _require_connection()
        conn.execute(
            """
            INSERT INTO gateways(
                gateway_id,
                latitude,
                longitude,
                sw_version,
                last_registered_at,
                created_at,
                updated_at
            )
            VALUES (%s, %s, %s, %s, %s, %s, %s)
            ON CONFLICT (gateway_id) DO UPDATE SET
                latitude = EXCLUDED.latitude,
                longitude = EXCLUDED.longitude,
                sw_version = EXCLUDED.sw_version,
                last_registered_at = EXCLUDED.last_registered_at,
                updated_at = EXCLUDED.updated_at
            """,
            (
                registration.gateway_id,
                registration.latitude,
                registration.longitude,
                registration.sw_version,
                registration.timestamp,
                now,
                now,
            ),
        )
        conn.execute(
            """
            INSERT INTO gateway_registrations(gateway_id, timestamp, raw_payload, created_at)
            VALUES (%s, %s, %s, %s)
            """,
            (registration.gateway_id, registration.timestamp, raw_payload, now),
        )


def _require_connection():
    raise RuntimeError("an explicit database connection is required for this operation")
