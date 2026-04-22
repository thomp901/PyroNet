from __future__ import annotations

import http.server
import json
import socket
import threading
import time


ROUTES = {
    "/api/v1/downlinks/nn-table": "nn-table",
    "/api/v1/downlinks/time-sync": "time-sync",
    "/api/v1/downlinks/config": "config",
}


class DownlinkHttpApi:
    def __init__(self, *, delivery_service, max_request_body_bytes: int) -> None:
        self._delivery_service = delivery_service
        self._max_request_body_bytes = max_request_body_bytes

    def handle_request(self, *, method: str, path: str, body: bytes, now: int) -> tuple[int, dict[str, str], bytes]:
        if method != "POST":
            return self._json_response(405, {"delivery_result": "invalid_request", "error_detail": "method not allowed"})
        if path not in ROUTES:
            return self._json_response(404, {"delivery_result": "invalid_request", "error_detail": "unknown endpoint"})
        if len(body) > self._max_request_body_bytes:
            return self._json_response(400, {"delivery_result": "invalid_request", "error_detail": "request body too large"})
        result = self._delivery_service.handle_request(
            request_type=ROUTES[path],
            request_body=body,
            now=now,
        )
        return self._json_response(result.status_code, result.body)

    def _json_response(self, status_code: int, body: dict) -> tuple[int, dict[str, str], bytes]:
        encoded = json.dumps(body, sort_keys=True).encode("utf-8")
        return status_code, {"Content-Type": "application/json", "Content-Length": str(len(encoded))}, encoded


class _ThreadingHttpServer(http.server.ThreadingHTTPServer):
    daemon_threads = True


class _ThreadingIPv6HttpServer(_ThreadingHttpServer):
    address_family = socket.AF_INET6


class _DownlinkRequestHandler(http.server.BaseHTTPRequestHandler):
    def do_POST(self) -> None:
        self._handle()

    def do_GET(self) -> None:
        self._handle()

    def _handle(self) -> None:
        content_length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(content_length) if content_length > 0 else b""
        status, headers, response_body = self.server.downlink_http_api.handle_request(
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


class DownlinkHttpServer:
    def __init__(self, *, bind_host: str, port: int, downlink_http_api: DownlinkHttpApi) -> None:
        server_class = _ThreadingIPv6HttpServer if ":" in bind_host else _ThreadingHttpServer
        self._server = server_class((bind_host, port), _DownlinkRequestHandler)
        self._server.downlink_http_api = downlink_http_api
        self._thread: threading.Thread | None = None

    def start(self) -> None:
        self._thread = threading.Thread(target=self._server.serve_forever, name="pyronet-http", daemon=True)
        self._thread.start()

    def close(self) -> None:
        self._server.shutdown()
        self._server.server_close()
        if self._thread is not None:
            self._thread.join(timeout=1.0)
