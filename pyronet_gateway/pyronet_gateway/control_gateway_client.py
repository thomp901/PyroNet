from __future__ import annotations

import json
import urllib.error
import urllib.request
from dataclasses import dataclass
from typing import Callable


@dataclass(frozen=True)
class GatewayControlResult:
    request_type: str
    target_node_id: int
    status_code: int | None
    response_body: str
    delivery_result: str
    response_payload: dict | None


class ControlGatewayClient:
    def __init__(
        self,
        *,
        base_url: str,
        timeout_seconds: float,
        urlopen: Callable[..., object] | None = None,
    ) -> None:
        self._base_url = base_url.rstrip("/")
        self._timeout_seconds = timeout_seconds
        self._urlopen = urlopen or urllib.request.urlopen

    @property
    def base_url(self) -> str:
        return self._base_url

    def send_nn_table(self, *, target_node_id: int, neighbor_node_ids: list[int]) -> GatewayControlResult:
        return self._post_json(
            request_type="nn-table",
            path="/api/v1/downlinks/nn-table",
            target_node_id=target_node_id,
            payload={
                "target_node_id": target_node_id,
                "neighbor_node_ids": neighbor_node_ids,
            },
        )

    def send_time_sync(self, *, target_node_id: int, epoch: int) -> GatewayControlResult:
        return self._post_json(
            request_type="time-sync",
            path="/api/v1/downlinks/time-sync",
            target_node_id=target_node_id,
            payload={
                "target_node_id": target_node_id,
                "epoch": epoch,
            },
        )

    def send_config_update(self, *, request_body: dict) -> GatewayControlResult:
        return self._post_json(
            request_type="config",
            path="/api/v1/downlinks/config",
            target_node_id=int(request_body["target_node_id"]),
            payload=request_body,
        )

    def _post_json(self, *, request_type: str, path: str, target_node_id: int, payload: dict) -> GatewayControlResult:
        body = json.dumps(payload, sort_keys=True).encode("utf-8")
        request = urllib.request.Request(
            f"{self._base_url}{path}",
            data=body,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        try:
            response = self._urlopen(request, timeout=self._timeout_seconds)
            raw_body = response.read()
            status_code = int(getattr(response, "status", 200))
        except urllib.error.HTTPError as exc:
            raw_body = exc.read()
            status_code = exc.code
        except urllib.error.URLError as exc:
            return GatewayControlResult(
                request_type=request_type,
                target_node_id=target_node_id,
                status_code=None,
                response_body=str(exc.reason),
                delivery_result="gateway_transport_error",
                response_payload=None,
            )

        decoded_body = raw_body.decode("utf-8", errors="replace")
        payload_body = None
        try:
            payload_body = json.loads(decoded_body) if decoded_body else None
        except json.JSONDecodeError:
            payload_body = None

        delivery_result = "gateway_http_error"
        if isinstance(payload_body, dict) and "delivery_result" in payload_body:
            delivery_result = str(payload_body["delivery_result"])
        elif 200 <= status_code < 300:
            delivery_result = "accepted"

        return GatewayControlResult(
            request_type=request_type,
            target_node_id=target_node_id,
            status_code=status_code,
            response_body=decoded_body,
            delivery_result=delivery_result,
            response_payload=payload_body if isinstance(payload_body, dict) else None,
        )
