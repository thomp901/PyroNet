from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol

from .protocol.backhaul import NodeUplinkEnvelope, UplinkReceipt


@dataclass(frozen=True)
class StoredReceiptRecord:
    gateway_id: int
    uplink_id: int
    status: int
    receipt_bytes: bytes
    detail: str | None
    created_at: int


@dataclass(frozen=True)
class StoredEnvelopeRecord:
    gateway_id: int
    uplink_id: int
    version: int
    received_at: int
    observed_src_ipv6: str
    payload_type: int
    node_id: int | None
    raw_envelope: bytes
    payload_bytes: bytes
    created_at: int


@dataclass(frozen=True)
class MalformedEnvelopeRejectRecord:
    gateway_id: int
    uplink_id: int
    version: int | None
    raw_request_body: bytes
    reject_reason: str
    created_at: int


class IdempotencyStore(Protocol):
    def get_receipt(
        self,
        gateway_id: int,
        uplink_id: int,
        *,
        connection: object | None = None,
    ) -> StoredReceiptRecord | None:
        """Return the stored terminal receipt for an already-ingested uplink."""

    def store_envelope(
        self,
        envelope: NodeUplinkEnvelope,
        *,
        raw_envelope: bytes,
        payload_type: int,
        node_id: int | None,
        created_at: int,
        connection: object | None = None,
    ) -> None:
        """Persist the raw 0x82 envelope and parsed metadata."""

    def store_malformed_envelope_reject(
        self,
        *,
        gateway_id: int,
        uplink_id: int,
        version: int | None,
        raw_request_body: bytes,
        reject_reason: str,
        created_at: int,
        connection: object | None = None,
    ) -> None:
        """Persist raw malformed 0x82 bytes when the durable identity is recoverable."""

    def store_receipt(
        self,
        receipt: UplinkReceipt,
        *,
        receipt_bytes: bytes,
        detail: str | None,
        created_at: int,
        connection: object | None = None,
    ) -> None:
        """Persist the terminal receipt bytes returned to the gateway."""


class PostgresIdempotencyStore:
    def __init__(self, database) -> None:
        self._database = database

    def get_receipt(
        self,
        gateway_id: int,
        uplink_id: int,
        *,
        connection: object | None = None,
    ) -> StoredReceiptRecord | None:
        def _load(conn):
            row = conn.execute(
                """
                SELECT gateway_id, uplink_id, terminal_status, stored_receipt, detail, created_at
                FROM uplink_receipts
                WHERE gateway_id = %s AND uplink_id = %s
                """,
                (gateway_id, uplink_id),
            ).fetchone()
            if row is None:
                return None
            return StoredReceiptRecord(
                gateway_id=int(row[0]),
                uplink_id=int(row[1]),
                status=int(row[2]),
                receipt_bytes=bytes(row[3]),
                detail=row[4],
                created_at=int(row[5]),
            )

        if connection is not None:
            return _load(connection)
        with self._database.connection() as conn:
            return _load(conn)

    def store_envelope(
        self,
        envelope: NodeUplinkEnvelope,
        *,
        raw_envelope: bytes,
        payload_type: int,
        node_id: int | None,
        created_at: int,
        connection: object | None = None,
    ) -> None:
        conn = connection or _require_connection()
        conn.execute(
            """
            INSERT INTO uplink_envelopes(
                gateway_id,
                uplink_id,
                version,
                received_at,
                observed_src_ipv6,
                payload_type,
                node_id,
                raw_envelope,
                payload_bytes,
                created_at
            )
            VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
            ON CONFLICT (gateway_id, uplink_id) DO NOTHING
            """,
            (
                envelope.gateway_id,
                envelope.uplink_id,
                envelope.version,
                envelope.received_at,
                envelope.observed_src_ipv6,
                payload_type,
                node_id,
                raw_envelope,
                envelope.payload,
                created_at,
            ),
        )

    def store_malformed_envelope_reject(
        self,
        *,
        gateway_id: int,
        uplink_id: int,
        version: int | None,
        raw_request_body: bytes,
        reject_reason: str,
        created_at: int,
        connection: object | None = None,
    ) -> None:
        conn = connection or _require_connection()
        conn.execute(
            """
            INSERT INTO malformed_uplink_rejects(
                gateway_id,
                uplink_id,
                version,
                raw_request_body,
                reject_reason,
                created_at
            )
            VALUES (%s, %s, %s, %s, %s, %s)
            ON CONFLICT (gateway_id, uplink_id) DO NOTHING
            """,
            (
                gateway_id,
                uplink_id,
                version,
                raw_request_body,
                reject_reason,
                created_at,
            ),
        )

    def store_receipt(
        self,
        receipt: UplinkReceipt,
        *,
        receipt_bytes: bytes,
        detail: str | None,
        created_at: int,
        connection: object | None = None,
    ) -> None:
        conn = connection or _require_connection()
        conn.execute(
            """
            INSERT INTO uplink_receipts(
                gateway_id,
                uplink_id,
                terminal_status,
                stored_receipt,
                detail,
                created_at
            )
            VALUES (%s, %s, %s, %s, %s, %s)
            ON CONFLICT (gateway_id, uplink_id) DO NOTHING
            """,
            (
                receipt.gateway_id,
                receipt.uplink_id,
                receipt.status,
                receipt_bytes,
                detail,
                created_at,
            ),
        )


def _require_connection():
    raise RuntimeError("an explicit database connection is required for this operation")
