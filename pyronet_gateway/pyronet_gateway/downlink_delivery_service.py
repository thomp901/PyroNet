from __future__ import annotations

import json
from dataclasses import dataclass

from .downlink_packet_codec import encode_config_update, encode_nn_table_update, encode_time_sync
from .downlink_request_validation import (
    DownlinkValidationError,
    validate_config_update_request,
    validate_nn_table_request,
    validate_time_sync_request,
)


@dataclass(frozen=True)
class DownlinkHttpResult:
    status_code: int
    body: dict


class DownlinkDeliveryService:
    def __init__(
        self,
        *,
        node_state_store,
        audit_store,
        coap_downlink_client,
        node_packet_version: int,
    ) -> None:
        self._node_state_store = node_state_store
        self._audit_store = audit_store
        self._coap_downlink_client = coap_downlink_client
        self._node_packet_version = node_packet_version

    def handle_request(self, *, request_type: str, request_body: bytes, now: int) -> DownlinkHttpResult:
        try:
            decoded = json.loads(request_body.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            attempt_id = self._audit_store.create_attempt(
                request_type=request_type,
                target_node_id=None,
                target_ipv6=None,
                request_body=request_body,
                encoded_payload=None,
                status="invalid_request",
                error_category="invalid_request",
                error_detail=str(exc),
                created_at=now,
                completed_at=now,
            )
            return DownlinkHttpResult(
                400,
                {
                    "attempt_id": attempt_id,
                    "request_type": request_type,
                    "delivery_result": "invalid_request",
                    "error_detail": str(exc),
                },
            )
        if not isinstance(decoded, dict):
            attempt_id = self._audit_store.create_attempt(
                request_type=request_type,
                target_node_id=None,
                target_ipv6=None,
                request_body=request_body,
                encoded_payload=None,
                status="invalid_request",
                error_category="invalid_request",
                error_detail="request body must be a JSON object",
                created_at=now,
                completed_at=now,
            )
            return DownlinkHttpResult(
                400,
                {
                    "attempt_id": attempt_id,
                    "request_type": request_type,
                    "delivery_result": "invalid_request",
                    "error_detail": "request body must be a JSON object",
                },
            )

        try:
            if request_type == "nn-table":
                validated = validate_nn_table_request(decoded)
            elif request_type == "time-sync":
                validated = validate_time_sync_request(decoded)
            elif request_type == "config":
                validated = validate_config_update_request(decoded)
            else:
                raise DownlinkValidationError(f"unsupported request type {request_type}")
        except DownlinkValidationError as exc:
            attempt_id = self._audit_store.create_attempt(
                request_type=request_type,
                target_node_id=decoded.get("target_node_id") if isinstance(decoded, dict) else None,
                target_ipv6=None,
                request_body=request_body,
                encoded_payload=None,
                status="invalid_request",
                error_category="invalid_request",
                error_detail=str(exc),
                created_at=now,
                completed_at=now,
            )
            return DownlinkHttpResult(
                400,
                {
                    "attempt_id": attempt_id,
                    "request_type": request_type,
                    "target_node_id": decoded.get("target_node_id") if isinstance(decoded, dict) else None,
                    "delivery_result": "invalid_request",
                    "error_detail": str(exc),
                },
            )

        target = self._node_state_store.get(validated.target_node_id)
        if target is None:
            attempt_id = self._audit_store.create_attempt(
                request_type=request_type,
                target_node_id=validated.target_node_id,
                target_ipv6=None,
                request_body=request_body,
                encoded_payload=None,
                status="target_unknown",
                error_category="target_unknown",
                error_detail="no current IPv6 mapping for target node",
                created_at=now,
                completed_at=now,
            )
            return DownlinkHttpResult(
                404,
                {
                    "attempt_id": attempt_id,
                    "request_type": request_type,
                    "target_node_id": validated.target_node_id,
                    "delivery_result": "target_unknown",
                },
            )

        if request_type == "nn-table":
            neighbor_records = self._node_state_store.get_many(validated.neighbor_node_ids)
            missing = [node_id for node_id in validated.neighbor_node_ids if node_id not in neighbor_records]
            if missing:
                attempt_id = self._audit_store.create_attempt(
                    request_type=request_type,
                    target_node_id=validated.target_node_id,
                    target_ipv6=target.current_ipv6,
                    request_body=request_body,
                    encoded_payload=None,
                    status="stale_precondition",
                    error_category="unresolved_neighbor_set",
                    error_detail=f"missing neighbor mappings: {missing}",
                    created_at=now,
                    completed_at=now,
                )
                return DownlinkHttpResult(
                    409,
                    {
                        "attempt_id": attempt_id,
                        "request_type": request_type,
                        "target_node_id": validated.target_node_id,
                        "target_ipv6": target.current_ipv6,
                        "delivery_result": "stale_precondition",
                        "missing_neighbor_node_ids": missing,
                    },
                )
            payload = encode_nn_table_update(
                version=self._node_packet_version,
                target_node_id=validated.target_node_id,
                neighbor_ipv6s=[neighbor_records[node_id].current_ipv6 for node_id in validated.neighbor_node_ids],
            )
        elif request_type == "time-sync":
            payload = encode_time_sync(version=self._node_packet_version, epoch=validated.epoch)
        else:
            payload = encode_config_update(version=self._node_packet_version, request=validated)

        attempt_id = self._audit_store.create_attempt(
            request_type=request_type,
            target_node_id=validated.target_node_id,
            target_ipv6=target.current_ipv6,
            request_body=request_body,
            encoded_payload=payload,
            status="in_progress",
            error_category=None,
            error_detail=None,
            created_at=now,
        )

        try:
            result = self._coap_downlink_client.send_confirmable(
                target_ipv6=target.current_ipv6,
                payload=payload,
            )
        except Exception as exc:
            self._audit_store.complete_attempt(
                attempt_id,
                target_ipv6=target.current_ipv6,
                encoded_payload=payload,
                status="gateway_failure",
                error_category="internal_gateway_failure",
                error_detail=str(exc),
                completed_at=now,
            )
            return DownlinkHttpResult(
                503,
                {
                    "attempt_id": attempt_id,
                    "request_type": request_type,
                    "target_node_id": validated.target_node_id,
                    "target_ipv6": target.current_ipv6,
                    "delivery_result": "transient_gateway_internal_failure",
                },
            )

        if result.success:
            self._audit_store.complete_attempt(
                attempt_id,
                target_ipv6=target.current_ipv6,
                encoded_payload=payload,
                status="accepted_and_delivered",
                error_category=None,
                error_detail=None,
                completed_at=now,
            )
            return DownlinkHttpResult(
                200,
                {
                    "attempt_id": attempt_id,
                    "request_type": request_type,
                    "target_node_id": validated.target_node_id,
                    "target_ipv6": target.current_ipv6,
                    "delivery_result": "accepted_and_delivered",
                },
            )

        status_code = 503 if result.error_category == "internal_gateway_failure" else 502
        delivery_result = (
            "transient_gateway_internal_failure"
            if status_code == 503
            else "target_stale_or_unreachable"
        )
        self._audit_store.complete_attempt(
            attempt_id,
            target_ipv6=target.current_ipv6,
            encoded_payload=payload,
            status=delivery_result,
            error_category=result.error_category,
            error_detail=result.error_detail,
            completed_at=now,
        )
        return DownlinkHttpResult(
            status_code,
            {
                "attempt_id": attempt_id,
                "request_type": request_type,
                "target_node_id": validated.target_node_id,
                "target_ipv6": target.current_ipv6,
                "delivery_result": delivery_result,
                "error_category": result.error_category,
                "error_detail": result.error_detail,
            },
        )
