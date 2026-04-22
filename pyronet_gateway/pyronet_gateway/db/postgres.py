from __future__ import annotations

from contextlib import contextmanager
from typing import Callable, Iterator

from .migrations import LATEST_SCHEMA_VERSION, MIGRATIONS


class PostgresDatabase:
    def __init__(self, dsn: str, *, connect: Callable[[str], object] | None = None) -> None:
        self._dsn = dsn
        self._connect = connect or _default_connect

    @contextmanager
    def connection(self) -> Iterator[object]:
        connection = self._connect(self._dsn)
        try:
            yield connection
        finally:
            close = getattr(connection, "close", None)
            if callable(close):
                close()

    @contextmanager
    def transaction(self) -> Iterator[object]:
        with self.connection() as connection:
            try:
                yield connection
            except Exception:
                rollback = getattr(connection, "rollback", None)
                if callable(rollback):
                    rollback()
                raise
            else:
                commit = getattr(connection, "commit", None)
                if callable(commit):
                    commit()

    def apply_migrations(self) -> None:
        with self.transaction() as connection:
            current_version = self._read_schema_version(connection)
            for version in range(current_version + 1, LATEST_SCHEMA_VERSION + 1):
                for statement in _split_sql_script(MIGRATIONS[version]):
                    connection.execute(statement)
                connection.execute("DELETE FROM schema_meta")
                connection.execute("INSERT INTO schema_meta(version) VALUES (%s)", (version,))

    def _read_schema_version(self, connection) -> int:
        connection.execute("CREATE TABLE IF NOT EXISTS schema_meta (version INTEGER NOT NULL)")
        row = connection.execute("SELECT version FROM schema_meta LIMIT 1").fetchone()
        return int(row[0]) if row is not None else 0


def _default_connect(dsn: str) -> object:
    try:
        import psycopg
    except ImportError as exc:
        raise RuntimeError("psycopg is required to run the phase 3 CSP backend") from exc
    return psycopg.connect(dsn)


def _split_sql_script(script: str) -> list[str]:
    statements = []
    for statement in script.split(";"):
        stripped = statement.strip()
        if stripped:
            statements.append(stripped)
    return statements
