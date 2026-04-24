from __future__ import annotations

import ipaddress
import os
import socket
import time
from dataclasses import dataclass

COAP_TYPE_CON = 0
COAP_TYPE_ACK = 2
COAP_CODE_EMPTY = 0
COAP_METHOD_POST = 2


@dataclass(frozen=True)
class CoapDeliveryResult:
    success: bool
    error_category: str | None
    error_detail: str | None
    attempts: int
    response_code: int | None = None


class CoapDownlinkClient:
    def __init__(
        self,
        *,
        port: int,
        resource_path: str,
        ack_timeout_seconds: float,
        max_retransmit: int,
        link_local_interface: str = "tun0",
        socket_factory=socket.socket,
        monotonic=time.monotonic,
    ) -> None:
        self._port = port
        self._resource_path = tuple(segment for segment in resource_path.strip("/").split("/") if segment)
        self._ack_timeout_seconds = ack_timeout_seconds
        self._max_retransmit = max_retransmit
        self._link_local_interface = link_local_interface
        self._socket_factory = socket_factory
        self._monotonic = monotonic
        self._message_id = 0

    def send_confirmable(self, *, target_ipv6: str, payload: bytes) -> CoapDeliveryResult:
        token = os.urandom(4)
        message_id = self._next_message_id()
        datagram = self._build_post(
            message_id=message_id,
            token=token,
            payload=payload,
        )
        timeout_seconds = self._ack_timeout_seconds
        attempts = 0
        try:
            sock = self._socket_factory(socket.AF_INET6, socket.SOCK_DGRAM)
        except OSError as exc:
            return CoapDeliveryResult(False, "internal_gateway_failure", str(exc), attempts=0)
        destination = self._destination(target_ipv6)
        try:
            for attempt in range(self._max_retransmit + 1):
                attempts = attempt + 1
                try:
                    sock.sendto(datagram, destination)
                except OSError as exc:
                    return CoapDeliveryResult(False, "target_unreachable", str(exc), attempts=attempts)
                result = self._await_response(sock, timeout_seconds, token=token, message_id=message_id)
                if result is not None:
                    return result
                timeout_seconds *= 2
            return CoapDeliveryResult(
                False,
                "target_stale_or_unreachable",
                "coap retry exhaustion",
                attempts=attempts,
            )
        finally:
            try:
                sock.close()
            except Exception:
                pass

    def _await_response(self, sock, timeout_seconds: float, *, token: bytes, message_id: int) -> CoapDeliveryResult | None:
        deadline = self._monotonic() + timeout_seconds
        while True:
            remaining = deadline - self._monotonic()
            if remaining <= 0:
                return None
            sock.settimeout(remaining)
            try:
                datagram, _address = sock.recvfrom(4096)
            except socket.timeout:
                return None
            except TimeoutError:
                return None
            except OSError as exc:
                return CoapDeliveryResult(False, "target_unreachable", str(exc), attempts=1)
            try:
                message = self._parse_message(datagram)
            except ValueError as exc:
                return CoapDeliveryResult(False, "coap_protocol_error", str(exc), attempts=1)
            if message["token"] != token or message["message_id"] != message_id:
                continue
            if message["code"] == COAP_CODE_EMPTY and message["msg_type"] == COAP_TYPE_ACK:
                return CoapDeliveryResult(True, None, None, attempts=1, response_code=message["code"])
            if 64 <= message["code"] < 96:
                return CoapDeliveryResult(True, None, None, attempts=1, response_code=message["code"])
            return CoapDeliveryResult(
                False,
                "target_stale_or_unreachable",
                f"coap response code {message['code']}",
                attempts=1,
                response_code=message["code"],
            )

    def _build_post(self, *, message_id: int, token: bytes, payload: bytes) -> bytes:
        first = (1 << 6) | (COAP_TYPE_CON << 4) | len(token)
        header = bytes([first, COAP_METHOD_POST]) + message_id.to_bytes(2, "big") + token
        options = b""
        previous_number = 0
        for segment in self._resource_path:
            option_number = 11
            delta = option_number - previous_number
            value = segment.encode("utf-8")
            if delta > 12 or len(value) > 12:
                raise ValueError("downlink resource path segments must be 12 bytes or shorter")
            options += bytes([(delta << 4) | len(value)]) + value
            previous_number = option_number
        return header + options + b"\xFF" + payload

    def _parse_message(self, datagram: bytes) -> dict:
        if len(datagram) < 4:
            raise ValueError("CoAP response shorter than header")
        version = datagram[0] >> 6
        if version != 1:
            raise ValueError(f"unsupported CoAP version {version}")
        token_length = datagram[0] & 0x0F
        if len(datagram) < 4 + token_length:
            raise ValueError("CoAP response shorter than token")
        return {
            "msg_type": (datagram[0] >> 4) & 0x03,
            "code": datagram[1],
            "message_id": int.from_bytes(datagram[2:4], "big"),
            "token": datagram[4 : 4 + token_length],
        }

    def _next_message_id(self) -> int:
        self._message_id = (self._message_id + 1) & 0xFFFF
        return self._message_id

    def _destination(self, target_ipv6: str):
        address = ipaddress.IPv6Address(target_ipv6)
        if not address.is_link_local:
            return (target_ipv6, self._port)
        scope_id = socket.if_nametoindex(self._link_local_interface)
        return (target_ipv6, self._port, 0, scope_id)
