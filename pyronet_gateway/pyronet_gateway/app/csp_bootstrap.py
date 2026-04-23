from __future__ import annotations

import argparse
import logging

from ..alert_store import PostgresAlertStore
from ..backhaul_http_api import BackhaulHttpApi, BackhaulHttpServer
from ..config import load_csp_config
from ..db.postgres import PostgresDatabase
from ..gateway_store import PostgresGatewayStore
from ..idempotency_store import PostgresIdempotencyStore
from ..ingest_service import IngestService
from ..node_store import PostgresNodeStore
from ..sensor_history_store import PostgresSensorHistoryStore
from ..topology_store import PostgresTopologyStore


class CspApplication:
    def __init__(
        self,
        config,
        *,
        transaction_manager,
        idempotency_store,
        gateway_store,
        node_store,
        sensor_history_store,
        topology_store,
        alert_store,
    ) -> None:
        self.config = config
        self.database = transaction_manager
        self.ingest_service = IngestService(
            transaction_manager=transaction_manager,
            idempotency_store=idempotency_store,
            gateway_store=gateway_store,
            node_store=node_store,
            sensor_history_store=sensor_history_store,
            topology_store=topology_store,
            alert_store=alert_store,
        )
        self.backhaul_http_api = BackhaulHttpApi(
            ingest_service=self.ingest_service,
            max_request_body_bytes=config.csp_http_api.max_request_body_bytes,
        )
        self.backhaul_http_server = BackhaulHttpServer(
            bind_host=config.csp_http_api.bind_host,
            port=config.csp_http_api.port,
            backhaul_http_api=self.backhaul_http_api,
        )

    def run_forever(self) -> None:
        self.backhaul_http_server.start()
        try:
            self.backhaul_http_server._thread.join()
        finally:
            self.close()

    def close(self) -> None:
        self.backhaul_http_server.close()


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="PyroNet CSP backhaul ingest service")
    parser.add_argument("--config", required=True, help="Path to TOML config file")
    parser.add_argument("--log-level", default=None, help="Optional log level override")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)
    config = load_csp_config(args.config)
    log_level = args.log_level or config.runtime.log_level
    logging.basicConfig(level=getattr(logging, log_level.upper(), logging.INFO))

    database = PostgresDatabase(config.postgres.dsn)
    database.apply_migrations()
    app = CspApplication(
        config,
        transaction_manager=database,
        idempotency_store=PostgresIdempotencyStore(database),
        gateway_store=PostgresGatewayStore(),
        node_store=PostgresNodeStore(database),
        sensor_history_store=PostgresSensorHistoryStore(),
        topology_store=PostgresTopologyStore(),
        alert_store=PostgresAlertStore(),
    )
    try:
        app.run_forever()
    except KeyboardInterrupt:
        return 0
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
