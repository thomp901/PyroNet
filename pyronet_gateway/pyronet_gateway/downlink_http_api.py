from __future__ import annotations

import http.server
import logging
import socket
import threading
import time

from .protocol.backhaul import DownlinkResult, decode_downlink_request_identity

ROUTE = "/api/v1/downlinks"

LOGGER = logging.getLogger(__name__)

_DOWNLINK_STATUS_NAMES = {
    0x00: "delivered",
    0x01: "unknown_node",
    0x02: "mesh_delivery_failed",
    0x03: "permanent_reject",
}


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
        remote_addr: str | None = None,
    ) -> tuple[int, dict[str, str], bytes]:
        request_identity = decode_downlink_request_identity(body)
        if path == ROUTE:
            LOGGER.info(
                "downlink_http recv remote=%s method=%s content_type=%s body_bytes=%d %s",
                remote_addr or "-",
                method,
                self._normalize_content_type(content_type) or "-",
                len(body),
                self._format_request_identity(request_identity),
            )
        if method != "POST":
            return self._log_response(
                remote_addr=remote_addr,
                request_identity=request_identity,
                response=self._response(405, None, b""),
            )
        if path != ROUTE:
            return self._response(404, None, b"")
        if len(body) > self._max_request_body_bytes:
            return self._log_response(
                remote_addr=remote_addr,
                request_identity=request_identity,
                response=self._response(413, None, b""),
            )
        if self._normalize_content_type(content_type) != "application/octet-stream":
            return self._log_response(
                remote_addr=remote_addr,
                request_identity=request_identity,
                response=self._response(415, None, b""),
            )

        status_code, response_body = self._delivery_service.handle_request(request_body=body, now=now)
        if status_code == 200:
            return self._log_response(
                remote_addr=remote_addr,
                request_identity=request_identity,
                response=self._response(200, "application/octet-stream", response_body),
            )
        return self._log_response(
            remote_addr=remote_addr,
            request_identity=request_identity,
            response=self._response(status_code, None, response_body),
        )

    def _normalize_content_type(self, content_type: str | None) -> str:
        if content_type is None:
            return ""
        return content_type.split(";", 1)[0].strip().lower()

    def _response(self, status_code: int, content_type: str | None, body: bytes) -> tuple[int, dict[str, str], bytes]:
        headers = {"Content-Length": str(len(body))}
        if content_type is not None:
            headers["Content-Type"] = content_type
        return status_code, headers, body

    def _log_response(
        self,
        *,
        remote_addr: str | None,
        request_identity,
        response: tuple[int, dict[str, str], bytes],
    ) -> tuple[int, dict[str, str], bytes]:
        status_code, headers, body = response
        content_type = headers.get("Content-Type")
        if request_identity is not None or status_code != 404:
            LOGGER.info(
                "downlink_http done remote=%s http_status=%d response_bytes=%d terminal_status=%s %s",
                remote_addr or "-",
                status_code,
                len(body),
                self._format_terminal_status(content_type=content_type, body=body),
                self._format_request_identity(request_identity),
            )
        return response

    def _format_request_identity(self, request_identity) -> str:
        if request_identity is None:
            return "request=unparsed"
        return (
            f"gateway_id={request_identity.gateway_id} "
            f"downlink_id={request_identity.downlink_id} "
            f"target_node_id={request_identity.target_node_id} "
            f"version={request_identity.version} "
            f"created_at={request_identity.created_at}"
        )

    def _format_terminal_status(self, *, content_type: str | None, body: bytes) -> str:
        if content_type != "application/octet-stream" or not body:
            return "-"
        try:
            result = DownlinkResult.from_bytes(body)
        except Exception:
            return "unparsed"
        return _DOWNLINK_STATUS_NAMES.get(result.status, f"0x{result.status:02x}")


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
            remote_addr=self.address_string(),
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
