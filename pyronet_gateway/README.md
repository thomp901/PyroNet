# PyroNet Gateway

This repository now contains only the gateway runtime.

The gateway accepts node traffic over CoAP, stores accepted uplinks durably in a local SQLite outbox, retries delivery to a remote backhaul service over HTTP, and exposes one binary HTTP downlink API at `POST /api/v1/downlinks` for `0x84` requests and `0x85` responses.

## Runtime Flow

1. Nodes send CoAP `POST` requests to the uplink resource.
2. [pyronet_gateway/coap_intake.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/coap_intake.py:1) validates the CoAP datagram and extracts the node payload.
3. [pyronet_gateway/service.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/service.py:1) parses the node packet, updates current node IPv6 and liveness in SQLite, and enqueues a backhaul envelope in the durable outbox.
4. [pyronet_gateway/registration_worker.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/registration_worker.py:1) keeps the gateway registered with the remote backhaul.
5. [pyronet_gateway/retry_worker.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/retry_worker.py:1) drains the outbox with exponential backoff until the remote service returns a terminal receipt.
6. [pyronet_gateway/downlink_http_api.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/downlink_http_api.py:1) accepts binary `application/octet-stream` downlink requests and [pyronet_gateway/downlink_delivery_service.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/downlink_delivery_service.py:1) resolves `node_id -> current IPv6` before emitting CoAP back into the mesh.

## Key Components

- [pyronet_gateway/app/bootstrap.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/app/bootstrap.py:1)
  Gateway process composition and main loop.
- [pyronet_gateway/storage.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/storage.py:1)
  SQLite schema, transaction helper, node-state store, outbox store, dead-letter store, and downlink audit storage.
- [pyronet_gateway/backhaul_client.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/backhaul_client.py:1)
  HTTP client for gateway registration and uplink delivery.
- [pyronet_gateway/protocol/backhaul.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/protocol/backhaul.py:1)
  Binary gateway-to-backhaul framing.
- [pyronet_gateway/protocol/node_packets.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/protocol/node_packets.py:1)
  Binary node packet parsing.

## Config

Example config lives in [config.example.toml](/home/admin/border_router/pyronet_gateway/config.example.toml:1).

Environment overrides:

- `PYRONET_GATEWAY_ID`
- `PYRONET_GATEWAY_LATITUDE`
- `PYRONET_GATEWAY_LONGITUDE`
- `PYRONET_GATEWAY_WISUN_IPV6`
- `PYRONET_SW_VERSION_OVERRIDE`
- `PYRONET_COAP_BIND_HOST`
- `PYRONET_COAP_BIND_PORT`
- `PYRONET_COAP_RESOURCE_PATH`
- `PYRONET_COAP_DUPLICATE_CACHE_TTL_SECONDS`
- `PYRONET_COAP_DUPLICATE_CACHE_MAX_ENTRIES`
- `PYRONET_COAP_DOWNLINK_PORT`
- `PYRONET_COAP_DOWNLINK_RESOURCE_PATH`
- `PYRONET_COAP_ACK_TIMEOUT_SECONDS`
- `PYRONET_COAP_MAX_RETRANSMIT`
- `PYRONET_BACKHAUL_BASE_URL`
- `PYRONET_HTTP_TIMEOUT_SECONDS`
- `PYRONET_REGISTER_RETRY_BASE_DELAY_SECONDS`
- `PYRONET_REGISTER_RETRY_MAX_DELAY_SECONDS`
- `PYRONET_UPLINK_RETRY_BASE_DELAY_SECONDS`
- `PYRONET_UPLINK_RETRY_MAX_DELAY_SECONDS`
- `PYRONET_DB_PATH`
- `PYRONET_WORKER_POLL_INTERVAL_SECONDS`
- `PYRONET_LOG_LEVEL`
- `PYRONET_HTTP_API_BIND_HOST`
- `PYRONET_HTTP_API_PORT`
- `PYRONET_HTTP_API_MAX_REQUEST_BODY_BYTES`

## Local Run

```bash
python3 -m pyronet_gateway --config ./config.example.toml
```

Equivalent `make` target:

```bash
make run
```

Override the config file when needed:

```bash
make run CONFIG=/path/to/gateway.toml
```

## Developer Workflow

```bash
make install-dev
make check
```

Useful targets:

- `make lint`
- `make format`
- `make run`
- `make live-decode`
- `make live-backhaul-send`
- `make test`
- `make test-live-backhaul`
- `make db-path`
- `make db-tables`
- `make db-shell`
- `make db-downlinks`
- `make pre-commit-install`
- `make pre-commit-run`
- `make check` runs `ruff` plus the unit test suite.
- `pre-commit` requires `pyronet_gateway` to be inside a Git repository root. If this directory is copied outside Git, `make pre-commit-run` will fail even though the configuration is valid.
- The DB helper targets default to `./pyronet-gateway.sqlite3`; override with `DB_PATH=/path/to/file.sqlite3` if your runtime config uses a different SQLite file.

