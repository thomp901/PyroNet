from __future__ import annotations

import threading

from .downlink_packet_codec import validate_node_downlink_payload
from .protocol.backhaul import (
    DOWNLINK_STATUS_DELIVERED,
    DOWNLINK_STATUS_MESH_DELIVERY_FAILED,
    DOWNLINK_STATUS_PERMANENT_REJECT,
    DOWNLINK_STATUS_UNKNOWN_NODE,
    BackhaulParseError,
    DownlinkRequest,
    DownlinkRequestIdentity,
    DownlinkResult,
    decode_downlink_request_identity,
)


class DownlinkDeliveryService:
    def __init__(
        self,
        *,
        gateway_id: int,
        backhaul_version: int,
        node_state_store,
        result_store,
        coap_downlink_client,
    ) -> None:
        self._gateway_id = gateway_id
        self._backhaul_version = backhaul_version
        self._node_state_store = node_state_store
        self._result_store = result_store
        self._coap_downlink_client = coap_downlink_client
        self._inflight_lock = threading.Lock()
        self._inflight: dict[tuple[int, int], threading.Event] = {}

    def handle_request(self, *, request_body: bytes, now: int) -> tuple[int, bytes]:
        identity = decode_downlink_request_identity(request_body)
        if identity is None:
            return 400, b""

        cached = self._result_store.get_terminal_result(
            gateway_id=identity.gateway_id,
            downlink_id=identity.downlink_id,
        )
        if cached is not None and cached.request_body == request_body:
            return 200, cached.result_body

        owner, event = self._acquire_owner(identity)
        if not owner:
            event.wait()
            cached = self._result_store.get_terminal_result(
                gateway_id=identity.gateway_id,
                downlink_id=identity.downlink_id,
            )
            if cached is not None and cached.request_body == request_body:
                return 200, cached.result_body
            return self.handle_request(request_body=request_body, now=now)

        try:
            cached = self._result_store.get_terminal_result(
                gateway_id=identity.gateway_id,
                downlink_id=identity.downlink_id,
            )
            if cached is not None and cached.request_body == request_body:
                return 200, cached.result_body
            return self._process_request(identity=identity, request_body=request_body, now=now)
        finally:
            self._release_owner(identity, event)

    def _process_request(self, *, identity: DownlinkRequestIdentity, request_body: bytes, now: int) -> tuple[int, bytes]:
        try:
            request = DownlinkRequest.from_bytes(request_body)
        except BackhaulParseError:
            return self._terminal_response(
                identity=identity,
                request_body=request_body,
                status=DOWNLINK_STATUS_PERMANENT_REJECT,
                now=now,
            )

        if request.version != self._backhaul_version:
            return self._terminal_response(
                identity=identity,
                request_body=request_body,
                status=DOWNLINK_STATUS_PERMANENT_REJECT,
                now=now,
            )

        if request.gateway_id != self._gateway_id:
            return self._terminal_response(
                identity=identity,
                request_body=request_body,
                status=DOWNLINK_STATUS_PERMANENT_REJECT,
                now=now,
            )

        try:
            payload_info = validate_node_downlink_payload(request.payload)
        except ValueError:
            return self._terminal_response(
                identity=identity,
                request_body=request_body,
                status=DOWNLINK_STATUS_PERMANENT_REJECT,
                now=now,
            )

        if payload_info.target_node_id is not None and payload_info.target_node_id != request.target_node_id:
            return self._terminal_response(
                identity=identity,
                request_body=request_body,
                status=DOWNLINK_STATUS_PERMANENT_REJECT,
                now=now,
            )

        target = self._node_state_store.get(request.target_node_id)
        if target is None:
            return self._terminal_response(
                identity=identity,
                request_body=request_body,
                status=DOWNLINK_STATUS_UNKNOWN_NODE,
                now=now,
            )

        try:
            result = self._coap_downlink_client.send_confirmable(
                target_ipv6=target.current_ipv6,
                payload=request.payload,
            )
        except Exception:
            return 503, b""

        if result.success:
            return self._terminal_response(
                identity=identity,
                request_body=request_body,
                status=DOWNLINK_STATUS_DELIVERED,
                now=now,
            )

        if result.error_category == "internal_gateway_failure":
            return 503, b""

        return self._terminal_response(
            identity=identity,
            request_body=request_body,
            status=DOWNLINK_STATUS_MESH_DELIVERY_FAILED,
            now=now,
        )

    def _terminal_response(
        self,
        *,
        identity: DownlinkRequestIdentity,
        request_body: bytes,
        status: int,
        now: int,
    ) -> tuple[int, bytes]:
        result = DownlinkResult(
            version=identity.version,
            gateway_id=identity.gateway_id,
            downlink_id=identity.downlink_id,
            target_node_id=identity.target_node_id,
            status=status,
            completed_at=now,
        )
        record = self._result_store.store_terminal_result(
            gateway_id=identity.gateway_id,
            downlink_id=identity.downlink_id,
            request_version=identity.version,
            target_node_id=identity.target_node_id,
            request_body=request_body,
            status=status,
            result_body=result.to_bytes(),
            created_at=identity.created_at,
            completed_at=now,
        )
        return 200, record.result_body

    def _acquire_owner(self, identity: DownlinkRequestIdentity) -> tuple[bool, threading.Event]:
        key = (identity.gateway_id, identity.downlink_id)
        with self._inflight_lock:
            event = self._inflight.get(key)
            if event is not None:
                return False, event
            event = threading.Event()
            self._inflight[key] = event
            return True, event

    def _release_owner(self, identity: DownlinkRequestIdentity, event: threading.Event) -> None:
        key = (identity.gateway_id, identity.downlink_id)
        with self._inflight_lock:
            current = self._inflight.get(key)
            if current is event:
                self._inflight.pop(key, None)
                event.set()
