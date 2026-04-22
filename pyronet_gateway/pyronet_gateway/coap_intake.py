from __future__ import annotations

from collections import OrderedDict
import socket
import socketserver
import threading
import time
from dataclasses import dataclass

from .protocol.node_packets import PacketParseError, TYPE_NEIGHBOR_ALERT, packet_type

COAP_TYPE_CON = 0
COAP_TYPE_NON = 1
COAP_TYPE_ACK = 2
COAP_METHOD_POST = 2
COAP_CODE_CHANGED = (2 << 5) | 4
COAP_CODE_BAD_REQUEST = (4 << 5) | 0
COAP_CODE_NOT_FOUND = (4 << 5) | 4
COAP_CODE_METHOD_NOT_ALLOWED = (4 << 5) | 5


class CoapParseError(ValueError):
    """Raised when a CoAP datagram cannot be decoded."""


@dataclass(frozen=True)
class CoapMessage:
    msg_type: int
    code: int
    message_id: int
    token: bytes
    uri_path: tuple[str, ...]
    payload: bytes


@dataclass(frozen=True)
class CoapIntakeResult:
    accepted: bool
    reason: str
    response_bytes: bytes | None
    uplink_id: int | None = None


@dataclass(frozen=True)
class _CachedDuplicate:
    result: CoapIntakeResult
    expires_at: float


def parse_coap_message(datagram: bytes) -> CoapMessage:
    if len(datagram) < 4:
        raise CoapParseError("CoAP datagram shorter than header")

    first = datagram[0]
    version = first >> 6
    msg_type = (first >> 4) & 0x03
    token_length = first & 0x0F
    if version != 1:
        raise CoapParseError(f"unsupported CoAP version {version}")
    if len(datagram) < 4 + token_length:
        raise CoapParseError("CoAP datagram shorter than token length")

    code = datagram[1]
    message_id = int.from_bytes(datagram[2:4], "big")
    token = datagram[4 : 4 + token_length]
    index = 4 + token_length
    option_number = 0
    uri_segments: list[str] = []
    payload = b""

    while index < len(datagram):
        current = datagram[index]
        if current == 0xFF:
            payload = datagram[index + 1 :]
            break
        index += 1
        delta, index = _decode_option_nibble(current >> 4, datagram, index)
        length, index = _decode_option_nibble(current & 0x0F, datagram, index)
        option_number += delta
        if index + length > len(datagram):
            raise CoapParseError("CoAP option overruns datagram")
        value = datagram[index : index + length]
        if option_number == 11:
            uri_segments.append(value.decode("utf-8"))
        index += length

    return CoapMessage(
        msg_type=msg_type,
        code=code,
        message_id=message_id,
        token=token,
        uri_path=tuple(uri_segments),
        payload=payload,
    )


def build_coap_response(request: CoapMessage, code: int) -> bytes:
    response_type = COAP_TYPE_ACK if request.msg_type == COAP_TYPE_CON else COAP_TYPE_NON
    first = (1 << 6) | (response_type << 4) | len(request.token)
    return bytes([first, code]) + request.message_id.to_bytes(2, "big") + request.token


def _decode_option_nibble(nibble: int, data: bytes, index: int) -> tuple[int, int]:
    if nibble <= 12:
        return nibble, index
    if nibble == 13:
        if index >= len(data):
            raise CoapParseError("truncated extended option")
        return 13 + data[index], index + 1
    if nibble == 14:
        if index + 1 >= len(data):
            raise CoapParseError("truncated 2-byte extended option")
        return 269 + int.from_bytes(data[index : index + 2], "big"), index + 2
    raise CoapParseError("option nibble 15 is reserved")


