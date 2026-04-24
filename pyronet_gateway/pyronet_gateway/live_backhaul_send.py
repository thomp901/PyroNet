from __future__ import annotations

import argparse
import ipaddress
import json
import struct
import time

from .backhaul_client import HTTPBackhaulClient
from .config import GatewayConfig, load_config
from .protocol.backhaul import GatewayRegistration, NodeUplinkEnvelope, UplinkReceipt
from .protocol.node_packets import TYPE_PARENT_UPDATE, TYPE_REGISTRATION, TYPE_SENSOR_ALERT, TYPE_SENSOR_REPORT

REGISTRATION_STRUCT = struct.Struct("<BBHffHB16s")
SENSOR_STRUCT = struct.Struct("<BBHIBhHHHB")
PARENT_UPDATE_STRUCT = struct.Struct("<BBHI16s")
_NODE_LATITUDE_OFFSET = 0.0012
_NODE_LONGITUDE_OFFSET = 0.0012


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Send spoofed PyroNet uplinks to the configured backhaul/CSP.")
    parser.add_argument("--config", default="./config.example.toml", help="Path to the gateway TOML config.")
    parser.add_argument("--node-id", type=int, default=1001, help="Spoofed node ID to send.")
    parser.add_argument(
        "--observed-src-ipv6",
        default="fd12:3456::abcd",
        help="Observed source IPv6 to place in the uplink envelope.",
    )
    parser.add_argument(
        "--parent-ipv6",
        default="fd12:3456::1",
        help="Parent IPv6 to encode into registration or parent-update payloads.",
    )
    parser.add_argument(
        "--followup-packet-type",
        type=lambda value: int(value, 0),
        default=TYPE_SENSOR_REPORT,
        help="Follow-up node packet type: 0x02, 0x03, or 0x08. Defaults to 0x02.",
    )
    parser.add_argument(
        "--send-registration",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Send the gateway registration packet before spoofed node uplinks.",
    )
    parser.add_argument(
        "--send-node-registration",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Send the spoofed node registration uplink before the follow-up uplink.",
    )
    parser.add_argument(
        "--received-at",
        type=int,
        default=None,
        help="Unix timestamp to use for the first uplink. Defaults to the current time.",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    config = load_config(args.config)
    client = HTTPBackhaulClient(
        base_url=config.backhaul.base_url,
        timeout_seconds=config.backhaul.http_timeout_seconds,
    )

    now = args.received_at if args.received_at is not None else int(time.time())

    if args.send_registration:
        registration = build_gateway_registration(config=config, now=now)
        client.send_gateway_registration(registration)
        print(
            json.dumps(
                {
                    "kind": "gateway_registration",
                    "base_url": config.backhaul.base_url,
                    "gateway_id": registration.gateway_id,
                    "timestamp": registration.timestamp,
                    "status": "sent",
                },
                sort_keys=True,
            )
        )

    if args.send_node_registration:
        registration_envelope = build_node_registration_envelope(
            config=config,
            node_id=args.node_id,
            observed_src_ipv6=args.observed_src_ipv6,
            parent_ipv6=args.parent_ipv6,
            received_at=now,
            uplink_id=now,
        )
        registration_receipt = client.send_uplink(registration_envelope)
        print(json.dumps(render_uplink_result("node_registration", registration_envelope, registration_receipt), sort_keys=True))

    followup_envelope = build_followup_envelope(
        config=config,
        node_id=args.node_id,
        observed_src_ipv6=args.observed_src_ipv6,
        parent_ipv6=args.parent_ipv6,
        packet_type=args.followup_packet_type,
        received_at=now + 1,
        uplink_id=now + 1,
    )
    followup_receipt = client.send_uplink(followup_envelope)
    print(json.dumps(render_uplink_result("node_followup", followup_envelope, followup_receipt), sort_keys=True))
    return 0


def build_gateway_registration(*, config: GatewayConfig, now: int) -> GatewayRegistration:
    return GatewayRegistration(
        version=config.backhaul_version,
        gateway_id=config.gateway_id,
        timestamp=now,
        wisun_ipv6=config.wisun_ipv6,
        latitude=config.latitude,
        longitude=config.longitude,
        sw_version=config.sw_version,
    )


def build_node_registration_envelope(
    *,
    config: GatewayConfig,
    node_id: int,
    observed_src_ipv6: str,
    parent_ipv6: str,
    received_at: int,
    uplink_id: int,
) -> NodeUplinkEnvelope:
    return NodeUplinkEnvelope(
        version=config.backhaul_version,
        gateway_id=config.gateway_id,
        uplink_id=uplink_id,
        received_at=received_at,
        observed_src_ipv6=observed_src_ipv6,
        payload=build_registration_packet(
            version=config.node_packet_version,
            node_id=node_id,
            latitude=config.latitude + _NODE_LATITUDE_OFFSET,
            longitude=config.longitude + _NODE_LONGITUDE_OFFSET,
            sw_version=config.sw_version,
            battery_pct=88,
            parent_ipv6=parent_ipv6,
        ),
    )


def build_followup_envelope(
    *,
    config: GatewayConfig,
    node_id: int,
    observed_src_ipv6: str,
    parent_ipv6: str,
    packet_type: int,
    received_at: int,
    uplink_id: int,
) -> NodeUplinkEnvelope:
    return NodeUplinkEnvelope(
        version=config.backhaul_version,
        gateway_id=config.gateway_id,
        uplink_id=uplink_id,
        received_at=received_at,
        observed_src_ipv6=observed_src_ipv6,
        payload=build_followup_packet(
            version=config.node_packet_version,
            packet_type=packet_type,
            node_id=node_id,
            timestamp=received_at,
            parent_ipv6=parent_ipv6,
        ),
    )


def build_registration_packet(
    *,
    version: int,
    node_id: int,
    latitude: float,
    longitude: float,
    sw_version: int,
    battery_pct: int,
    parent_ipv6: str | None,
) -> bytes:
    parent = encode_optional_ipv6(parent_ipv6)
    return REGISTRATION_STRUCT.pack(
        TYPE_REGISTRATION,
        version,
        node_id,
        latitude,
        longitude,
        sw_version,
        battery_pct,
        parent,
    )


def build_followup_packet(
    *,
    version: int,
    packet_type: int,
    node_id: int,
    timestamp: int,
    parent_ipv6: str | None,
) -> bytes:
    if packet_type in {TYPE_SENSOR_REPORT, TYPE_SENSOR_ALERT}:
        return SENSOR_STRUCT.pack(
            packet_type,
            version,
            node_id,
            timestamp,
            3,
            2450,
            5000,
            123,
            55,
            90,
        )
    if packet_type == TYPE_PARENT_UPDATE:
        return PARENT_UPDATE_STRUCT.pack(
            TYPE_PARENT_UPDATE,
            version,
            node_id,
            timestamp,
            encode_optional_ipv6(parent_ipv6),
        )
    raise ValueError("follow-up packet type must be 0x02, 0x03, or 0x08")


def encode_optional_ipv6(value: str | None) -> bytes:
    if not value:
        return b"\x00" * 16
    return ipaddress.IPv6Address(value).packed


def render_uplink_result(kind: str, envelope: NodeUplinkEnvelope, receipt: UplinkReceipt) -> dict[str, object]:
    return {
        "kind": kind,
        "gateway_id": envelope.gateway_id,
        "uplink_id": envelope.uplink_id,
        "observed_src_ipv6": envelope.observed_src_ipv6,
        "payload_len": len(envelope.payload),
        "receipt_status": receipt.status,
        "status": "sent",
    }


if __name__ == "__main__":
    raise SystemExit(main())
