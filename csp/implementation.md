# Persistence Implementation

This document tracks the phased implementation of gateway-to-CSP persistence for binary backhaul ingest. Update it as development progresses so scope, status, and sequencing stay aligned with the codebase.

## Boundary

- This workstream is limited to CSP-side persistence and ingest handling.
- It does not implement gateway firmware, durable outbox behavior at the gateway, CoAP termination, Wi-SUN border-router logic, or mesh delivery/retry behavior.
- Gateway-observed fields such as source IPv6 are persisted as reported metadata for CSP use; they do not make the CSP authoritative for border-router state.

## Status Legend

- `not_started`: phase has not begun
- `in_progress`: active development
- `blocked`: waiting on a decision or prerequisite
- `done`: implemented and verified at the intended scope

## Current Focus

- Workstream: durable persistence for gateway registration and node uplink ingest
- Primary files expected to change first: [server/index.ts](/home/diego/PyroNet/csp/server/index.ts), [src/api/types.ts](/home/diego/PyroNet/csp/src/api/types.ts)
- Current active phase: Phase 8 when resumed

## Update Rules

- Update phase status when development starts or completes.
- Add concrete file references under each phase as implementation lands.
- Record blocked decisions in the open questions section instead of letting them stay implicit.
- Keep this document focused on persistence and ingest; UI follow-on work can be linked here but should not dominate the plan.

## Open Questions

- `bvoc_ppm` wire encoding is still unspecified for both telemetry and config payloads.
- Permanent reject criteria for `0x83.status = 0x01` need to be defined precisely.
- Historical storage requirements for parent updates (`0x08`) need to be confirmed.
- Gateway registration deduplication behavior should be specified explicitly.
- The `0x82` uplink envelope size text says `36 + N`, but the packed struct fields sum to `34 + N`; codec work currently follows the struct field sizes.
- The current relational telemetry column is still named `voc_iaq`; Phase 5 projection currently maps `bvoc_ppm` raw values there as an interim fit pending a schema rename or clearer unit decision.

## Current Behavior Notes

- CSP ingest now accepts private IPv6 ULA addresses such as Tailscale `fd...` ranges for observed node and parent addresses.
- CSP still rejects invalid IPv6 categories for these fields, including unspecified, loopback, multicast, and link-local addresses.
- CSP now uses stable permanent-reject codes for parse-stage and validation-stage terminal failures.
- Projection-stage semantic failures are retryable and currently surface as server errors rather than terminal `0x83` rejects.

## Phases

### Phase 0 - Contract Freeze

- Status: `not_started`
- Goal: lock the persistence-facing protocol assumptions before schema or handler work begins
- Scope:
- finalize backhaul `version` policy
- define terminal reject reasons for malformed, unsupported, and semantically invalid envelopes
- define idempotency contract for `(gateway_id, uplink_id)`
- define which fields are persisted raw vs projected into application tables
- Exit criteria:
- binary ingest contract is stable enough to encode in SQL constraints and handler validation
- unresolved wire-format questions are listed explicitly in this file
- Planned artifacts:
- protocol notes in this document
- validation rules implemented in server code later

### Phase 1 - Persistence Foundation

- Status: `in_progress`
- Goal: create durable storage for backhaul envelopes and receipts
- Scope:
- add gateway identity tables
- add gateway registration history
- add durable uplink inbox or raw envelope table
- add terminal receipt tracking and optional dead-letter storage
- enforce uniqueness on `(gateway_id, uplink_id)`
- preserve raw payload bytes and observed metadata for replay/audit
- Exit criteria:
- schema can durably store every inbound `0x81` and `0x82`
- duplicate uplinks can be detected without reprocessing side effects
- Implemented so far:
- added schema support for `gateways` and `gateway_registrations`
- added durable `gateway_uplinks` storage keyed by `(gateway_id, uplink_id)`
- added terminal receipt and dead-letter tables for replay-safe auditability
- Planned files:
- [schema.sql](/home/diego/PyroNet/csp/schema.sql)

### Phase 2 - Binary Codec Layer