class CoapIntakeService:
    def __init__(
        self,
        *,
        gateway_service,
        resource_path: str,
        duplicate_cache_ttl_seconds: float = 60.0,
        duplicate_cache_max_entries: int = 1024,
        time_fn=time.time,
    ) -> None:
        self._gateway_service = gateway_service
        self._resource_path = tuple(segment for segment in resource_path.strip("/").split("/") if segment)
        self._duplicate_cache_ttl_seconds = duplicate_cache_ttl_seconds
        self._duplicate_cache_max_entries = duplicate_cache_max_entries
        self._time_fn = time_fn
        self._duplicate_cache: OrderedDict[tuple[str, int, bytes, bytes], _CachedDuplicate] = OrderedDict()
        self._duplicate_cache_lock = threading.RLock()

    def handle_datagram(self, *, src_ipv6: str, datagram: bytes, received_at: int | None = None) -> CoapIntakeResult:
        received_at = received_at if received_at is not None else int(time.time())
        try:
            request = parse_coap_message(datagram)
        except CoapParseError as exc:
            return CoapIntakeResult(False, str(exc), None)

        duplicate_key = self._duplicate_key(src_ipv6=src_ipv6, request=request, datagram=datagram)
        if duplicate_key is not None:
            cached = self._get_cached_duplicate(duplicate_key)
            if cached is not None:
                return cached

        if request.code != COAP_METHOD_POST:
            result = CoapIntakeResult(
                False,
                "method not allowed",
                build_coap_response(request, COAP_CODE_METHOD_NOT_ALLOWED),
            )
            self._store_duplicate(duplicate_key, result)
            return result
        if request.uri_path != self._resource_path:
            result = CoapIntakeResult(
                False,
                "resource not found",
                build_coap_response(request, COAP_CODE_NOT_FOUND),
            )
            self._store_duplicate(duplicate_key, result)
            return result
        if not request.payload:
            result = CoapIntakeResult(
                False,
                "empty CoAP payload",
                build_coap_response(request, COAP_CODE_BAD_REQUEST),
            )
            self._store_duplicate(duplicate_key, result)
            return result

        try:
            msg_type = packet_type(request.payload)
            if msg_type == TYPE_NEIGHBOR_ALERT:
                result = CoapIntakeResult(
                    False,
                    "neighbor alert is not accepted at the uplink endpoint",
                    build_coap_response(request, COAP_CODE_BAD_REQUEST),
                )
                self._store_duplicate(duplicate_key, result)
                return result
            decision = self._gateway_service.handle_node_packet(
                observed_src_ipv6=src_ipv6,
                raw_node_packet=request.payload,
                received_at=received_at,
            )
        except PacketParseError as exc:
            result = CoapIntakeResult(
                False,
                str(exc),
                build_coap_response(request, COAP_CODE_BAD_REQUEST),
            )
            self._store_duplicate(duplicate_key, result)
            return result

        result = CoapIntakeResult(
            decision.accepted,
            decision.reason,
            build_coap_response(request, COAP_CODE_CHANGED),
            decision.uplink_id,
        )
        self._store_duplicate(duplicate_key, result)
        return result

    def _duplicate_key(
        self,
        *,
        src_ipv6: str,
        request: CoapMessage,
        datagram: bytes,
    ) -> tuple[str, int, bytes, bytes] | None:
        if request.msg_type != COAP_TYPE_CON:
            return None
        return (src_ipv6, request.message_id, request.token, datagram)

    def _get_cached_duplicate(self, duplicate_key: tuple[str, int, bytes, bytes]) -> CoapIntakeResult | None:
        now = self._time_fn()
        with self._duplicate_cache_lock:
            self._prune_duplicate_cache(now)
            cached = self._duplicate_cache.get(duplicate_key)
            if cached is None:
                return None
            self._duplicate_cache.move_to_end(duplicate_key)
            return cached.result

    def _store_duplicate(
        self,
        duplicate_key: tuple[str, int, bytes, bytes] | None,
        result: CoapIntakeResult,
    ) -> None:
        if duplicate_key is None or result.response_bytes is None or self._duplicate_cache_max_entries <= 0:
            return
        expires_at = self._time_fn() + self._duplicate_cache_ttl_seconds
        with self._duplicate_cache_lock:
            self._prune_duplicate_cache(self._time_fn())
            self._duplicate_cache[duplicate_key] = _CachedDuplicate(result=result, expires_at=expires_at)
            self._duplicate_cache.move_to_end(duplicate_key)
            while len(self._duplicate_cache) > self._duplicate_cache_max_entries:
                self._duplicate_cache.popitem(last=False)

    def _prune_duplicate_cache(self, now: float) -> None:
        expired = [
            duplicate_key
            for duplicate_key, cached in self._duplicate_cache.items()
            if cached.expires_at <= now
        ]
        for duplicate_key in expired:
            self._duplicate_cache.pop(duplicate_key, None)


class _ThreadingUdp6Server(socketserver.ThreadingMixIn, socketserver.UDPServer):
    address_family = socket.AF_INET6
    daemon_threads = True
    allow_reuse_address = True


class _CoapRequestHandler(socketserver.BaseRequestHandler):
    def handle(self) -> None:
        datagram = self.request[0]
        sock = self.request[1]
        result = self.server.coap_intake_service.handle_datagram(
            src_ipv6=self.client_address[0],
            datagram=datagram,
            received_at=int(time.time()),
        )
        if result.response_bytes is not None:
            sock.sendto(result.response_bytes, self.client_address)


class CoapUdpServer:
    def __init__(self, *, bind_host: str, port: int, coap_intake_service: CoapIntakeService) -> None:
        self._server = _ThreadingUdp6Server((bind_host, port), _CoapRequestHandler)
        self._server.coap_intake_service = coap_intake_service
        self._thread: threading.Thread | None = None

    def start(self) -> None:
        self._thread = threading.Thread(target=self._server.serve_forever, name="pyronet-coap", daemon=True)
        self._thread.start()

    def close(self) -> None:
        self._server.shutdown()
        self._server.server_close()
        if self._thread is not None:
            self._thread.join(timeout=1.0)
