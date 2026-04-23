# PyroNet Live Decode Tool

This document covers only the passive live decoder for PyroNet packets:

```bash
python3 -m pyronet_gateway.live_decode
```

## Purpose

`pyronet_gateway.live_decode` passively sniffs IPv6 UDP traffic on a Linux
interface, parses CoAP traffic on the configured PyroNet port, and decodes the
PyroNet packet payload into human-readable fields.

It is intended for:

- Watching live PyroNet uplinks from nodes
- Watching live PyroNet downlinks sent back to nodes
- Watching backhaul queue and delivery state for accepted uplinks
- Verifying packet contents without manually unpacking binary payloads

It does not bind the gateway UDP port, so it can run alongside the gateway
service.

## What It Decodes

### Uplink packets

The tool decodes CoAP `POST` requests to the uplink resource, normally
`/uplink`.

Supported PyroNet uplink packet types:

- `0x01` Registration
- `0x02` Sensor report
- `0x03` Sensor alert
- `0x08` Parent update

Special case:

- `0x07` Neighbor alert is recognized but shown as ignored by the gateway

### Downlink packets

The tool decodes CoAP `POST` requests to the downlink resource, normally
`/downlink`.

Supported PyroNet downlink packet types:

- `0x04` Neighbor table update
- `0x05` Time sync
- `0x06` Config update

### Backhaul events

With `--show-backhaul`, the tool also tails the gateway SQLite database and
shows uplink lifecycle events as the retry worker processes them.

Supported backhaul events:

- `backhaul enqueue`
- `backhaul retry`
- `backhaul delivered`
- `backhaul dead-letter`

This is derived from the local outbox and dead-letter tables, not by decoding
raw HTTP packets on the wire.

## Requirements

- Linux
- Root privileges for passive packet sniffing
- Python 3
- The `pyronet_gateway` source tree

The decoder uses raw sockets via `AF_PACKET`, so it usually must be run with
`sudo`.

## Basic Usage

From the `pyronet_gateway/` directory:

```bash
sudo python3 -m pyronet_gateway.live_decode --config ./config.example.toml
```

Equivalent `make` target:

```bash
sudo make live-decode CONFIG=./config.example.toml
```

By default this:

- Sniffs `tun0`
- Uses the CoAP port from the config file
- Uses the configured uplink and downlink resource paths
- Shows recognized PyroNet traffic only

To also show backhaul queue state from the gateway database:

```bash
sudo python3 -m pyronet_gateway.live_decode \
  --config ./config.example.toml \
  --show-backhaul
```

If you are not passing `--config`, provide the SQLite database path explicitly:

```bash
sudo python3 -m pyronet_gateway.live_decode \
  --port 5683 \
  --uplink-resource /uplink \
  --downlink-resource /downlink \
  --show-backhaul \
  --db-path ./pyronet-gateway.sqlite3
```

## Common Options

### Show ACKs

```bash
sudo python3 -m pyronet_gateway.live_decode \
  --config ./config.example.toml \
  --show-acks
```

Or with `make`:

```bash
sudo make live-decode \
  CONFIG=./config.example.toml \
  LIVE_DECODE_ARGS="--show-acks"
```

### Use a different interface

```bash
sudo python3 -m pyronet_gateway.live_decode \
  --config ./config.example.toml \
  --interface tun0
```

### Override the CoAP port or resource paths

```bash
sudo python3 -m pyronet_gateway.live_decode \
  --port 5683 \
  --uplink-resource /uplink \
  --downlink-resource /downlink
```

### Show other CoAP traffic on the same port

```bash
sudo python3 -m pyronet_gateway.live_decode \
  --config ./config.example.toml \
  --show-non-pyronet
```

## Example Output

Registration uplink:

```text
12:34:56.123 fd12:3456::10:61616 -> fd12:3456::1:5683 uplink code=0.02 path=/uplink token=01020304 type=REGISTRATION node=1001 ver=1 lat=39.0000 lon=-86.0000 fw=0x0102 batt=88% parent=fd12:3456::2
```

Sensor alert uplink:

```text
12:34:58.440 fd12:3456::10:61616 -> fd12:3456::1:5683 uplink code=0.02 path=/uplink token=01020304 type=SENSOR_ALERT node=1002 ver=1 ts=123456 risk=4 temp_raw=2450 humidity_raw=5000 voc_iaq=123 pm25=55 batt=90%
```

Neighbor-table downlink:

```text
12:35:01.008 fd12:3456::1:40000 -> fd12:3456::10:5683 downlink code=0.02 path=/downlink token=01020304 type=NN_TABLE target=1003 ver=1 count=2 neighbors=['fd12:3456::10', 'fd12:3456::11']
```

Backhaul enqueue:

```text
12:35:02.000 backhaul enqueue uplink_id=7 gateway=77 src=fd12:3456::10 type=REGISTRATION node=1001 ver=1 lat=39.0000 lon=-86.0000 fw=0x0102 batt=88% parent=fd12:3456::2
```

Backhaul retry:

```text
12:35:04.000 backhaul retry uplink_id=7 attempts=1 next_at=12:35:09 error=HTTP 503
```

Backhaul delivery:

```text
12:35:05.000 backhaul delivered uplink_id=7 attempts=1 gateway=77 src=fd12:3456::10 type=REGISTRATION node=1001 ver=1 lat=39.0000 lon=-86.0000 fw=0x0102 batt=88% parent=fd12:3456::2
```

## Field Notes

- `temp_raw`, `humidity_raw`, and similar values are shown exactly as encoded in
  the current PyroNet packet format
- The decoder does not apply any extra engineering-unit scaling beyond what is
  explicitly encoded in the packet structures
- `parent` shows `-` when the parent IPv6 field is all zeros
- Backhaul lines reflect durable queue state from SQLite and may appear slightly
  after the corresponding uplink packet line

## What The Tool Does Not Do

- It does not decode Wi-SUN MAC fields
- It does not decode raw 802.15.4 frames
- It does not read packets from `pcapng`
- It does not modify or inject traffic
- It does not persist anything to the gateway database
- It does not decode raw HTTP traffic to the remote backhaul service

This tool is specifically for decoding PyroNet CoAP payloads observed on the
wire plus reporting local backhaul queue state.

## Troubleshooting

### `live decode needs root privileges`

Run with `sudo`.

### `failed to open interface`

Check that the interface exists and is up:

```bash
ip link show tun0
```

### No output appears

Check:

- The gateway and nodes are actively sending traffic
- You are sniffing the correct interface
- The correct CoAP port is configured
- The resource paths match the runtime configuration

### Only ACKs are visible

Enable:

```bash
--show-acks
```

If you want to also see unrelated CoAP traffic on the same port:

```bash
--show-non-pyronet
```

## Source

Implementation:

- [pyronet_gateway/live_decode.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/live_decode.py:1)

Related protocol definitions:

- [pyronet_gateway/protocol/node_packets.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/protocol/node_packets.py:1)
- [pyronet_gateway/downlink_packet_codec.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/downlink_packet_codec.py:1)
- [pyronet_gateway/coap_intake.py](/home/admin/border_router/pyronet_gateway/pyronet_gateway/coap_intake.py:1)