- Status: `done`
- Goal: parse and validate binary backhaul messages independently from HTTP routing
- Scope:
- decode `0x81` gateway registration
- decode `0x82` uplink envelope
- emit `0x83` terminal receipt
- decode inner node payloads `0x01`, `0x02`, `0x03`, `0x08`
- validate payload sizes, supported versions, and field bounds
- Exit criteria:
- handler code can depend on typed parse results instead of raw buffers
- malformed payloads can be rejected deterministically
- Implemented so far:
- added [server/backhaulCodec.ts](/home/diego/PyroNet/csp/server/backhaulCodec.ts) with binary parsing for `0x81`, `0x82`, `0x83`
- added inner payload decoding for `0x01`, `0x02`, `0x03`, and `0x08`
- added strict length, version, coordinate, battery, risk-level, and IPv6 validation
- Planned files:
- [server/index.ts](/home/diego/PyroNet/csp/server/index.ts) or a new server-side codec module

### Phase 3 - Gateway Registration Persistence

- Status: `done`
- Goal: persist `POST /api/v1/gateways/register` requests as binary registrations
- Scope:
- add octet-stream request handling
- parse and validate `0x81`
- upsert current gateway identity
- append registration history
- return HTTP `2xx` on successful durable persistence
- Exit criteria:
- a gateway can register repeatedly without corrupting identity state
- registration events are queryable historically
- Implemented so far:
- added `POST /api/v1/gateways/register` in [server/index.ts](/home/diego/PyroNet/csp/server/index.ts)
- route now validates `application/octet-stream`, parses one `0x81` message, and persists gateway snapshot plus append-only registration history
- current gateway snapshot updates are guarded so older gateway timestamps do not overwrite newer state
- Planned files:
- [server/index.ts](/home/diego/PyroNet/csp/server/index.ts)
- [schema.sql](/home/diego/PyroNet/csp/schema.sql)

### Phase 4 - Durable Uplink Inbox

- Status: `done`
- Goal: persist every `0x82` envelope before application projection
- Scope:
- add `POST /api/v1/uplinks`
- accept one octet-stream envelope per request
- persist raw envelope bytes and parsed header metadata first
- deduplicate on `(gateway_id, uplink_id)`
- return a terminal `0x83` only after durable persistence and terminal classification
- Exit criteria:
- replay-safe ingest exists even before full packet projection is complete
- duplicate deliveries produce the same terminal result
- Implemented so far:
- added `POST /api/v1/uplinks` in [server/index.ts](/home/diego/PyroNet/csp/server/index.ts)
- valid `0x82` envelopes are parsed, stored durably in `gateway_uplinks`, and answered with a binary `0x83` durable-ingest receipt
- duplicate `(gateway_id, uplink_id)` deliveries reuse the stored terminal receipt and increment delivery-attempt tracking
- parse and validation failures with enough header information are persisted as rejected raw envelopes with terminal reject receipts and dead-letter rows
- adjusted [schema.sql](/home/diego/PyroNet/csp/schema.sql) so placeholder gateway rows and rejected raw envelopes can be stored without inventing trusted parsed fields
- Planned files:
- [server/index.ts](/home/diego/PyroNet/csp/server/index.ts)
- [schema.sql](/home/diego/PyroNet/csp/schema.sql)

### Phase 5 - Application Projection

- Status: `done`
- Goal: project persisted uplinks into the existing CSP domain model
- Scope:
- `0x01` -> devices, registrations, IPv6 history, current last-seen state
- `0x02` / `0x03` -> sensor readings, current device telemetry, alert-driving records
- `0x08` -> preferred-parent observation history and latest topology state
- attach gateway metadata and raw-payload metadata where appropriate
- keep projection idempotent relative to the raw envelope key
- Exit criteria:
- existing read APIs can operate on ingested backhaul data
- duplicate uplinks do not create duplicate domain records
- Implemented so far:
- valid `0x82` uplinks now project inside the ingest transaction before the terminal durable-ingest receipt is stored
- `0x01` projection updates `devices`, writes `device_registrations`, syncs `device_ipv6_history`, and captures registration-time preferred-parent observations
- `0x02` / `0x03` projection writes `sensor_readings`, updates latest device telemetry, and creates `alerts` plus `alert_events` for `0x03`
- `0x08` projection updates current preferred-parent state and writes append-only parent observation history
- added explicit uplink traceability from projected rows back to `gateway_uplinks`
- Planned files:
- [server/index.ts](/home/diego/PyroNet/csp/server/index.ts)
- [schema.sql](/home/diego/PyroNet/csp/schema.sql)

