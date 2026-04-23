# PyroNet Services

This workspace now contains:

- the gateway service for phases 1 and 2
- the CSP ingest service for phase 3
- the CSP control-plane workflows for phase 4

Phase 4 adds backend-only control logic for configuration rollout, nearest-neighbor computation and push, daily time sync, and stale-node monitoring. It does not add frontend/UI, notifications, long-term analytics, or node-application acknowledgements beyond gateway delivery results.

## Phase 4 Flow

1. The CSP ingests gateway `0x81` and `0x82` traffic and maintains current node state in PostgreSQL.
2. Control workflows operate in stable `node_id` space only.
3. The CSP sends downlink intent to the gateway over the phase-2 HTTP API:
   - `POST /api/v1/downlinks/config`
   - `POST /api/v1/downlinks/nn-table`
   - `POST /api/v1/downlinks/time-sync`
4. Config rollout intent reaches the CSP through `POST /api/v1/control/configs`.
5. The gateway resolves `node_id -> current IPv6` and emits CoAP into the mesh.
6. The CSP records control intent, per-target attempts, scheduler runs, and connectivity transitions for operator visibility.

## Phase 4 Components

- [pyronet_gateway/control_gateway_client.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/control_gateway_client.py:1)
  HTTP client for the gateway’s downlink API.
- [pyronet_gateway/config_workflow_service.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/config_workflow_service.py:1)
  Monotonic `config_id` allocation, immutable config revision persistence, and per-target config push.
- [pyronet_gateway/control_http_api.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/control_http_api.py:1)
  Runtime HTTP surface for config rollout intent.
- [pyronet_gateway/neighbor_compute_service.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/neighbor_compute_service.py:1)
  Deterministic nearest-neighbor computation from stored node coordinates.
- [pyronet_gateway/neighbor_push_service.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/neighbor_push_service.py:1)
  Neighbor-set diffing, persistence, and push through the gateway.
- [pyronet_gateway/time_sync_scheduler.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/time_sync_scheduler.py:1)
  Daily time-sync run creation and per-node attempt persistence.
- [pyronet_gateway/connectivity_monitor.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/connectivity_monitor.py:1)
  Stale/healthy transition generation from `nodes.last_seen`.

Persistence helpers for phase 4:

- [pyronet_gateway/config_store.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/config_store.py:1)
- [pyronet_gateway/neighbor_store.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/neighbor_store.py:1)
- [pyronet_gateway/control_attempt_store.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/control_attempt_store.py:1)
- [pyronet_gateway/scheduler_store.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/scheduler_store.py:1)
- [pyronet_gateway/connectivity_event_store.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/connectivity_event_store.py:1)

Runtime entrypoint:

- [pyronet_gateway/app/control_bootstrap.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/app/control_bootstrap.py:1)

## PostgreSQL Schema

Phase 4 extends the phase-3 schema in [pyronet_gateway/db/migrations.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/db/migrations.py:1) with:

- `control_sequences`
- `risk_configs`
- `risk_config_targets`
- `neighbor_sets`
- `neighbor_set_members`
- `control_attempts`
- `scheduler_runs`
- `node_connectivity_events`
- `nodes.connectivity_state`

Important semantics:

- `config_id` is allocated from `control_sequences` and remains monotonic.
- `risk_configs` is immutable revision history.
- `risk_config_targets` keeps the intended target set plus the latest transport outcome.
- `neighbor_sets` stores the last successfully pushed effective set per node.
- `control_attempts` is append-only and records both success and failure responses from the gateway.
- `scheduler_runs` records time-sync job runs.
- `node_connectivity_events` records only stale/healthy transitions.

## Workflow Semantics

### Config rollout

- A new config revision is persisted before any push.
- Runtime config rollout is exposed at `POST /api/v1/control/configs`.
- Each target node receives a gateway request body containing the allocated `config_id`.
- Gateway delivery is recorded as a transport outcome only.
- Historical revisions are preserved.

### Neighbor recompute

- Neighbor sets are computed from persisted node coordinates using configurable radius and optional max-neighbor limits.
- Selection is deterministic: distance first, then `node_id`.
- Nodes without coordinates are skipped.
- Only a successful gateway push updates the effective current set.
- If a node’s last successful effective set is unchanged, no push attempt is sent.
- Failed pushes remain retryable on the next recompute because they do not advance the effective set.

