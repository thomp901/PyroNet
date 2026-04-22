from __future__ import annotations

import copy
import json
import unittest
import urllib.error

from pyronet_gateway.app.control_bootstrap import ControlPlaneApplication
from pyronet_gateway.config_store import RiskConfigRecord, RiskConfigTargetRecord
from pyronet_gateway.config_workflow_service import ConfigWorkflowService, RiskConfigSpec
from pyronet_gateway.connectivity_event_store import ConnectivityEventRecord
from pyronet_gateway.connectivity_monitor import ConnectivityMonitor
from pyronet_gateway.control_attempt_store import ControlAttemptRecord
from pyronet_gateway.control_gateway_client import ControlGatewayClient, GatewayControlResult
from pyronet_gateway.control_http_api import ControlHttpApi
from pyronet_gateway.db.migrations import MIGRATIONS
from pyronet_gateway.neighbor_compute_service import NeighborComputeService
from pyronet_gateway.neighbor_push_service import NeighborPushService
from pyronet_gateway.neighbor_store import NeighborSetRecord
from pyronet_gateway.node_store import NodeRecord
from pyronet_gateway.scheduler_store import SchedulerRunRecord
from pyronet_gateway.time_sync_scheduler import TimeSyncScheduler


class InMemoryTransactionManager:
    def __init__(self, stores) -> None:
        self._stores = stores

    def transaction(self):
        return _InMemoryTransaction(self._stores)


class _InMemoryTransaction:
    def __init__(self, stores) -> None:
        self._stores = stores

    def __enter__(self):
        self._snapshots = [store.snapshot() for store in self._stores]
        return self

    def __exit__(self, exc_type, exc, _tb):
        if exc_type is not None:
            for store, snapshot in zip(self._stores, self._snapshots):
                store.restore(snapshot)
        return False


class InMemoryNodeStore:
    def __init__(self) -> None:
        self.nodes = {}

    def snapshot(self):
        return copy.deepcopy(self.nodes)

    def restore(self, snapshot):
        self.nodes = snapshot

    def add(self, node: NodeRecord) -> None:
        self.nodes[node.node_id] = node

    def list_with_coordinates(self, *, connection=None):
        return [node for node in self.nodes.values() if node.latitude is not None and node.longitude is not None]

    def list_recently_seen(self, min_last_seen: int, *, connection=None):
        return [
            node
            for node in self.nodes.values()
            if node.last_seen is not None and node.last_seen >= min_last_seen
        ]

    def list_for_connectivity(self, *, connection=None):
        return [node for node in self.nodes.values() if node.last_seen is not None]

    def set_connectivity_state(self, node_id: int, *, connectivity_state: str, updated_at: int, connection=None):
        node = self.nodes[node_id]
        self.nodes[node_id] = NodeRecord(
            node_id=node.node_id,
            current_ipv6=node.current_ipv6,
            latitude=node.latitude,
            longitude=node.longitude,
            firmware_version=node.firmware_version,
            latest_battery=node.latest_battery,
            latest_risk_level=node.latest_risk_level,
            latest_temperature=node.latest_temperature,
            latest_humidity=node.latest_humidity,
            latest_voc=node.latest_voc,
            latest_pm25=node.latest_pm25,
            last_seen=node.last_seen,
            last_gateway_id=node.last_gateway_id,
            latest_parent_ipv6=node.latest_parent_ipv6,
            created_at=node.created_at,
            updated_at=updated_at,
            connectivity_state=connectivity_state,
        )


class InMemoryControlAttemptStore:
    def __init__(self) -> None:
        self.attempts = []
        self._next_id = 1

    def snapshot(self):
        return copy.deepcopy((self.attempts, self._next_id))

    def restore(self, snapshot):
        self.attempts, self._next_id = snapshot

    def record_attempt(self, **kwargs):
        kwargs.pop("connection", None)
        record = ControlAttemptRecord(id=self._next_id, **kwargs)
        self.attempts.append(record)
        self._next_id += 1
        return record.id


