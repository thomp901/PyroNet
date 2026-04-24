from __future__ import annotations

import sqlite3
import threading
from contextlib import contextmanager
from dataclasses import dataclass
from pathlib import Path
from typing import Iterator

from .interfaces import OutboxRecord
from .protocol.backhaul import NodeUplinkEnvelope
from .protocol.node_packets import NodePacket, ParentUpdatePacket, RegistrationPacket

MIGRATIONS = {
    1: """
    CREATE TABLE IF NOT EXISTS schema_meta (
        version INTEGER NOT NULL
    );

    CREATE TABLE IF NOT EXISTS gateway_runtime (
        gateway_id INTEGER PRIMARY KEY,
        latitude REAL NOT NULL,
        longitude REAL NOT NULL,
        sw_version INTEGER NOT NULL,
        backhaul_base_url TEXT NOT NULL,
        coap_bind_host TEXT NOT NULL,
        coap_bind_port INTEGER NOT NULL,
        coap_resource_path TEXT NOT NULL,
        db_path TEXT NOT NULL,
        updated_at INTEGER NOT NULL
    );

    CREATE TABLE IF NOT EXISTS sequences (
        name TEXT PRIMARY KEY,
        next_value INTEGER NOT NULL
    );

    CREATE TABLE IF NOT EXISTS node_state (
        node_id INTEGER PRIMARY KEY,
        current_ipv6 TEXT NOT NULL,
        last_seen INTEGER NOT NULL,
        last_packet_type INTEGER NOT NULL,
        parent_ipv6 TEXT,
        last_registration_ipv6 TEXT,
        created_at INTEGER NOT NULL,
        updated_at INTEGER NOT NULL
    );

    CREATE TABLE IF NOT EXISTS outbox (
        uplink_id INTEGER PRIMARY KEY,
        gateway_id INTEGER NOT NULL,
        received_at INTEGER NOT NULL,
        observed_src_ipv6 TEXT NOT NULL,
        payload_type INTEGER NOT NULL,
        payload_bytes BLOB NOT NULL,
        envelope BLOB NOT NULL,
        attempt_count INTEGER NOT NULL DEFAULT 0,
        last_attempt_at INTEGER,
        next_attempt_at INTEGER NOT NULL,
        last_error TEXT,
        state TEXT NOT NULL DEFAULT 'pending',
        created_at INTEGER NOT NULL,
        updated_at INTEGER NOT NULL
    );

    CREATE TABLE IF NOT EXISTS dead_letter (
        uplink_id INTEGER PRIMARY KEY,
        gateway_id INTEGER NOT NULL,
        received_at INTEGER NOT NULL,
        observed_src_ipv6 TEXT NOT NULL,
        payload_type INTEGER NOT NULL,
        payload_bytes BLOB NOT NULL,
        envelope BLOB NOT NULL,
        receipt_status INTEGER NOT NULL,
        reason TEXT NOT NULL,
        moved_at INTEGER NOT NULL
    );
    """,
    2: """
    ALTER TABLE gateway_runtime ADD COLUMN http_api_bind_host TEXT NOT NULL DEFAULT '::';
    ALTER TABLE gateway_runtime ADD COLUMN http_api_port INTEGER NOT NULL DEFAULT 8081;
    """,
    3: """
    CREATE TABLE IF NOT EXISTS downlink_terminal_results (
        gateway_id INTEGER NOT NULL,
        downlink_id INTEGER NOT NULL,
        request_version INTEGER NOT NULL,
        target_node_id INTEGER NOT NULL,
        request_body BLOB NOT NULL,
        status INTEGER NOT NULL,
        result_body BLOB NOT NULL,
        created_at INTEGER NOT NULL,
        completed_at INTEGER NOT NULL,
        PRIMARY KEY (gateway_id, downlink_id)
    );
    """,
}

LATEST_SCHEMA_VERSION = max(MIGRATIONS)


@dataclass(frozen=True)
class NodeStateRecord:
    node_id: int
    current_ipv6: str
    last_seen: int
    last_packet_type: int
    parent_ipv6: str | None
    last_registration_ipv6: str | None
    created_at: int
    updated_at: int


@dataclass(frozen=True)
class DeadLetterRecord:
    uplink_id: int
    receipt_status: int
    reason: str