### Time sync

- A simple in-process loop runs time sync on a configurable interval.
- Only nodes seen within the configured active window are targeted.
- Each run is persisted in `scheduler_runs`.
- On restart, the control plane restores the most recent completed time-sync run and will not rerun immediately if the interval has not elapsed.

### Connectivity monitoring

- Nodes are considered stale when `now - last_seen` exceeds the configured threshold.
- Initial healthy state is tracked silently.
- Stale and recovery transitions are written once per state change.

## Config

Gateway phase 1/2 example config remains in [config.example.toml](/home/admin/border_router/pyronet_gateway/config.example.toml:1).

CSP example config is in [csp.example.toml](/home/admin/border_router/pyronet_gateway/csp.example.toml:1).

Relevant CSP sections:

- `[postgres]`
  `dsn`
- `[csp_http_api]`
  phase-3 ingest bind host/port
- `[control_http_api]`
  phase-4 config rollout bind host/port
- `[gateway_api]`
  gateway base URL and timeout for phase-4 control requests
- `[control]`
  scheduler poll interval
- `[neighbor_policy]`
  radius, max neighbors, recompute interval
- `[time_sync]`
  schedule interval and active-node window
- `[connectivity]`
  stale threshold and scan interval

Environment overrides:

- `PYRONET_CSP_POSTGRES_DSN`
- `PYRONET_CSP_HTTP_BIND_HOST`
- `PYRONET_CSP_HTTP_PORT`
- `PYRONET_CSP_CONTROL_HTTP_BIND_HOST`
- `PYRONET_CSP_CONTROL_HTTP_PORT`
- `PYRONET_CSP_CONTROL_HTTP_MAX_REQUEST_BODY_BYTES`
- `PYRONET_CSP_GATEWAY_BASE_URL`
- `PYRONET_CSP_GATEWAY_HTTP_TIMEOUT_SECONDS`
- `PYRONET_CSP_CONTROL_SCHEDULER_POLL_INTERVAL_SECONDS`
- `PYRONET_CSP_NEIGHBOR_RADIUS_METERS`
- `PYRONET_CSP_NEIGHBOR_MAX_NEIGHBORS`
- `PYRONET_CSP_NEIGHBOR_RECOMPUTE_INTERVAL_SECONDS`
- `PYRONET_CSP_TIME_SYNC_INTERVAL_SECONDS`
- `PYRONET_CSP_TIME_SYNC_ACTIVE_NODE_WINDOW_SECONDS`
- `PYRONET_CSP_CONNECTIVITY_STALE_AFTER_SECONDS`
- `PYRONET_CSP_CONNECTIVITY_POLL_INTERVAL_SECONDS`

## Local Run

Gateway service:

```bash
python3 -m pyronet_gateway --config ./config.example.toml
```

CSP ingest-only service:

```bash
python3 -m pyronet_gateway.app.csp_bootstrap --config ./csp.example.toml
```

CSP ingest + control-plane runtime:

```bash
python3 -m pyronet_gateway.app.control_bootstrap --config ./csp.example.toml
```

The CSP runtime expects `psycopg` plus a reachable PostgreSQL DSN. Tests use in-memory stores and mocked gateway HTTP responses instead of a live database or gateway.

## Test Strategy

Phase 3 tests remain in [tests/test_csp_ingest_phase3.py](/home/admin/border_router/pyronet_gateway/tests/test_csp_ingest_phase3.py:1).

Phase 4 tests are in [tests/test_control_plane_phase4.py](/home/admin/border_router/pyronet_gateway/tests/test_control_plane_phase4.py:1). They cover:

- monotonic `config_id` allocation
- runtime config-rollout HTTP entrypoint behavior
- config revision persistence and per-node target persistence
- correct config push request bodies
- deterministic neighbor computation
- unchanged neighbor-set skip behavior
- changed neighbor-set push behavior
- failed neighbor push retryability
- time-sync run persistence and per-node requests
- restart-safe time-sync scheduling state restore
- stale and healthy connectivity transitions
- gateway failure persistence
- end-to-end control workflow behavior with mocked gateway HTTP responses
