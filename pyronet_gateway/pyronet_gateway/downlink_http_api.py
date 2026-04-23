from __future__ import annotations

import http.server
import socket
import threading
import time

ROUTE = "/api/v1/downlinks"


class DownlinkHttpApi:
    def __init__(self, *, delivery_service, max_request_body_bytes: int) -> None:
        self._delivery_service = delivery_service
        self._max_request_body_bytes = max_request_body_bytes

    def handle_request(
        self,
        *,
        method: str,
        path: str,
        content_type: str | None,
        body: bytes,
        now: int,
    ) -> tuple[int, dict[str, str], bytes]:
        if method != "POST":
            return self._response(405, None, b"")
        if path != ROUTE:
            return self._response(404, None, b"")
        if len(body) > self._max_request_body_bytes:
            return self._response(413, None, b"")
        if self._normalize_content_type(content_type) != "application/octet-stream":
            return self._response(415, None, b"")

        status_code, response_body = self._delivery_service.handle_request(request_body=body, now=now)
        if status_code == 200:
            return self._response(200, "application/octet-stream", response_body)
        return self._response(status_code, None, response_body)

    def _normalize_content_type(self, content_type: str | None) -> str:
        if content_type is None:
            return ""
        return content_type.split(";", 1)[0].strip().lower()

    def _response(self, status_code: int, content_type: str | None, body: bytes) -> tuple[int, dict[str, str], bytes]:
        headers = {"Content-Length": str(len(body))}
        if content_type is not None:
            headers["Content-Type"] = content_type
        return status_code, headers, body


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
            content_type=self.headers.get("Content-Type"),
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