### Phase 6 - Receipt and Dead-Letter Semantics

- Status: `in_progress`
- Goal: make terminal outcomes explicit and auditable
- Scope:
- store terminal status for each uplink
- return consistent `0x83` for duplicate submissions
- persist permanent rejects with reason codes or operator-readable detail
- separate transient failures from permanent rejects cleanly
- Exit criteria:
- gateway retry behavior can rely on CSP terminal responses
- operators can inspect why a payload was rejected
- Implemented so far:
- parse-stage permanent rejects now use a stable reject-code taxonomy
- duplicate terminal rejects reuse the stored `0x83` and surface the same reject code in API response headers
- projection-stage semantic failures remain retryable rather than becoming terminal `0x83` rejects
- dead-letter and receipt metadata remain focused on parse/validation permanent rejects
- Planned files:
- [schema.sql](/home/diego/PyroNet/csp/schema.sql)
- [server/index.ts](/home/diego/PyroNet/csp/server/index.ts)

### Phase 7 - Read Model Alignment

- Status: `done`
- Goal: align existing API queries and history views with persisted ingest data
- Scope:
- verify dashboard/node/history/alert queries consume the projected records consistently
- ensure packet history is backed by actual ingested records and downlink events
- minimize assumptions that currently exist only for mocks or derived history
- Exit criteria:
- read APIs reflect the new persistence model without ad hoc translation gaps
- Implemented so far:
- added `0x08` / `parent_update` to shared packet-history type definitions
- packet history now includes persisted `device_parent_observations` rows for `0x08` parent updates
- node detail responses now expose current preferred parent and recent parent-observation history
- history-page filters now expose the persisted packet-code dimension instead of only direction/event/status
- mock backend and node-detail UI surfaces were aligned to the same response contract
- Planned files:
- [server/index.ts](/home/diego/PyroNet/csp/server/index.ts)
- [src/api/types.ts](/home/diego/PyroNet/csp/src/api/types.ts)

### Phase 8 - Verification and Operations

- Status: `not_started`
- Goal: verify persistence behavior under duplicate delivery, malformed input, and restart-safe ingest flows
- Scope:
- fixture-driven binary parser tests
- idempotency tests for duplicate uplinks
- tests for permanent reject vs transient failure behavior
- metrics/logging hooks for ingest success, duplicates, and rejects
- Exit criteria:
- persistence semantics are test-backed and observable
- Planned files:
- tests or validation harness to be added

## Change Log

- 2026-04-22: Created phased persistence implementation tracker.
- 2026-04-22: Began Phase 1 and added schema-level persistence tables for gateways, raw uplinks, terminal receipts, and dead-letter records.
- 2026-04-22: Began Phase 2 and added a standalone backhaul codec module for binary message parsing and `0x83` receipt encoding.
- 2026-04-22: Began Phase 3 and wired `POST /api/v1/gateways/register` to persist binary `0x81` gateway registrations.
- 2026-04-22: Completed Phase 4 route wiring for `POST /api/v1/uplinks`, including durable raw-envelope storage, duplicate receipt reuse, and binary `0x83` responses.
- 2026-04-22: Began Phase 5 by projecting `0x01`, `0x02`, `0x03`, and `0x08` uplinks into device, telemetry, alert, and parent-observation tables.
- 2026-04-22: Relaxed ingest validation and schema constraints to accept private/ULA IPv6 addresses for observed node and parent metadata.
- 2026-04-22: Began Phase 6 by normalizing permanent-reject taxonomy across parse and projection failures and surfacing stable reject codes on uplink responses.
- 2026-04-22: Adjusted Phase 6 behavior so projection-stage semantic failures remain retryable and only parse/validation failures produce terminal permanent rejects.
- 2026-04-22: Began Phase 7 by aligning packet history and node detail read models with persisted `0x08` parent-update observations.
- 2026-04-22: Closed Phase 7 for now after aligning packet history, node detail, and traffic-log filters with persisted parent-update data.
