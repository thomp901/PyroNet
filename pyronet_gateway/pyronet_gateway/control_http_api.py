from __future__ import annotations

import http.server
import json
import socket
import threading
import time

from .config_workflow_service import RiskConfigSpec


ROUTES = {
    "/api/v1/control/configs": "config-rollout",
}


class ControlHttpApi:
    def __init__(self, *, config_workflow_service, max_request_body_bytes: int) -> None:
        self._config_workflow_service = config_workflow_service
        self._max_request_body_bytes = max_request_body_bytes

    def handle_request(self, *, method: str, path: str, body: bytes, now: int) -> tuple[int, dict[str, str], bytes]:
        if method != "POST":
            return self._json_response(405, {"error": "method not allowed"})
        if path not in ROUTES:
            return self._json_response(404, {"error": "unknown endpoint"})
        if len(body) > self._max_request_body_bytes:
            return self._json_response(400, {"error": "request body too large"})
        try:
            payload = json.loads(body.decode("utf-8"))
        except json.JSONDecodeError:
            return self._json_response(400, {"error": "invalid json body"})
        try:
            spec, target_node_ids = _validate_config_rollout_request(payload)
        except ValueError as exc:
            return self._json_response(400, {"error": str(exc)})

        result = self._config_workflow_service.create_and_push(
            spec=spec,
            target_node_ids=target_node_ids,
            now=now,
        )
        return self._json_response(
            201,
            {
                "config_id": result.config_id,
                "target_results": [
                    {
                        "request_type": item.request_type,
                        "target_node_id": item.target_node_id,
                        "status_code": item.status_code,
                        "delivery_result": item.delivery_result,
                        "response_body": item.response_body,
                    }
                    for item in result.target_results
                ],
            },
        )

    def _json_response(self, status_code: int, body: dict) -> tuple[int, dict[str, str], bytes]:
        encoded = json.dumps(body, sort_keys=True).encode("utf-8")
        return status_code, {"Content-Type": "application/json", "Content-Length": str(len(encoded))}, encoded


class _ThreadingHttpServer(http.server.ThreadingHTTPServer):
    daemon_threads = True


class _ThreadingIPv6HttpServer(_ThreadingHttpServer):
    address_family = socket.AF_INET6


class _ControlRequestHandler(http.server.BaseHTTPRequestHandler):
    def do_POST(self) -> None:
        self._handle()

    def do_GET(self) -> None:
        self._handle()

    def _handle(self) -> None:
        content_length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(content_length) if content_length > 0 else b""
        status, headers, response_body = self.server.control_http_api.handle_request(
            method=self.command,
            path=self.path,
            body=body,
            now=int(time.time()),
        )
        self.send_response(status)
        for key, value in headers.items():
            self.send_header(key, value)
        self.end_headers()
        self.wfile.write(response_body)

    def log_message(self, format: str, *args) -> None:
        return


class ControlHttpServer:
    def __init__(self, *, bind_host: str, port: int, control_http_api: ControlHttpApi) -> None:
        server_class = _ThreadingIPv6HttpServer if ":" in bind_host else _ThreadingHttpServer
        self._server = server_class((bind_host, port), _ControlRequestHandler)
        self._server.control_http_api = control_http_api
        self._thread: threading.Thread | None = None

    def start(self) -> None:
        self._thread = threading.Thread(target=self._server.serve_forever, name="pyronet-control-http", daemon=True)
        self._thread.start()

    def close(self) -> None:
        self._server.shutdown()
        self._server.server_close()
        if self._thread is not None:
            self._thread.join(timeout=1.0)


def _validate_config_rollout_request(payload: object) -> tuple[RiskConfigSpec, list[int]]:
    if not isinstance(payload, dict):
        raise ValueError("request body must be an object")
    target_node_ids = payload.get("target_node_ids")
    if not isinstance(target_node_ids, list) or not target_node_ids:
        raise ValueError("target_node_ids must be a non-empty array")
    validated_targets = []
    for index, node_id in enumerate(target_node_ids):
        if isinstance(node_id, bool) or not isinstance(node_id, int) or node_id < 0 or node_id > 0xFFFF:
            raise ValueError(f"target_node_ids[{index}] must be a uint16")
        validated_targets.append(node_id)

    spec = RiskConfigSpec(
        l2_temp_thresh=_require_int(payload, "l2_temp_thresh", minimum=-0x8000, maximum=0x7FFF),
        l2_humidity_thresh=_require_int(payload, "l2_humidity_thresh", minimum=0, maximum=0xFFFF),
        l2_voc_thresh=_require_int(payload, "l2_voc_thresh", minimum=0, maximum=0xFFFF),
        l3_temp_thresh=_require_int(payload, "l3_temp_thresh", minimum=-0x8000, maximum=0x7FFF),
        l3_humidity_thresh=_require_int(payload, "l3_humidity_thresh", minimum=0, maximum=0xFFFF),
        l3_voc_thresh=_require_int(payload, "l3_voc_thresh", minimum=0, maximum=0xFFFF),
        l4_voc_thresh=_require_int(payload, "l4_voc_thresh", minimum=0, maximum=0xFFFF),
        l5_voc_thresh=_require_int(payload, "l5_voc_thresh", minimum=0, maximum=0xFFFF),
        l5_pm25_thresh=_require_int(payload, "l5_pm25_thresh", minimum=0, maximum=0xFFFF),
        created_by=_optional_text(payload.get("created_by")),
        source=_optional_text(payload.get("source")),
        comment=_optional_text(payload.get("comment")),
    )
    return spec, validated_targets


def _require_int(payload: dict, key: str, *, minimum: int, maximum: int) -> int:
    if key not in payload:
        raise ValueError(f"missing field: {key}")
    value = payload[key]
    if isinstance(value, bool) or not isinstance(value, int):
        raise ValueError(f"field {key} must be an integer")
    if value < minimum or value > maximum:
        raise ValueError(f"field {key} out of range")
    return value


def _optional_text(value: object) -> str | None:
    if value is None:
        return None
    if not isinstance(value, str):
        raise ValueError("optional metadata fields must be strings")
    return value
