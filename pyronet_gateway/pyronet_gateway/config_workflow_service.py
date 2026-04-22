from __future__ import annotations

import json
from dataclasses import dataclass

from .config_store import ConfigStore, RiskConfigRecord
from .control_attempt_store import ControlAttemptStore
from .control_gateway_client import ControlGatewayClient, GatewayControlResult


@dataclass(frozen=True)
class RiskConfigSpec:
    l2_temp_thresh: int
    l2_humidity_thresh: int
    l2_voc_thresh: int
    l3_temp_thresh: int
    l3_humidity_thresh: int
    l3_voc_thresh: int
    l4_voc_thresh: int
    l5_voc_thresh: int
    l5_pm25_thresh: int
    created_by: str | None = None
    source: str | None = None
    comment: str | None = None


@dataclass(frozen=True)
class ConfigWorkflowResult:
    config_id: int
    target_results: list[GatewayControlResult]


class ConfigWorkflowService:
    def __init__(
        self,
        *,
        transaction_manager,
        config_store: ConfigStore,
        control_attempt_store: ControlAttemptStore,
        gateway_client: ControlGatewayClient,
    ) -> None:
        self._transaction_manager = transaction_manager
        self._config_store = config_store
        self._control_attempt_store = control_attempt_store
        self._gateway_client = gateway_client

    def create_and_push(self, *, spec: RiskConfigSpec, target_node_ids: list[int], now: int) -> ConfigWorkflowResult:
        with self._transaction_manager.transaction() as connection:
            config_id = self._config_store.allocate_next_config_id(connection=connection)
            self._config_store.create_config(
                RiskConfigRecord(
                    config_id=config_id,
                    l2_temp_thresh=spec.l2_temp_thresh,
                    l2_humidity_thresh=spec.l2_humidity_thresh,
                    l2_voc_thresh=spec.l2_voc_thresh,
                    l3_temp_thresh=spec.l3_temp_thresh,
                    l3_humidity_thresh=spec.l3_humidity_thresh,
                    l3_voc_thresh=spec.l3_voc_thresh,
                    l4_voc_thresh=spec.l4_voc_thresh,
                    l5_voc_thresh=spec.l5_voc_thresh,
                    l5_pm25_thresh=spec.l5_pm25_thresh,
                    created_at=now,
                    created_by=spec.created_by,
                    source=spec.source,
                    comment=spec.comment,
                ),
                connection=connection,
            )
            self._config_store.upsert_targets(
                config_id=config_id,
                node_ids=target_node_ids,
                desired_state="pending",
                connection=connection,
            )

        results = []
        for node_id in target_node_ids:
            request_body = {
                "target_node_id": node_id,
                "config_id": config_id,
                "l2_temp_thresh": spec.l2_temp_thresh,
                "l2_humidity_thresh": spec.l2_humidity_thresh,
                "l2_voc_thresh": spec.l2_voc_thresh,
                "l3_temp_thresh": spec.l3_temp_thresh,
                "l3_humidity_thresh": spec.l3_humidity_thresh,
                "l3_voc_thresh": spec.l3_voc_thresh,
                "l4_voc_thresh": spec.l4_voc_thresh,
                "l5_voc_thresh": spec.l5_voc_thresh,
                "l5_pm25_thresh": spec.l5_pm25_thresh,
            }
            result = self._gateway_client.send_config_update(request_body=request_body)
            results.append(result)
            result_payload = json.dumps(
                {
                    "status_code": result.status_code,
                    "delivery_result": result.delivery_result,
                    "response_body": result.response_body,
                },
                sort_keys=True,
            )
            with self._transaction_manager.transaction() as connection:
                self._control_attempt_store.record_attempt(
                    request_type="config",
                    target_node_id=node_id,
                    request_body=json.dumps(request_body, sort_keys=True),
                    gateway_url=self._gateway_client.base_url,
                    gateway_id=None,
                    scheduler_run_id=None,
                    response_status=result.status_code,
                    response_body=result.response_body,
                    delivery_result=result.delivery_result,
                    created_at=now,
                    completed_at=now,
                    connection=connection,
                )
                self._config_store.update_target_result(
                    config_id=config_id,
                    node_id=node_id,
                    latest_attempt_status=result.delivery_result,
                    latest_attempt_time=now,
                    latest_result_payload=result_payload,
                    connection=connection,
                )
        return ConfigWorkflowResult(config_id=config_id, target_results=results)