To watch live HTTP downlink traffic hitting `POST /api/v1/downlinks`, run the gateway with `INFO` logs enabled and tail stdout/stderr. Each request now emits `downlink_http recv` and `downlink_http done` lines with `gateway_id`, `downlink_id`, `target_node_id`, HTTP status, and terminal delivery status when the binary request header is valid.

Example:

```bash
python3 -m pyronet_gateway --config ./config.example.toml --log-level INFO
```

## Backhaul Payload Contract

All integer fields are little-endian. IPv6 fields are 16 raw network-order bytes.

Gateway registration is sent as binary `application/octet-stream` to:

```text
POST /api/v1/gateways/register
```

Packet type `0x81` has this 34-byte payload:

| Offset | Size | Field | Type | Notes |
| ---: | ---: | --- | --- | --- |
| 0 | 1 | `message_type` | `uint8` | Constant `0x81` |
| 1 | 1 | `version` | `uint8` | Backhaul protocol version |
| 2 | 2 | `gateway_id` | `uint16` | Gateway identifier |
| 4 | 4 | `timestamp` | `uint32` | Unix epoch seconds |
| 8 | 16 | `wisun_ipv6` | `uint8[16]` | Gateway Wi-SUN IPv6 address |
| 24 | 4 | `latitude` | `float32` | Degrees |
| 28 | 4 | `longitude` | `float32` | Degrees |
| 32 | 2 | `sw_version` | `uint16` | Packed software version |

Python struct format:

```text
<BBHI16sffH
```

The `wisun_ipv6` field is intentionally placed after `timestamp`, matching the existing node uplink envelope convention where the mesh IPv6 value follows the time field before payload-specific metadata.

## Live Packet Decode

For passive live decode of PyroNet CoAP traffic on Linux, run:

```bash
sudo make live-decode
```

By default this uses `./config.example.toml`, sniffs `tun0`, decodes CoAP payloads for `/uplink` and `/downlink`, shows ACKs, and shows backhaul queue/retry/delivery state from the local SQLite outbox.

You can still override the config or decoder flags:

```bash
sudo make live-decode CONFIG=./other-config.toml
```

```bash
sudo make live-decode LIVE_DECODE_ARGS="--show-unknown-coap"
```

## Spoofed CSP Uplink Send

To send a spoofed gateway registration plus node uplinks directly to the configured backhaul/CSP, run:

```bash
make live-backhaul-send CONFIG=./config.example.toml
```

The command reads the backhaul base URL and gateway metadata from the selected config file and prints one JSON line per send result.
By default, the follow-up uplink uses packet type `0x02` (`SENSOR_REPORT`), and the spoofed node registration point is nudged slightly away from the gateway coordinates so the two locations do not overlap.

Pass node-specific overrides through `LIVE_BACKHAUL_SEND_ARGS`, for example:

```bash
make live-backhaul-send \
  CONFIG=./config.example.toml \
  LIVE_BACKHAUL_SEND_ARGS="--node-id 4242 --observed-src-ipv6 fd12:3456::4242 --parent-ipv6 fd12:3456::1 --followup-packet-type 0x03"
```

## Test Strategy

Current tests live in:

- [tests/test_gateway_service.py](/home/admin/border_router/pyronet_gateway/tests/test_gateway_service.py:1) for uplink intake, outbox persistence, retry behavior, and HTTP backhaul classification.
- [tests/test_downlink_phase2.py](/home/admin/border_router/pyronet_gateway/tests/test_downlink_phase2.py:1) for binary downlink encoding, delivery routing, and `/api/v1/downlinks` HTTP behavior.
- [tests/test_backhaul_live.py](/home/admin/border_router/pyronet_gateway/tests/test_backhaul_live.py:1) for opt-in live backhaul sends against a real endpoint.

Live backhaul test guard:

- `tests/test_backhaul_live.py` will not send anything unless `PYRONET_RUN_LIVE_BACKHAUL_TESTS=1`.
- It also requires `PYRONET_LIVE_BACKHAUL_BASE_URL` to be set explicitly, so it cannot accidentally use the default runtime base URL.
- The live node-flow test sends in this order: gateway `0x81` registration, node `0x01` registration uplink, then one follow-up `0x02`, `0x03`, or `0x08` uplink.
- Optional overrides: `PYRONET_LIVE_BACKHAUL_TIMEOUT_SECONDS`, `PYRONET_LIVE_BACKHAUL_GATEWAY_ID`, `PYRONET_LIVE_BACKHAUL_VERSION`, `PYRONET_LIVE_BACKHAUL_LATITUDE`, `PYRONET_LIVE_BACKHAUL_LONGITUDE`, `PYRONET_LIVE_BACKHAUL_SW_VERSION`, `PYRONET_LIVE_BACKHAUL_OBSERVED_SRC_IPV6`, `PYRONET_LIVE_BACKHAUL_NODE_ID`, `PYRONET_LIVE_BACKHAUL_PARENT_IPV6`, and `PYRONET_LIVE_BACKHAUL_FOLLOWUP_PACKET_TYPE`.

Example on-demand invocation:

```bash
PYRONET_RUN_LIVE_BACKHAUL_TESTS=1 \
PYRONET_LIVE_BACKHAUL_BASE_URL=http://example.test:4000 \
make test-live-backhaul
```
