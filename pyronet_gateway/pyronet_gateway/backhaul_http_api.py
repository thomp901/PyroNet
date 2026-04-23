from __future__ import annotations

import http.server
import socket
import threading
import time


ROUTES = {
    "/api/v1/gateways/register": "gateway-registration",
    "/api/v1/uplinks": "uplinks",
}


class BackhaulHttpApi:
    def __init__(self, *, ingest_service, max_request_body_bytes: int) -> None:
        self._ingest_service = ingest_service
        self._max_request_body_bytes = max_request_body_bytes

    def handle_request(self, *, method: str, path: str, headers: dict[str, str], body: bytes, now: int):
        if method != "POST":
            return self._text_response(405, "method not allowed")
        if path not in ROUTES:
            return self._text_response(404, "unknown endpoint")
        if len(body) > self._max_request_body_bytes:
            return self._text_response(400, "request body too large")
        if headers.get("content-type", "").split(";", 1)[0].strip().lower() != "application/octet-stream":
            return self._text_response(415, "content type must be application/octet-stream")
        if ROUTES[path] == "gateway-registration":
            response = self._ingest_service.handle_gateway_registration(body=body, now=now)
            return response.status_code, response.headers, response.body
        response = self._ingest_service.handle_uplink(body=body, now=now)
        return response.status_code, response.headers, response.body

    def _text_response(self, status_code: int, message: str):
        body = message.encode("utf-8")
        return status_code, {
            "Content-Type": "text/plain; charset=utf-8",
            "Content-Length": str(len(body)),
        }, body


class _ThreadingHttpServer(http.server.ThreadingHTTPServer):
    daemon_threads = True


class _ThreadingIPv6HttpServer(_ThreadingHttpServer):
    address_family = socket.AF_INET6


class _BackhaulRequestHandler(http.server.BaseHTTPRequestHandler):
    def do_POST(self) -> None:
        self._handle()

    def do_GET(self) -> None:
        self._handle()

    def _handle(self) -> None:
        content_length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(content_length) if content_length > 0 else b""
        status, headers, response_body = self.server.backhaul_http_api.handle_request(
            method=self.command,
            path=self.path,
            headers={key.lower(): value for key, value in self.headers.items()},
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


class BackhaulHttpServer:
    def __init__(self, *, bind_host: str, port: int, backhaul_http_api: BackhaulHttpApi) -> None:
        server_class = _ThreadingIPv6HttpServer if ":" in bind_host else _ThreadingHttpServer
        self._server = server_class((bind_host, port), _BackhaulRequestHandler)
        self._server.backhaul_http_api = backhaul_http_api
        self._thread: threading.Thread | None = None

    def start(self) -> None:
        self._thread = threading.Thread(target=self._server.serve_forever, name="pyronet-backhaul-http", daemon=True)
        self._thread.start()

    def close(self) -> None:
        self._server.shutdown()
        self._server.server_close()
        if self._thread is not None:
            self._thread.join(timeout=1.0)
