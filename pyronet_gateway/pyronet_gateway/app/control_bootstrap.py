from __future__ import annotations

import argparse
import logging
import time

from ..alert_store import PostgresAlertStore
from ..backhaul_http_api import BackhaulHttpApi, BackhaulHttpServer
from ..config import load_csp_config
from ..config_store import PostgresConfigStore
from ..config_workflow_service import ConfigWorkflowService
from ..connectivity_event_store import PostgresConnectivityEventStore
from ..connectivity_monitor import ConnectivityMonitor
from ..control_attempt_store import PostgresControlAttemptStore
from ..control_gateway_client import ControlGatewayClient
from ..control_http_api import ControlHttpApi, ControlHttpServer
from ..db.postgres import PostgresDatabase
from ..gateway_store import PostgresGatewayStore
from ..idempotency_store import PostgresIdempotencyStore
from ..ingest_service import IngestService
from ..neighbor_compute_service import NeighborComputeService
from ..neighbor_push_service import NeighborPushService
from ..neighbor_store import PostgresNeighborStore
from ..node_store import PostgresNodeStore
from ..scheduler_store import PostgresSchedulerStore
from ..sensor_history_store import PostgresSensorHistoryStore
from ..time_sync_scheduler import TimeSyncScheduler
from ..topology_store import PostgresTopologyStore


class ControlPlaneApplication:
    def __init__(self, config, *, database: PostgresDatabase) -> None:
        self.config = config
        self.database = database
        self.node_store = PostgresNodeStore(database)
        self.gateway_client = ControlGatewayClient(
            base_url=config.control.gateway_api.base_url,
            timeout_seconds=config.control.gateway_api.http_timeout_seconds,
        )
        self.control_attempt_store = PostgresControlAttemptStore()
        self.scheduler_store = PostgresSchedulerStore()
        self.neighbor_store = PostgresNeighborStore(database)
        self.config_store = PostgresConfigStore()
        self.connectivity_event_store = PostgresConnectivityEventStore()
        self.config_workflow_service = ConfigWorkflowService(
            transaction_manager=database,
            config_store=self.config_store,
            control_attempt_store=self.control_attempt_store,
            gateway_client=self.gateway_client,
        )

        self.ingest_service = IngestService(
            transaction_manager=database,
            idempotency_store=PostgresIdempotencyStore(database),
            gateway_store=PostgresGatewayStore(),
            node_store=self.node_store,
            sensor_history_store=PostgresSensorHistoryStore(),
            topology_store=PostgresTopologyStore(),
            alert_store=PostgresAlertStore(),
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
        self.control_http_api = ControlHttpApi(
            config_workflow_service=self.config_workflow_service,
            max_request_body_bytes=config.control_http_api.max_request_body_bytes,
        )
        self.control_http_server = ControlHttpServer(
            bind_host=config.control_http_api.bind_host,
            port=config.control_http_api.port,
            control_http_api=self.control_http_api,
        )
        self.neighbor_push_service = NeighborPushService(
            transaction_manager=database,
            node_store=self.node_store,
            neighbor_store=self.neighbor_store,
            control_attempt_store=self.control_attempt_store,
            gateway_client=self.gateway_client,
            compute_service=NeighborComputeService(
                radius_meters=config.control.neighbor_policy.radius_meters,
                max_neighbors=config.control.neighbor_policy.max_neighbors,
            ),
        )
        self.time_sync_scheduler = TimeSyncScheduler(
            transaction_manager=database,
            node_store=self.node_store,
            scheduler_store=self.scheduler_store,
            control_attempt_store=self.control_attempt_store,
            gateway_client=self.gateway_client,
            active_node_window_seconds=config.control.time_sync.active_node_window_seconds,
        )
        self.connectivity_monitor = ConnectivityMonitor(
            transaction_manager=database,
            node_store=self.node_store,
            connectivity_event_store=self.connectivity_event_store,
            stale_after_seconds=config.control.connectivity.stale_after_seconds,
        )
        self._last_neighbor_recompute_at: int | None = None
        self._last_time_sync_at: int | None = self._load_last_completed_run_at("time-sync")
        self._last_connectivity_scan_at: int | None = None

    def run_forever(self) -> None:
        self.backhaul_http_server.start()
        self.control_http_server.start()
        try:
            while True:
                now = int(time.time())
                if self._should_run(now, self._last_neighbor_recompute_at, self.config.control.neighbor_policy.recompute_interval_seconds):
                    self.neighbor_push_service.recompute_and_push(now=now)
                    self._last_neighbor_recompute_at = now
                if self._should_run(now, self._last_time_sync_at, self.config.control.time_sync.interval_seconds):
                    self.time_sync_scheduler.run_once(now=now)
                    self._last_time_sync_at = now
                if self._should_run(now, self._last_connectivity_scan_at, self.config.control.connectivity.poll_interval_seconds):
                    self.connectivity_monitor.run_once(now=now)
                    self._last_connectivity_scan_at = now
                time.sleep(self.config.control.scheduler_poll_interval_seconds)
        finally:
            self.close()

    def close(self) -> None:
        self.control_http_server.close()
        self.backhaul_http_server.close()

    @staticmethod
    def _should_run(now: int, last_run_at: int | None, interval_seconds: int) -> bool:
        return last_run_at is None or now - last_run_at >= interval_seconds

    def _load_last_completed_run_at(self, job_type: str) -> int | None:
        latest = self.scheduler_store.latest_completed_run(job_type)
        if latest is None:
            return None
        return latest.completed_at


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="PyroNet CSP control-plane service")
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
    app = ControlPlaneApplication(config, database=database)
    try:
        app.run_forever()
    except KeyboardInterrupt:
        return 0
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
