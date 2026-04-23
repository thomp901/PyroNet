from __future__ import annotations

import json

from .control_attempt_store import ControlAttemptStore
from .control_gateway_client import ControlGatewayClient
from .neighbor_compute_service import NeighborComputeService
from .neighbor_store import NeighborStore


class NeighborPushService:
    def __init__(
        self,
        *,
        transaction_manager,
        node_store,
        neighbor_store: NeighborStore,
        control_attempt_store: ControlAttemptStore,
        gateway_client: ControlGatewayClient,
        compute_service: NeighborComputeService,
    ) -> None:
        self._transaction_manager = transaction_manager
        self._node_store = node_store
        self._neighbor_store = neighbor_store
        self._control_attempt_store = control_attempt_store
        self._gateway_client = gateway_client
        self._compute_service = compute_service

    def recompute_and_push(self, *, now: int) -> dict[int, list[int]]:
        nodes = self._node_store.list_with_coordinates()
        computed_sets = self._compute_service.compute(nodes)
        changed_sets = {}
        for node_id, computation in computed_sets.items():
            current = self._neighbor_store.get_neighbor_set(node_id)
            if current is not None and current.neighbor_node_ids == computation.neighbor_node_ids:
                continue
            changed_sets[node_id] = computation.neighbor_node_ids
            result = self._gateway_client.send_nn_table(
                target_node_id=node_id,
                neighbor_node_ids=computation.neighbor_node_ids,
            )
            request_body = json.dumps(
                {
                    "target_node_id": node_id,
                    "neighbor_node_ids": computation.neighbor_node_ids,
                },
                sort_keys=True,
            )
            with self._transaction_manager.transaction() as connection:
                self._control_attempt_store.record_attempt(
                    request_type="nn-table",
                    target_node_id=node_id,
                    request_body=request_body,
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
                if result.delivery_result == "accepted_and_delivered":
                    self._neighbor_store.upsert_neighbor_set(
                        node_id=node_id,
                        neighbor_node_ids=computation.neighbor_node_ids,
                        computed_at=now,
                        radius_meters=self._compute_service.radius_meters,
                        max_neighbors=self._compute_service.max_neighbors,
                        connection=connection,
                    )
        return changed_sets
