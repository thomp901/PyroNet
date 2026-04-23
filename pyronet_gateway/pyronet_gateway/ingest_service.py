from __future__ import annotations

from contextlib import AbstractContextManager
from dataclasses import dataclass
from typing import Protocol

from .alert_store import AlertStore
from .backhaul_binary_codec import (
    durable_ingest_receipt,
    parse_embedded_node_packet,
    parse_gateway_registration,
    parse_node_uplink_envelope,
    permanent_reject_receipt,
    recover_uplink_identity,
)
from .gateway_store import GatewayStore
from .idempotency_store import IdempotencyStore, StoredReceiptRecord
from .node_store import NodeStore
from .protocol.backhaul import NodeUplinkEnvelope
from .protocol.node_packets import (
    ParentUpdatePacket,
    RegistrationPacket,
    SensorPacket,
    TYPE_PARENT_UPDATE,
    TYPE_REGISTRATION,
    TYPE_SENSOR_ALERT,
)
from .sensor_history_store import SensorHistoryStore
from .topology_store import TopologyStore


class TransactionManager(Protocol):
    def transaction(self) -> AbstractContextManager[object]:
        """Open a transaction that commits on success and rolls back on failure."""


@dataclass(frozen=True)
class IngestHttpResponse:
    status_code: int
    headers: dict[str, str]
    body: bytes