@dataclass(frozen=True)
class DownlinkTerminalResultRecord:
    gateway_id: int
    downlink_id: int
    request_version: int
    target_node_id: int
    request_body: bytes
    status: int
    result_body: bytes
    created_at: int
    completed_at: int


class SQLiteDatabase:
    def __init__(self, path: Path | str) -> None:
        self._path = str(path)
        self._connection = sqlite3.connect(self._path, check_same_thread=False)
        self._connection.row_factory = sqlite3.Row
        self._lock = threading.RLock()
        self._connection.execute("PRAGMA journal_mode=WAL")
        self._connection.execute("PRAGMA foreign_keys=ON")
        self._apply_migrations()
        self._connection.execute("INSERT OR IGNORE INTO sequences(name, next_value) VALUES ('uplink_id', 1)")
        self._connection.commit()

    @property
    def connection(self) -> sqlite3.Connection:
        return self._connection

    @contextmanager
    def transaction(self) -> Iterator[sqlite3.Connection]:
        with self._lock:
            try:
                self._connection.execute("BEGIN IMMEDIATE")
                yield self._connection
            except Exception:
                self._connection.rollback()
                raise
            else:
                self._connection.commit()

    def persist_gateway_runtime(self, config, *, updated_at: int) -> None:
        self._connection.execute(
            """
            INSERT INTO gateway_runtime(
                gateway_id,
                latitude,
                longitude,
                sw_version,
                backhaul_base_url,
                coap_bind_host,
                coap_bind_port,
                coap_resource_path,
                http_api_bind_host,
                http_api_port,
                db_path,
                updated_at
            )
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(gateway_id) DO UPDATE SET
                latitude=excluded.latitude,
                longitude=excluded.longitude,
                sw_version=excluded.sw_version,
                backhaul_base_url=excluded.backhaul_base_url,
                coap_bind_host=excluded.coap_bind_host,
                coap_bind_port=excluded.coap_bind_port,
                coap_resource_path=excluded.coap_resource_path,
                http_api_bind_host=excluded.http_api_bind_host,
                http_api_port=excluded.http_api_port,
                db_path=excluded.db_path,
                updated_at=excluded.updated_at
            """,
            (
                config.gateway_id,
                config.latitude,
                config.longitude,
                config.sw_version,
                config.backhaul.base_url,
                config.coap.bind_host,
                config.coap.port,
                config.coap.resource_path,
                config.http_api.bind_host,
                config.http_api.port,
                str(config.runtime.db_path),
                updated_at,
            ),
        )
        self._connection.commit()

    def close(self) -> None:
        self._connection.close()

    def _apply_migrations(self) -> None:
        row = self._connection.execute("PRAGMA user_version").fetchone()
        current_version = int(row[0])
        for version in range(current_version + 1, LATEST_SCHEMA_VERSION + 1):
            self._connection.executescript(MIGRATIONS[version])
            self._connection.execute(f"PRAGMA user_version={version}")