class InMemoryConfigStore:
    def __init__(self) -> None:
        self.next_config_id = 1
        self.configs = {}
        self.targets = {}

    def snapshot(self):
        return copy.deepcopy((self.next_config_id, self.configs, self.targets))

    def restore(self, snapshot):
        self.next_config_id, self.configs, self.targets = snapshot

    def allocate_next_config_id(self, *, connection=None):
        config_id = self.next_config_id
        self.next_config_id += 1
        return config_id

    def create_config(self, config: RiskConfigRecord, *, connection=None):
        self.configs[config.config_id] = config

    def upsert_targets(self, *, config_id: int, node_ids: list[int], desired_state: str, connection=None):
        for node_id in node_ids:
            self.targets[(config_id, node_id)] = RiskConfigTargetRecord(
                config_id=config_id,
                node_id=node_id,
                desired_state=desired_state,
                latest_attempt_status=None,
                latest_attempt_time=None,
                latest_result_payload=None,
            )

    def update_target_result(
        self,
        *,
        config_id: int,
        node_id: int,
        latest_attempt_status: str,
        latest_attempt_time: int,
        latest_result_payload: str,
        connection=None,
    ):
        target = self.targets[(config_id, node_id)]
        self.targets[(config_id, node_id)] = RiskConfigTargetRecord(
            config_id=target.config_id,
            node_id=target.node_id,
            desired_state=target.desired_state,
            latest_attempt_status=latest_attempt_status,
            latest_attempt_time=latest_attempt_time,
            latest_result_payload=latest_result_payload,
        )


class InMemoryNeighborStore:
    def __init__(self) -> None:
        self.sets = {}

    def snapshot(self):
        return copy.deepcopy(self.sets)

    def restore(self, snapshot):
        self.sets = snapshot

    def get_neighbor_set(self, node_id: int, *, connection=None):
        return self.sets.get(node_id)

    def upsert_neighbor_set(
        self,
        *,
        node_id: int,
        neighbor_node_ids: list[int],
        computed_at: int,
        radius_meters: float,
        max_neighbors: int | None,
        connection=None,
    ):
        self.sets[node_id] = NeighborSetRecord(
            node_id=node_id,
            computed_at=computed_at,
            radius_meters=radius_meters,
            max_neighbors=max_neighbors,
            neighbor_node_ids=list(neighbor_node_ids),
        )


class InMemorySchedulerStore:
    def __init__(self) -> None:
        self.runs = {}
        self._next_id = 1

    def snapshot(self):
        return copy.deepcopy((self.runs, self._next_id))

    def restore(self, snapshot):
        self.runs, self._next_id = snapshot

    def start_run(self, *, job_type: str, started_at: int, details: str | None, connection=None):
        run = SchedulerRunRecord(
            id=self._next_id,
            job_type=job_type,
            started_at=started_at,
            completed_at=None,
            status="running",
            details=details,
        )
        self.runs[self._next_id] = run
        self._next_id += 1
        return run.id

    def complete_run(self, *, run_id: int, completed_at: int, status: str, details: str | None, connection=None):
        run = self.runs[run_id]
        self.runs[run_id] = SchedulerRunRecord(
            id=run.id,
            job_type=run.job_type,
            started_at=run.started_at,
            completed_at=completed_at,
            status=status,
            details=details,
        )

    def latest_completed_run(self, job_type: str, *, connection=None):
        completed = [
            run
            for run in self.runs.values()
            if run.job_type == job_type and run.completed_at is not None
        ]
        if not completed:
            return None
        completed.sort(key=lambda run: (run.completed_at, run.id), reverse=True)
        return completed[0]


class InMemoryConnectivityEventStore:
    def __init__(self) -> None:
        self.events = []
        self._next_id = 1

    def snapshot(self):
        return copy.deepcopy((self.events, self._next_id))

    def restore(self, snapshot):
        self.events, self._next_id = snapshot

    def append_event(self, *, node_id: int, event_type: str, created_at: int, detail: str | None, connection=None):
        event = ConnectivityEventRecord(
            id=self._next_id,
            node_id=node_id,
            event_type=event_type,
            created_at=created_at,
            detail=detail,
        )
        self.events.append(event)
        self._next_id += 1
        return event.id


