from __future__ import annotations

import argparse
import logging
import time

from ..backhaul_client import HTTPBackhaulClient
from ..coap_downlink_client import CoapDownlinkClient
from ..coap_intake import CoapIntakeService, CoapUdpServer
from ..config import load_config
from ..downlink_delivery_service import DownlinkDeliveryService
from ..downlink_http_api import DownlinkHttpApi, DownlinkHttpServer
from ..registration_worker import RegistrationWorker
from ..retry_worker import OutboxRetryWorker, RetryPolicy
from ..service import GatewayService


class GatewayApplication:
    def __init__(self, config) -> None:
        self.config = config
        self.service = GatewayService.open(config, opened_at=int(time.time()))
        self.backhaul_client = HTTPBackhaulClient(
            base_url=config.backhaul.base_url,
            timeout_seconds=config.backhaul.http_timeout_seconds,
        )
        self.registration_worker = RegistrationWorker(
            service=self.service,
            backhaul_client=self.backhaul_client,
            retry_policy=RetryPolicy(
                base_delay_seconds=config.backhaul.registration_retry_base_delay_seconds,
                max_delay_seconds=config.backhaul.registration_retry_max_delay_seconds,
            ),
        )
        self.retry_worker = OutboxRetryWorker(
            outbox_store=self.service.outbox_store,
            backhaul_client=self.backhaul_client,
            retry_policy=RetryPolicy(
                base_delay_seconds=config.backhaul.uplink_retry_base_delay_seconds,
                max_delay_seconds=config.backhaul.uplink_retry_max_delay_seconds,
            ),
            registration_worker=self.registration_worker,
        )
        self.coap_downlink_client = CoapDownlinkClient(
            port=config.coap.downlink_port,
            resource_path=config.coap.downlink_resource_path,
            ack_timeout_seconds=config.coap.ack_timeout_seconds,
            max_retransmit=config.coap.max_retransmit,
        )
        self.downlink_delivery_service = DownlinkDeliveryService(
            gateway_id=config.gateway_id,
            backhaul_version=config.backhaul_version,
            node_state_store=self.service.node_state_store,
            result_store=self.service.downlink_audit_store,
            coap_downlink_client=self.coap_downlink_client,
        )
        self.downlink_http_api = DownlinkHttpApi(
            delivery_service=self.downlink_delivery_service,
            max_request_body_bytes=config.http_api.max_request_body_bytes,
        )
        self.downlink_http_server = DownlinkHttpServer(
            bind_host=config.http_api.bind_host,
            port=config.http_api.port,
            downlink_http_api=self.downlink_http_api,
        )
        self.coap_intake = CoapIntakeService(
            gateway_service=self.service,
            resource_path=config.coap.resource_path,
            duplicate_cache_ttl_seconds=config.coap.duplicate_cache_ttl_seconds,
            duplicate_cache_max_entries=config.coap.duplicate_cache_max_entries,
        )
        self.coap_server = CoapUdpServer(
            bind_host=config.coap.bind_host,
            port=config.coap.port,
            coap_intake_service=self.coap_intake,
        )

    def run_forever(self) -> None:
        self.coap_server.start()
        self.downlink_http_server.start()
        try:
            while True:
                now = int(time.time())
                self.registration_worker.run_once(now)
                self.retry_worker.run_once(now)
                time.sleep(self.config.runtime.worker_poll_interval_seconds)
        finally:
            self.close()

    def close(self) -> None:
        self.downlink_http_server.close()
        self.coap_server.close()
        self.service.close()


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="PyroNet gateway service")
    parser.add_argument("--config", required=True, help="Path to TOML config file")
    parser.add_argument("--log-level", default=None, help="Optional log level override")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)
    config = load_config(args.config)
    log_level = args.log_level or config.runtime.log_level
    logging.basicConfig(level=getattr(logging, log_level.upper(), logging.INFO))
    app = GatewayApplication(config)
    try:
        app.run_forever()
    except KeyboardInterrupt:
        return 0
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