class SQLiteNodeStateStore:
    def __init__(self, database: SQLiteDatabase) -> None:
        self._database = database

    def update_from_packet(
        self,
        packet: NodePacket,
        *,
        observed_src_ipv6: str,
        received_at: int,
        connection: sqlite3.Connection | None = None,
    ) -> None:
        conn = connection or self._database.connection
        parent_ipv6 = None
        last_registration_ipv6 = None
        if isinstance(packet, RegistrationPacket):
            parent_ipv6 = packet.parent_ipv6
            last_registration_ipv6 = observed_src_ipv6
        elif isinstance(packet, ParentUpdatePacket):
            parent_ipv6 = packet.parent_ipv6

        conn.execute(
            """
            INSERT INTO node_state(
                node_id,
                current_ipv6,
                last_seen,
                last_packet_type,
                parent_ipv6,
                last_registration_ipv6,
                created_at,
                updated_at
            )
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(node_id) DO UPDATE SET
                current_ipv6=excluded.current_ipv6,
                last_seen=excluded.last_seen,
                last_packet_type=excluded.last_packet_type,
                parent_ipv6=COALESCE(excluded.parent_ipv6, node_state.parent_ipv6),
                last_registration_ipv6=COALESCE(excluded.last_registration_ipv6, node_state.last_registration_ipv6),
                updated_at=excluded.updated_at
            """,
            (
                packet.node_id,
                observed_src_ipv6,
                received_at,
                packet.packet_type,
                parent_ipv6,
                last_registration_ipv6,
                received_at,
                received_at,
            ),
        )
        if connection is None:
            conn.commit()

    def get(self, node_id: int) -> NodeStateRecord | None:
        row = self._database.connection.execute(
            """
            SELECT
                node_id,
                current_ipv6,
                last_seen,
                last_packet_type,
                parent_ipv6,
                last_registration_ipv6,
                created_at,
                updated_at
            FROM node_state
            WHERE node_id = ?
            """,
            (node_id,),
        ).fetchone()
        if row is None:
            return None
        return NodeStateRecord(
            node_id=row["node_id"],
            current_ipv6=row["current_ipv6"],
            last_seen=row["last_seen"],
            last_packet_type=row["last_packet_type"],
            parent_ipv6=row["parent_ipv6"],
            last_registration_ipv6=row["last_registration_ipv6"],
            created_at=row["created_at"],
            updated_at=row["updated_at"],
        )

    def get_many(self, node_ids: list[int]) -> dict[int, NodeStateRecord]:
        if not node_ids:
            return {}
        placeholders = ", ".join("?" for _ in node_ids)
        rows = self._database.connection.execute(
            f"""
            SELECT
                node_id,
                current_ipv6,
                last_seen,
                last_packet_type,
                parent_ipv6,
                last_registration_ipv6,
                created_at,
                updated_at
            FROM node_state
            WHERE node_id IN ({placeholders})
            """,
            tuple(node_ids),
        ).fetchall()
        return {
            row["node_id"]: NodeStateRecord(
                node_id=row["node_id"],
                current_ipv6=row["current_ipv6"],
                last_seen=row["last_seen"],
                last_packet_type=row["last_packet_type"],
                parent_ipv6=row["parent_ipv6"],
                last_registration_ipv6=row["last_registration_ipv6"],
                created_at=row["created_at"],
                updated_at=row["updated_at"],
            )
            for row in rows
        }


