from __future__ import annotations

import json

from .control_attempt_store import ControlAttemptStore
from .control_gateway_client import ControlGatewayClient
from .scheduler_store import SchedulerStore


class TimeSyncScheduler:
    def __init__(
        self,
        *,
        transaction_manager,
        node_store,
        scheduler_store: SchedulerStore,
        control_attempt_store: ControlAttemptStore,
        gateway_client: ControlGatewayClient,
        active_node_window_seconds: int,
    ) -> None:
        self._transaction_manager = transaction_manager
        self._node_store = node_store
        self._scheduler_store = scheduler_store
        self._control_attempt_store = control_attempt_store
        self._gateway_client = gateway_client
        self._active_node_window_seconds = active_node_window_seconds

    def run_once(self, *, now: int) -> int:
        active_nodes = self._node_store.list_recently_seen(now - self._active_node_window_seconds)
        with self._transaction_manager.transaction() as connection:
            run_id = self._scheduler_store.start_run(
                job_type="time-sync",
                started_at=now,
                details=f"target_count={len(active_nodes)}",
                connection=connection,
            )

        for node in active_nodes:
            result = self._gateway_client.send_time_sync(target_node_id=node.node_id, epoch=now)
            request_body = json.dumps(
                {
                    "target_node_id": node.node_id,
                    "epoch": now,
                },
                sort_keys=True,
            )
            with self._transaction_manager.transaction() as connection:
                self._control_attempt_store.record_attempt(
                    request_type="time-sync",
                    target_node_id=node.node_id,
                    request_body=request_body,
                    gateway_url=self._gateway_client.base_url,
                    gateway_id=None,
                    scheduler_run_id=run_id,
                    response_status=result.status_code,
                    response_body=result.response_body,
                    delivery_result=result.delivery_result,
                    created_at=now,
                    completed_at=now,
                    connection=connection,
                )

        with self._transaction_manager.transaction() as connection:
            self._scheduler_store.complete_run(
                run_id=run_id,
                completed_at=now,
                status="completed",
                details=f"target_count={len(active_nodes)}",
                connection=connection,
            )
        return run_id