class FakeGatewayClient:
    def __init__(self, *, config_results=None, nn_results=None, time_sync_results=None) -> None:
        self.base_url = "http://gateway.example"
        self.config_results = list(config_results or [])
        self.nn_results = list(nn_results or [])
        self.time_sync_results = list(time_sync_results or [])
        self.config_calls = []
        self.nn_calls = []
        self.time_sync_calls = []

    def send_config_update(self, *, request_body: dict):
        self.config_calls.append(request_body)
        return self.config_results.pop(0) if self.config_results else _gateway_result("config", request_body["target_node_id"])

    def send_nn_table(self, *, target_node_id: int, neighbor_node_ids: list[int]):
        self.nn_calls.append((target_node_id, neighbor_node_ids))
        return self.nn_results.pop(0) if self.nn_results else _gateway_result("nn-table", target_node_id)

    def send_time_sync(self, *, target_node_id: int, epoch: int):
        self.time_sync_calls.append((target_node_id, epoch))
        return self.time_sync_results.pop(0) if self.time_sync_results else _gateway_result("time-sync", target_node_id)


class FakeHttpResponse:
    def __init__(self, *, status: int, body: str) -> None:
        self.status = status
        self._body = body.encode("utf-8")

    def read(self):
        return self._body


class ControlPlaneWorkflowTests(unittest.TestCase):
    def setUp(self) -> None:
        self.node_store = InMemoryNodeStore()
        self.config_store = InMemoryConfigStore()
        self.attempt_store = InMemoryControlAttemptStore()
        self.neighbor_store = InMemoryNeighborStore()
        self.scheduler_store = InMemorySchedulerStore()
        self.connectivity_event_store = InMemoryConnectivityEventStore()
        self.transaction_manager = InMemoryTransactionManager(
            [
                self.node_store,
                self.config_store,
                self.attempt_store,
                self.neighbor_store,
                self.scheduler_store,
                self.connectivity_event_store,
            ]
        )

    def test_monotonic_config_id_allocation(self) -> None:
        gateway = FakeGatewayClient()
        workflow = ConfigWorkflowService(
            transaction_manager=self.transaction_manager,
            config_store=self.config_store,
            control_attempt_store=self.attempt_store,
            gateway_client=gateway,
        )
        spec = _config_spec()
        first = workflow.create_and_push(spec=spec, target_node_ids=[10], now=1_700_001_000)
        second = workflow.create_and_push(spec=spec, target_node_ids=[11], now=1_700_001_001)
        self.assertEqual(1, first.config_id)
        self.assertEqual(2, second.config_id)

    def test_config_workflow_persists_revisions_and_targets_and_sends_correct_body(self) -> None:
        gateway = FakeGatewayClient()
        workflow = ConfigWorkflowService(
            transaction_manager=self.transaction_manager,
            config_store=self.config_store,
            control_attempt_store=self.attempt_store,
            gateway_client=gateway,
        )
        result = workflow.create_and_push(spec=_config_spec(created_by="ops"), target_node_ids=[100, 101], now=1_700_001_010)
        self.assertEqual(1, result.config_id)
        self.assertEqual("ops", self.config_store.configs[1].created_by)
        self.assertEqual(2, len(self.config_store.targets))
        self.assertEqual(2, len(gateway.config_calls))
        self.assertEqual(
            {
                "target_node_id": 100,
                "config_id": 1,
                "l2_temp_thresh": -100,
                "l2_humidity_thresh": 100,
                "l2_voc_thresh": 200,
                "l3_temp_thresh": -50,
                "l3_humidity_thresh": 150,
                "l3_voc_thresh": 300,
                "l4_voc_thresh": 350,
                "l5_voc_thresh": 400,
                "l5_pm25_thresh": 500,
            },
            gateway.config_calls[0],
        )
        self.assertEqual("accepted_and_delivered", self.config_store.targets[(1, 100)].latest_attempt_status)

    def test_neighbor_computation_correctness_from_sample_coordinates(self) -> None:
        compute = NeighborComputeService(radius_meters=130.0, max_neighbors=2)
        nodes = [
            _node(1, lat=0.0, lon=0.0),
            _node(2, lat=0.0, lon=0.0005),
            _node(3, lat=0.0, lon=0.0010),
            _node(4, lat=0.0, lon=0.01),
        ]
        computed = compute.compute(nodes)
        self.assertEqual([2, 3], computed[1].neighbor_node_ids)
        self.assertEqual([1, 3], computed[2].neighbor_node_ids)
        self.assertEqual([2, 1], computed[3].neighbor_node_ids)
        self.assertEqual([], computed.get(4, _neighbor_computation(4, [])).neighbor_node_ids)

    def test_unchanged_neighbor_set_does_not_trigger_push(self) -> None:
        self.node_store.add(_node(1, lat=0.0, lon=0.0))
        self.node_store.add(_node(2, lat=0.0, lon=0.0005))
        compute = NeighborComputeService(radius_meters=100.0, max_neighbors=4)
        self.neighbor_store.upsert_neighbor_set(
            node_id=1,
            neighbor_node_ids=[2],
            computed_at=1_700_001_020,
            radius_meters=100.0,
            max_neighbors=4,
        )
        self.neighbor_store.upsert_neighbor_set(
            node_id=2,
            neighbor_node_ids=[1],
            computed_at=1_700_001_020,
            radius_meters=100.0,
            max_neighbors=4,
        )
        gateway = FakeGatewayClient()
        service = NeighborPushService(
            transaction_manager=self.transaction_manager,
            node_store=self.node_store,
            neighbor_store=self.neighbor_store,
            control_attempt_store=self.attempt_store,
            gateway_client=gateway,
            compute_service=compute,
        )
        changed = service.recompute_and_push(now=1_700_001_021)
        self.assertEqual({}, changed)
        self.assertEqual([], gateway.nn_calls)

    def test_changed_neighbor_set_triggers_push(self) -> None:
        self.node_store.add(_node(1, lat=0.0, lon=0.0))
        self.node_store.add(_node(2, lat=0.0, lon=0.0005))
        self.node_store.add(_node(3, lat=0.0, lon=0.0009))
        gateway = FakeGatewayClient()
        service = NeighborPushService(
            transaction_manager=self.transaction_manager,
            node_store=self.node_store,
            neighbor_store=self.neighbor_store,
            control_attempt_store=self.attempt_store,
            gateway_client=gateway,
            compute_service=NeighborComputeService(radius_meters=120.0, max_neighbors=4),
        )
        changed = service.recompute_and_push(now=1_700_001_030)
        self.assertIn(1, changed)
        self.assertEqual([2, 3], self.neighbor_store.sets[1].neighbor_node_ids)
        self.assertEqual(3, len(gateway.nn_calls))

    def test_failed_neighbor_push_does_not_mark_set_as_current_and_is_retried(self) -> None:
        self.node_store.add(_node(1, lat=0.0, lon=0.0))
        self.node_store.add(_node(2, lat=0.0, lon=0.0005))
        gateway = FakeGatewayClient(
            nn_results=[
                GatewayControlResult(
                    request_type="nn-table",
                    target_node_id=1,
                    status_code=503,
                    response_body="gateway unavailable",
                    delivery_result="temporary_gateway_failure",
                    response_payload=None,
                ),
                _gateway_result("nn-table", 1),
            ]
        )
        service = NeighborPushService(
            transaction_manager=self.transaction_manager,
            node_store=self.node_store,
            neighbor_store=self.neighbor_store,
            control_attempt_store=self.attempt_store,
            gateway_client=gateway,
            compute_service=NeighborComputeService(radius_meters=100.0, max_neighbors=4),
        )
        service.recompute_and_push(now=1_700_001_031)
        self.assertIsNone(self.neighbor_store.get_neighbor_set(1))

        service.recompute_and_push(now=1_700_001_032)
        self.assertEqual([2], self.neighbor_store.get_neighbor_set(1).neighbor_node_ids)
        self.assertEqual(2, [call[0] for call in gateway.nn_calls].count(1))

    def test_successful_neighbor_push_updates_effective_set_and_unchanged_successful_set_skips(self) -> None:
        self.node_store.add(_node(1, lat=0.0, lon=0.0))
        self.node_store.add(_node(2, lat=0.0, lon=0.0005))
        gateway = FakeGatewayClient()
        service = NeighborPushService(
            transaction_manager=self.transaction_manager,
            node_store=self.node_store,
            neighbor_store=self.neighbor_store,
            control_attempt_store=self.attempt_store,
            gateway_client=gateway,
            compute_service=NeighborComputeService(radius_meters=100.0, max_neighbors=4),
        )
        service.recompute_and_push(now=1_700_001_033)
        self.assertEqual([2], self.neighbor_store.get_neighbor_set(1).neighbor_node_ids)
        first_call_count = len(gateway.nn_calls)

        service.recompute_and_push(now=1_700_001_034)
        self.assertEqual(first_call_count, len(gateway.nn_calls))

    def test_time_sync_scheduler_produces_per_node_requests_and_persists_run(self) -> None:
        self.node_store.add(_node(10, last_seen=1_700_001_100))
        self.node_store.add(_node(11, last_seen=1_699_000_000))
        gateway = FakeGatewayClient()
        scheduler = TimeSyncScheduler(
            transaction_manager=self.transaction_manager,
            node_store=self.node_store,
            scheduler_store=self.scheduler_store,
            control_attempt_store=self.attempt_store,
            gateway_client=gateway,
            active_node_window_seconds=100,
        )
        run_id = scheduler.run_once(now=1_700_001_150)
        self.assertEqual(1, run_id)
        self.assertEqual([(10, 1_700_001_150)], gateway.time_sync_calls)
        self.assertEqual("completed", self.scheduler_store.runs[run_id].status)
        self.assertEqual(run_id, self.attempt_store.attempts[0].scheduler_run_id)

    def test_connectivity_stale_transition_generation(self) -> None:
        self.node_store.add(_node(20, last_seen=1_700_000_000, connectivity_state=None))
        monitor = ConnectivityMonitor(
            transaction_manager=self.transaction_manager,
            node_store=self.node_store,
            connectivity_event_store=self.connectivity_event_store,
            stale_after_seconds=100,
        )
        transitions = monitor.run_once(now=1_700_000_200)
        self.assertEqual([(20, "stale")], transitions)
        self.assertEqual("stale", self.node_store.nodes[20].connectivity_state)
        self.assertEqual("stale", self.connectivity_event_store.events[0].event_type)

    def test_connectivity_healthy_recovery_transition_generation(self) -> None:
        self.node_store.add(_node(21, last_seen=1_700_000_000, connectivity_state="stale"))
        monitor = ConnectivityMonitor(
            transaction_manager=self.transaction_manager,
            node_store=self.node_store,
            connectivity_event_store=self.connectivity_event_store,
            stale_after_seconds=100,
        )
        self.node_store.nodes[21] = _node(21, last_seen=1_700_000_250, connectivity_state="stale")
        transitions = monitor.run_once(now=1_700_000_260)
        self.assertEqual([(21, "healthy")], transitions)
        self.assertEqual("healthy", self.node_store.nodes[21].connectivity_state)
        self.assertEqual("healthy", self.connectivity_event_store.events[0].event_type)

    def test_gateway_client_failure_persistence(self) -> None:
        gateway = FakeGatewayClient(
            config_results=[
                GatewayControlResult(
                    request_type="config",
                    target_node_id=200,
                    status_code=None,
                    response_body="timed out",
                    delivery_result="gateway_transport_error",
                    response_payload=None,
                )
            ]
        )
        workflow = ConfigWorkflowService(
            transaction_manager=self.transaction_manager,
            config_store=self.config_store,
            control_attempt_store=self.attempt_store,
            gateway_client=gateway,
        )
        workflow.create_and_push(spec=_config_spec(), target_node_ids=[200], now=1_700_001_200)
        self.assertEqual("gateway_transport_error", self.attempt_store.attempts[0].delivery_result)
        self.assertEqual("gateway_transport_error", self.config_store.targets[(1, 200)].latest_attempt_status)

    def test_runtime_config_http_entrypoint_persists_revision_and_attempts(self) -> None:
        gateway = FakeGatewayClient()
        workflow = ConfigWorkflowService(
            transaction_manager=self.transaction_manager,
            config_store=self.config_store,
            control_attempt_store=self.attempt_store,
            gateway_client=gateway,
        )
        api = ControlHttpApi(config_workflow_service=workflow, max_request_body_bytes=4096)
        status, headers, body = api.handle_request(
            method="POST",
            path="/api/v1/control/configs",
            body=json.dumps(
                {
                    "target_node_ids": [300, 301],
                    "l2_temp_thresh": -100,
                    "l2_humidity_thresh": 100,
                    "l2_voc_thresh": 200,
                    "l3_temp_thresh": -50,
                    "l3_humidity_thresh": 150,
                    "l3_voc_thresh": 300,
                    "l4_voc_thresh": 350,
                    "l5_voc_thresh": 400,
                    "l5_pm25_thresh": 500,
                    "created_by": "api",
                }
            ).encode("utf-8"),
            now=1_700_001_250,
        )
        payload = json.loads(body)
        self.assertEqual(201, status)
        self.assertEqual("application/json", headers["Content-Type"])
        self.assertEqual(1, payload["config_id"])
        self.assertEqual(2, len(self.attempt_store.attempts))
        self.assertIn(1, self.config_store.configs)

    def test_invalid_config_http_request_is_rejected_cleanly(self) -> None:
        gateway = FakeGatewayClient()
        workflow = ConfigWorkflowService(
            transaction_manager=self.transaction_manager,
            config_store=self.config_store,
            control_attempt_store=self.attempt_store,
            gateway_client=gateway,
        )
        api = ControlHttpApi(config_workflow_service=workflow, max_request_body_bytes=4096)
        status, _headers, body = api.handle_request(
            method="POST",
            path="/api/v1/control/configs",
            body=json.dumps({"target_node_ids": [300]}).encode("utf-8"),
            now=1_700_001_251,
        )
        payload = json.loads(body)
        self.assertEqual(400, status)
        self.assertIn("missing field", payload["error"])

    def test_runtime_config_http_entrypoint_invokes_workflow_service(self) -> None:
        class StubWorkflow:
            def __init__(self) -> None:
                self.calls = []

            def create_and_push(self, *, spec, target_node_ids, now):
                self.calls.append((spec, target_node_ids, now))
                return type("Result", (), {"config_id": 77, "target_results": []})()

        workflow = StubWorkflow()
        api = ControlHttpApi(config_workflow_service=workflow, max_request_body_bytes=4096)
        status, _headers, body = api.handle_request(
            method="POST",
            path="/api/v1/control/configs",
            body=json.dumps(
                {
                    "target_node_ids": [400],
                    "l2_temp_thresh": -100,
                    "l2_humidity_thresh": 100,
                    "l2_voc_thresh": 200,
                    "l3_temp_thresh": -50,
                    "l3_humidity_thresh": 150,
                    "l3_voc_thresh": 300,
                    "l4_voc_thresh": 350,
                    "l5_voc_thresh": 400,
                    "l5_pm25_thresh": 500,
                }
            ).encode("utf-8"),
            now=1_700_001_252,
        )
        payload = json.loads(body)
        self.assertEqual(201, status)
        self.assertEqual(77, payload["config_id"])
        self.assertEqual(1, len(workflow.calls))

    def test_end_to_end_config_workflow_with_mocked_gateway_http_response(self) -> None:
        client = ControlGatewayClient(
            base_url="http://gateway.example",
            timeout_seconds=5.0,
            urlopen=lambda request, timeout: FakeHttpResponse(
                status=200,
                body=json.dumps(
                    {
                        "delivery_result": "accepted_and_delivered",
                        "target_node_id": json.loads(request.data.decode("utf-8"))["target_node_id"],
                    }
                ),
            ),
        )
        workflow = ConfigWorkflowService(
            transaction_manager=self.transaction_manager,
            config_store=self.config_store,
            control_attempt_store=self.attempt_store,
            gateway_client=client,
        )
        result = workflow.create_and_push(spec=_config_spec(), target_node_ids=[300], now=1_700_001_300)
        self.assertEqual("accepted_and_delivered", result.target_results[0].delivery_result)
        self.assertEqual(200, self.attempt_store.attempts[0].response_status)

    def test_control_gateway_client_transport_error(self) -> None:
        client = ControlGatewayClient(
            base_url="http://gateway.example",
            timeout_seconds=5.0,
            urlopen=lambda _request, timeout: (_ for _ in ()).throw(urllib.error.URLError("offline")),
        )
        result = client.send_time_sync(target_node_id=400, epoch=1_700_001_400)
        self.assertEqual("gateway_transport_error", result.delivery_result)
        self.assertIsNone(result.status_code)

    def test_control_plane_migration_contains_required_tables(self) -> None:
        migration = MIGRATIONS[3]
        self.assertIn("CREATE TABLE IF NOT EXISTS risk_configs", migration)
        self.assertIn("CREATE TABLE IF NOT EXISTS neighbor_sets", migration)
        self.assertIn("CREATE TABLE IF NOT EXISTS control_attempts", migration)
        self.assertIn("CREATE TABLE IF NOT EXISTS scheduler_runs", migration)
        self.assertIn("CREATE TABLE IF NOT EXISTS node_connectivity_events", migration)

    def test_restart_with_recent_completed_time_sync_run_does_not_rerun_immediately(self) -> None:
        self.scheduler_store.start_run(job_type="time-sync", started_at=1_700_001_300, details=None)
        self.scheduler_store.complete_run(run_id=1, completed_at=1_700_001_350, status="completed", details=None)
        app = ControlPlaneApplication.__new__(ControlPlaneApplication)
        app.scheduler_store = self.scheduler_store
        last_run_at = app._load_last_completed_run_at("time-sync")
        self.assertEqual(1_700_001_350, last_run_at)
        self.assertFalse(ControlPlaneApplication._should_run(1_700_001_360, last_run_at, 86_400))

    def test_restart_after_interval_expiry_does_run_again(self) -> None:
        self.scheduler_store.start_run(job_type="time-sync", started_at=1_700_001_300, details=None)
        self.scheduler_store.complete_run(run_id=1, completed_at=1_700_001_350, status="completed", details=None)
        app = ControlPlaneApplication.__new__(ControlPlaneApplication)
        app.scheduler_store = self.scheduler_store
        last_run_at = app._load_last_completed_run_at("time-sync")
        self.assertTrue(ControlPlaneApplication._should_run(1_700_090_000, last_run_at, 86_400))