class SQLiteDeadLetterStore:
    def __init__(self, database: SQLiteDatabase) -> None:
        self._database = database

    def put(
        self,
        *,
        uplink_id: int,
        gateway_id: int,
        received_at: int,
        observed_src_ipv6: str,
        payload_type: int,
        payload_bytes: bytes,
        envelope: bytes,
        receipt_status: int,
        reason: str,
        moved_at: int,
        connection: sqlite3.Connection | None = None,
    ) -> None:
        conn = connection or self._database.connection
        conn.execute(
            """
            INSERT OR REPLACE INTO dead_letter(
                uplink_id,
                gateway_id,
                received_at,
                observed_src_ipv6,
                payload_type,
                payload_bytes,
                envelope,
                receipt_status,
                reason,
                moved_at
            )
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                uplink_id,
                gateway_id,
                received_at,
                observed_src_ipv6,
                payload_type,
                payload_bytes,
                envelope,
                receipt_status,
                reason,
                moved_at,
            ),
        )
        if connection is None:
            conn.commit()

    def list_records(self) -> list[DeadLetterRecord]:
        rows = self._database.connection.execute(
            "SELECT uplink_id, receipt_status, reason FROM dead_letter ORDER BY uplink_id ASC"
        ).fetchall()
        return [
            DeadLetterRecord(
                uplink_id=row["uplink_id"],
                receipt_status=row["receipt_status"],
                reason=row["reason"],
            )
            for row in rows
        ]


class SQLiteOutboxStore:
    def __init__(self, database: SQLiteDatabase, dead_letter_store: SQLiteDeadLetterStore | None = None) -> None:
        self._database = database
        self._dead_letter_store = dead_letter_store or SQLiteDeadLetterStore(database)

    def enqueue(
        self,
        *,
        gateway_id: int,
        observed_src_ipv6: str,
        payload_type: int,
        received_at: int,
        payload: bytes,
        version: int,
        connection: sqlite3.Connection | None = None,
    ) -> NodeUplinkEnvelope:
        conn = connection or self._database.connection
        uplink_id = self._allocate_uplink_id(conn)
        envelope = NodeUplinkEnvelope(
            version=version,
            gateway_id=gateway_id,
            uplink_id=uplink_id,
            received_at=received_at,
            observed_src_ipv6=observed_src_ipv6,
            payload=payload,
        )
        conn.execute(
            """
            INSERT INTO outbox(
                uplink_id,
                gateway_id,
                received_at,
                observed_src_ipv6,
                payload_type,
                payload_bytes,
                envelope,
                attempt_count,
                last_attempt_at,
                next_attempt_at,
                last_error,
                state,
                created_at,
                updated_at
            )
            VALUES (?, ?, ?, ?, ?, ?, ?, 0, NULL, ?, NULL, 'pending', ?, ?)
            """,
            (
                uplink_id,
                gateway_id,
                received_at,
                observed_src_ipv6,
                payload_type,
                payload,
                envelope.to_bytes(),
                received_at,
                received_at,
                received_at,
            ),
        )
        if connection is None:
            conn.commit()
        return envelope

    def list_due(self, now: int, *, limit: int = 100) -> list[OutboxRecord]:
        rows = self._database.connection.execute(
            """
            SELECT envelope, attempt_count, next_attempt_at, last_attempt_at, last_error, created_at
            FROM outbox
            WHERE state = 'pending' AND next_attempt_at <= ?
            ORDER BY uplink_id ASC
            LIMIT ?
            """,
            (now, limit),
        ).fetchall()
        return [
            OutboxRecord(
                envelope=NodeUplinkEnvelope.from_bytes(row["envelope"]),
                attempt_count=row["attempt_count"],
                next_attempt_at=row["next_attempt_at"],
                last_attempt_at=row["last_attempt_at"],
                last_error=row["last_error"],
                created_at=row["created_at"],
            )
            for row in rows
        ]

    def pending_uplink_ids(self) -> list[int]:
        rows = self._database.connection.execute(
            "SELECT uplink_id FROM outbox WHERE state = 'pending' ORDER BY uplink_id ASC"
        ).fetchall()
        return [int(row["uplink_id"]) for row in rows]

    def dead_letter_uplink_ids(self) -> list[int]:
        return [record.uplink_id for record in self._dead_letter_store.list_records()]

    def load_envelope(self, uplink_id: int) -> NodeUplinkEnvelope | None:
        row = self._database.connection.execute(
            "SELECT envelope FROM outbox WHERE uplink_id = ?",
            (uplink_id,),
        ).fetchone()
        if row is None:
            return None
        return NodeUplinkEnvelope.from_bytes(row["envelope"])

    def mark_retry(self, uplink_id: int, *, attempted_at: int, next_attempt_at: int, error: str) -> None:
        self._database.connection.execute(
            """
            UPDATE outbox
            SET attempt_count = attempt_count + 1,
                last_attempt_at = ?,
                next_attempt_at = ?,
                last_error = ?,
                updated_at = ?
            WHERE uplink_id = ?
            """,
            (attempted_at, next_attempt_at, error, attempted_at, uplink_id),
        )
        self._database.connection.commit()

    def delete(self, uplink_id: int) -> None:
        self._database.connection.execute("DELETE FROM outbox WHERE uplink_id = ?", (uplink_id,))
        self._database.connection.commit()

    def move_to_dead_letter(self, uplink_id: int, *, receipt_status: int, reason: str, finalized_at: int) -> None:
        with self._database.transaction() as conn:
            row = conn.execute(
                """
                SELECT gateway_id, received_at, observed_src_ipv6, payload_type, payload_bytes, envelope
                FROM outbox
                WHERE uplink_id = ?
                """,
                (uplink_id,),
            ).fetchone()
            if row is None:
                return
            self._dead_letter_store.put(
                uplink_id=uplink_id,
                gateway_id=row["gateway_id"],
                received_at=row["received_at"],
                observed_src_ipv6=row["observed_src_ipv6"],
                payload_type=row["payload_type"],
                payload_bytes=row["payload_bytes"],
                envelope=row["envelope"],
                receipt_status=receipt_status,
                reason=reason,
                moved_at=finalized_at,
                connection=conn,
            )
            conn.execute("DELETE FROM outbox WHERE uplink_id = ?", (uplink_id,))

    def _allocate_uplink_id(self, conn: sqlite3.Connection) -> int:
        row = conn.execute("SELECT next_value FROM sequences WHERE name = 'uplink_id'").fetchone()
        uplink_id = int(row["next_value"])
        conn.execute(
            "UPDATE sequences SET next_value = ? WHERE name = 'uplink_id'",
            (uplink_id + 1,),
        )
        return uplink_id


class SQLiteDownlinkAuditStore:
    def __init__(self, database: SQLiteDatabase) -> None:
        self._database = database

    def get_terminal_result(self, *, gateway_id: int, downlink_id: int) -> DownlinkTerminalResultRecord | None:
        row = self._database.connection.execute(
            """
            SELECT
                gateway_id,
                downlink_id,
                request_version,
                target_node_id,
                request_body,
                status,
                result_body,
                created_at,
                completed_at
            FROM downlink_terminal_results
            WHERE gateway_id = ? AND downlink_id = ?
            """,
            (gateway_id, downlink_id),
        ).fetchone()
        if row is None:
            return None
        return DownlinkTerminalResultRecord(
            gateway_id=row["gateway_id"],
            downlink_id=row["downlink_id"],
            request_version=row["request_version"],
            target_node_id=row["target_node_id"],
            request_body=row["request_body"],
            status=row["status"],
            result_body=row["result_body"],
            created_at=row["created_at"],
            completed_at=row["completed_at"],
        )

    def store_terminal_result(
        self,
        *,
        gateway_id: int,
        downlink_id: int,
        request_version: int,
        target_node_id: int,
        request_body: bytes,
        status: int,
        result_body: bytes,
        created_at: int,
        completed_at: int,
    ) -> DownlinkTerminalResultRecord:
        with self._database.transaction() as conn:
            conn.execute(
                """
                INSERT INTO downlink_terminal_results(
                    gateway_id,
                    downlink_id,
                    request_version,
                    target_node_id,
                    request_body,
                    status,
                    result_body,
                    created_at,
                    completed_at
                )
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
                ON CONFLICT(gateway_id, downlink_id) DO UPDATE SET
                    request_version = excluded.request_version,
                    target_node_id = excluded.target_node_id,
                    request_body = excluded.request_body,
                    status = excluded.status,
                    result_body = excluded.result_body,
                    created_at = excluded.created_at,
                    completed_at = excluded.completed_at
                """,
                (
                    gateway_id,
                    downlink_id,
                    request_version,
                    target_node_id,
                    request_body,
                    status,
                    result_body,
                    created_at,
                    completed_at,
                ),
            )
            row = conn.execute(
                """
                SELECT
                    gateway_id,
                    downlink_id,
                    request_version,
                    target_node_id,
                    request_body,
                    status,
                    result_body,
                    created_at,
                    completed_at
                FROM downlink_terminal_results
                WHERE gateway_id = ? AND downlink_id = ?
                """,
                (gateway_id, downlink_id),
            ).fetchone()
        return DownlinkTerminalResultRecord(
            gateway_id=row["gateway_id"],
            downlink_id=row["downlink_id"],
            request_version=row["request_version"],
            target_node_id=row["target_node_id"],
            request_body=row["request_body"],
            status=row["status"],
            result_body=row["result_body"],
            created_at=row["created_at"],
            completed_at=row["completed_at"],
        )

    def list_terminal_results(self) -> list[DownlinkTerminalResultRecord]:
        rows = self._database.connection.execute(
            """
            SELECT
                gateway_id,
                downlink_id,
                request_version,
                target_node_id,
                request_body,
                status,
                result_body,
                created_at,
                completed_at
            FROM downlink_terminal_results
            ORDER BY gateway_id ASC, downlink_id ASC
            """
        ).fetchall()
        return [
            DownlinkTerminalResultRecord(
                gateway_id=row["gateway_id"],
                downlink_id=row["downlink_id"],
                request_version=row["request_version"],
                target_node_id=row["target_node_id"],
                request_body=row["request_body"],
                status=row["status"],
                result_body=row["result_body"],
                created_at=row["created_at"],
                completed_at=row["completed_at"],
            )
            for row in rows
        ]
