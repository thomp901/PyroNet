from __future__ import annotations

from .connectivity_event_store import ConnectivityEventStore


class ConnectivityMonitor:
    def __init__(
        self,
        *,
        transaction_manager,
        node_store,
        connectivity_event_store: ConnectivityEventStore,
        stale_after_seconds: int,
    ) -> None:
        self._transaction_manager = transaction_manager
        self._node_store = node_store
        self._connectivity_event_store = connectivity_event_store
        self._stale_after_seconds = stale_after_seconds

    def run_once(self, *, now: int) -> list[tuple[int, str]]:
        transitions = []
        for node in self._node_store.list_for_connectivity():
            next_state = "stale" if (node.last_seen is None or now - node.last_seen > self._stale_after_seconds) else "healthy"
            previous_state = node.connectivity_state
            if previous_state == next_state:
                continue
            if previous_state is None and next_state == "healthy":
                with self._transaction_manager.transaction() as connection:
                    self._node_store.set_connectivity_state(
                        node.node_id,
                        connectivity_state="healthy",
                        updated_at=now,
                        connection=connection,
                    )
                continue
            detail = f"last_seen={node.last_seen}"
            with self._transaction_manager.transaction() as connection:
                self._node_store.set_connectivity_state(
                    node.node_id,
                    connectivity_state=next_state,
                    updated_at=now,
                    connection=connection,
                )
                self._connectivity_event_store.append_event(
                    node_id=node.node_id,
                    event_type=next_state,
                    created_at=now,
                    detail=detail,
                    connection=connection,
                )
            transitions.append((node.node_id, next_state))
        return transitions