def _config_spec(*, created_by: str | None = None) -> RiskConfigSpec:
    return RiskConfigSpec(
        l2_temp_thresh=-100,
        l2_humidity_thresh=100,
        l2_voc_thresh=200,
        l3_temp_thresh=-50,
        l3_humidity_thresh=150,
        l3_voc_thresh=300,
        l4_voc_thresh=350,
        l5_voc_thresh=400,
        l5_pm25_thresh=500,
        created_by=created_by,
        source="test",
        comment="phase4",
    )


def _node(
    node_id: int,
    *,
    lat: float | None = 39.0,
    lon: float | None = -86.0,
    last_seen: int = 1_700_001_000,
    connectivity_state: str | None = None,
) -> NodeRecord:
    return NodeRecord(
        node_id=node_id,
        current_ipv6=f"fd12:3456::{node_id:x}",
        latitude=lat,
        longitude=lon,
        firmware_version=0x0102,
        latest_battery=80,
        latest_risk_level=2,
        latest_temperature=220,
        latest_humidity=500,
        latest_voc=300,
        latest_pm25=10,
        last_seen=last_seen,
        last_gateway_id=7,
        latest_parent_ipv6=None,
        created_at=1_700_000_000,
        updated_at=1_700_000_000,
        connectivity_state=connectivity_state,
    )


def _gateway_result(request_type: str, node_id: int) -> GatewayControlResult:
    return GatewayControlResult(
        request_type=request_type,
        target_node_id=node_id,
        status_code=200,
        response_body=json.dumps({"delivery_result": "accepted_and_delivered"}),
        delivery_result="accepted_and_delivered",
        response_payload={"delivery_result": "accepted_and_delivered"},
    )


def _neighbor_computation(node_id: int, neighbors: list[int]):
    return type("NeighborComputation", (), {"node_id": node_id, "neighbor_node_ids": neighbors})()