class IngestService:
    def __init__(
        self,
        *,
        transaction_manager: TransactionManager,
        idempotency_store: IdempotencyStore,
        gateway_store: GatewayStore,
        node_store: NodeStore,
        sensor_history_store: SensorHistoryStore,
        topology_store: TopologyStore,
        alert_store: AlertStore,
    ) -> None:
        self._transaction_manager = transaction_manager
        self._idempotency_store = idempotency_store
        self._gateway_store = gateway_store
        self._node_store = node_store
        self._sensor_history_store = sensor_history_store
        self._topology_store = topology_store
        self._alert_store = alert_store

    def handle_gateway_registration(self, *, body: bytes, now: int) -> IngestHttpResponse:
        try:
            registration = parse_gateway_registration(body)
        except Exception as exc:
            return self._text_response(400, f"invalid gateway registration: {exc}")

        try:
            with self._transaction_manager.transaction() as connection:
                self._gateway_store.upsert_registration(
                    registration,
                    raw_payload=body,
                    now=now,
                    connection=connection,
                )
        except Exception as exc:
            return self._text_response(503, f"transient registration failure: {exc}")

        return IngestHttpResponse(status_code=204, headers={"Content-Length": "0"}, body=b"")

    def handle_uplink(self, *, body: bytes, now: int) -> IngestHttpResponse:
        try:
            envelope = parse_node_uplink_envelope(body)
        except Exception as exc:
            identity = recover_uplink_identity(body)
            if identity is None:
                return self._text_response(400, f"invalid uplink envelope: {exc}")
            return self._persist_terminal_reject(
                raw_envelope=body,
                envelope=None,
                gateway_id=identity.gateway_id,
                uplink_id=identity.uplink_id,
                version=identity.version,
                reason=f"invalid uplink envelope: {exc}",
                now=now,
            )

        try:
            existing = self._idempotency_store.get_receipt(envelope.gateway_id, envelope.uplink_id)
        except Exception as exc:
            return self._text_response(503, f"transient uplink failure: {exc}")
        if existing is not None:
            return self._receipt_response(existing.receipt_bytes)

        try:
            packet = parse_embedded_node_packet(envelope.payload)
        except Exception as exc:
            return self._persist_terminal_reject(
                raw_envelope=body,
                envelope=envelope,
                gateway_id=envelope.gateway_id,
                uplink_id=envelope.uplink_id,
                version=envelope.version,
                reason=f"invalid embedded node packet: {exc}",
                now=now,
            )

        receipt = durable_ingest_receipt(envelope)
        receipt_bytes = receipt.to_bytes()

        try:
            with self._transaction_manager.transaction() as connection:
                existing = self._idempotency_store.get_receipt(
                    envelope.gateway_id,
                    envelope.uplink_id,
                    connection=connection,
                )
                if existing is not None:
                    return self._receipt_response(existing.receipt_bytes)

                self._gateway_store.ensure_placeholder(
                    envelope.gateway_id,
                    seen_at=envelope.received_at,
                    connection=connection,
                )
                self._idempotency_store.store_envelope(
                    envelope,
                    raw_envelope=body,
                    payload_type=packet.packet_type,
                    node_id=packet.node_id,
                    created_at=now,
                    connection=connection,
                )
                self._apply_domain_updates(packet=packet, envelope=envelope, connection=connection)
                self._idempotency_store.store_receipt(
                    receipt,
                    receipt_bytes=receipt_bytes,
                    detail="durable ingest",
                    created_at=now,
                    connection=connection,
                )
        except Exception as exc:
            return self._text_response(503, f"transient uplink failure: {exc}")

        return self._receipt_response(receipt_bytes)

    def _persist_terminal_reject(
        self,
        *,
        raw_envelope: bytes | None,
        envelope: NodeUplinkEnvelope | None,
        gateway_id: int,
        uplink_id: int,
        version: int,
        reason: str,
        now: int,
    ) -> IngestHttpResponse:
        receipt = permanent_reject_receipt(version=version, gateway_id=gateway_id, uplink_id=uplink_id)
        receipt_bytes = receipt.to_bytes()
        try:
            with self._transaction_manager.transaction() as connection:
                existing = self._idempotency_store.get_receipt(
                    gateway_id,
                    uplink_id,
                    connection=connection,
                )
                if existing is not None:
                    return self._receipt_response(existing.receipt_bytes)
                self._gateway_store.ensure_placeholder(gateway_id, seen_at=now, connection=connection)
                if envelope is not None and raw_envelope is not None:
                    self._idempotency_store.store_envelope(
                        envelope,
                        raw_envelope=raw_envelope,
                        payload_type=envelope.payload[0] if envelope.payload else -1,
                        node_id=None,
                        created_at=now,
                        connection=connection,
                    )
                elif raw_envelope is not None:
                    self._idempotency_store.store_malformed_envelope_reject(
                        gateway_id=gateway_id,
                        uplink_id=uplink_id,
                        version=version,
                        raw_request_body=raw_envelope,
                        reject_reason=reason,
                        created_at=now,
                        connection=connection,
                    )
                self._idempotency_store.store_receipt(
                    receipt,
                    receipt_bytes=receipt_bytes,
                    detail=reason,
                    created_at=now,
                    connection=connection,
                )
        except Exception as exc:
            return self._text_response(503, f"transient uplink failure: {exc}")
        return self._receipt_response(receipt_bytes)

    def _apply_domain_updates(self, *, packet, envelope: NodeUplinkEnvelope, connection: object) -> None:
        if isinstance(packet, RegistrationPacket):
            self._node_store.upsert_registration(packet, envelope=envelope, connection=connection)
            self._topology_store.append_event(
                packet_type=TYPE_REGISTRATION,
                node_id=packet.node_id,
                parent_ipv6=packet.parent_ipv6,
                node_event_time=None,
                envelope=envelope,
                raw_payload=envelope.payload,
                connection=connection,
            )
            return

        if isinstance(packet, SensorPacket):
            self._node_store.upsert_sensor_state(packet, envelope=envelope, connection=connection)
            self._sensor_history_store.append_reading(packet, envelope=envelope, connection=connection)
            if packet.packet_type == TYPE_SENSOR_ALERT:
                self._alert_store.append_alert(packet, envelope=envelope, connection=connection)
            return

        if isinstance(packet, ParentUpdatePacket):
            self._node_store.upsert_parent_update(packet, envelope=envelope, connection=connection)
            self._topology_store.append_event(
                packet_type=TYPE_PARENT_UPDATE,
                node_id=packet.node_id,
                parent_ipv6=packet.parent_ipv6,
                node_event_time=packet.timestamp,
                envelope=envelope,
                raw_payload=envelope.payload,
                connection=connection,
            )
            return

        raise ValueError(f"unsupported packet class {type(packet).__name__}")

    def _receipt_response(self, receipt_bytes: bytes) -> IngestHttpResponse:
        return IngestHttpResponse(
            status_code=200,
            headers={
                "Content-Type": "application/octet-stream",
                "Content-Length": str(len(receipt_bytes)),
            },
            body=receipt_bytes,
        )

    def _text_response(self, status_code: int, message: str) -> IngestHttpResponse:
        body = message.encode("utf-8")
        return IngestHttpResponse(
            status_code=status_code,
            headers={"Content-Type": "text/plain; charset=utf-8", "Content-Length": str(len(body))},
            body=body,
        )
