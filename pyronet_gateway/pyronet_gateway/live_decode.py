from __future__ import annotations

import argparse
import datetime as dt
import ipaddress
import os
import socket
import sqlite3
import struct
import sys
import time
from dataclasses import dataclass
from pathlib import Path

from .coap_intake import (
    COAP_CODE_BAD_REQUEST,
    COAP_CODE_CHANGED,
    COAP_METHOD_POST,
    CoapParseError,
    parse_coap_message,
)
from .config import CoapConfig, load_config
from .protocol.backhaul import RECEIPT_PERMANENT_REJECT
from .protocol.node_packets import (
    TYPE_NEIGHBOR_ALERT,
    TYPE_SENSOR_ALERT,
    PacketParseError,
    ParentUpdatePacket,
    RegistrationPacket,
    SensorPacket,
    packet_type,
    parse_node_packet,
)

ETH_P_ALL = 0x0003
ETHERTYPE_IPV6 = 0x86DD
ETHERTYPE_VLAN = 0x8100
IPPROTO_UDP = 17
IPV6_NEXT_HEADERS_WITH_EXT_LEN = {0, 43, 60}
IPV6_NEXT_HEADER_FRAGMENT = 44
IPV6_NEXT_HEADER_AH = 51
NN_TABLE_HEADER = struct.Struct("<BBHB")
TIME_SYNC = struct.Struct("<BBI")
CONFIG_UPDATE = struct.Struct("<BBIhHHhHHHHH")


@dataclass(frozen=True)
class ObservedUdpPacket:
    timestamp: dt.datetime
    src_ipv6: str
    dst_ipv6: str
    src_port: int
    dst_port: int
    payload: bytes


@dataclass(frozen=True)
class LiveDecodeConfig:
    coap_port: int
    uplink_resource_path: tuple[str, ...]
    downlink_resource_path: tuple[str, ...]
    show_acks: bool = False
    show_non_pyronet: bool = False
    show_backhaul: bool = False
    backhaul_db_path: Path | None = None


@dataclass(frozen=True)
class BackhaulPendingSnapshot:
    uplink_id: int
    gateway_id: int
    received_at: int
    observed_src_ipv6: str
    payload_bytes: bytes
    attempt_count: int
    last_attempt_at: int | None
    next_attempt_at: int
    last_error: str | None


@dataclass(frozen=True)
class BackhaulDeadLetterSnapshot:
    uplink_id: int
    receipt_status: int
    reason: str
    moved_at: int


def _coap_resource_path(resource_path: str) -> tuple[str, ...]:
    return tuple(segment for segment in resource_path.strip("/").split("/") if segment)


def build_live_decode_config(args: argparse.Namespace) -> LiveDecodeConfig:
    if args.config:
        config = load_config(args.config)
        coap = config.coap
        db_path = config.runtime.db_path
    else:
        coap = CoapConfig()
        db_path = None

    return LiveDecodeConfig(
        coap_port=args.port if args.port is not None else coap.port,
        uplink_resource_path=_coap_resource_path(args.uplink_resource or coap.resource_path),
        downlink_resource_path=_coap_resource_path(args.downlink_resource or coap.downlink_resource_path),
        show_acks=args.show_acks,
        show_non_pyronet=args.show_non_pyronet,
        show_backhaul=args.show_backhaul,
        backhaul_db_path=Path(args.db_path) if args.db_path else db_path,
    )


def extract_ipv6_packet(frame: bytes) -> bytes | None:
    if len(frame) >= 40 and frame[0] >> 4 == 6:
        return frame
    if len(frame) < 14:
        return None

    ethertype = int.from_bytes(frame[12:14], "big")
    if ethertype == ETHERTYPE_IPV6:
        return frame[14:]
    if ethertype == ETHERTYPE_VLAN and len(frame) >= 18:
        vlan_ethertype = int.from_bytes(frame[16:18], "big")
        if vlan_ethertype == ETHERTYPE_IPV6:
            return frame[18:]
    return None


def parse_observed_udp_packet(frame: bytes, *, timestamp: dt.datetime | None = None) -> ObservedUdpPacket | None:
    ipv6_packet = extract_ipv6_packet(frame)
    if ipv6_packet is None or len(ipv6_packet) < 48:
        return None
    if ipv6_packet[0] >> 4 != 6:
        return None

    payload_length = int.from_bytes(ipv6_packet[4:6], "big")
    next_header = ipv6_packet[6]
    src_ipv6 = str(ipaddress.IPv6Address(ipv6_packet[8:24]))
    dst_ipv6 = str(ipaddress.IPv6Address(ipv6_packet[24:40]))
    payload_end = min(len(ipv6_packet), 40 + payload_length)
    offset = 40

    while True:
        if next_header == IPPROTO_UDP:
            break
        if next_header in IPV6_NEXT_HEADERS_WITH_EXT_LEN:
            if offset + 2 > payload_end:
                return None
            next_header = ipv6_packet[offset]
            offset += (ipv6_packet[offset + 1] + 1) * 8
            continue
        if next_header == IPV6_NEXT_HEADER_FRAGMENT:
            if offset + 8 > payload_end:
                return None
            next_header = ipv6_packet[offset]
            offset += 8
            continue
        if next_header == IPV6_NEXT_HEADER_AH:
            if offset + 2 > payload_end:
                return None
            next_header = ipv6_packet[offset]
            offset += (ipv6_packet[offset + 1] + 2) * 4
            continue
        return None

    if offset + 8 > payload_end:
        return None

    src_port = int.from_bytes(ipv6_packet[offset : offset + 2], "big")
    dst_port = int.from_bytes(ipv6_packet[offset + 2 : offset + 4], "big")
    udp_length = int.from_bytes(ipv6_packet[offset + 4 : offset + 6], "big")
    udp_end = min(payload_end, offset + udp_length)
    if udp_end < offset + 8:
        return None

    return ObservedUdpPacket(
        timestamp=timestamp or dt.datetime.now(),
        src_ipv6=src_ipv6,
        dst_ipv6=dst_ipv6,
        src_port=src_port,
        dst_port=dst_port,
        payload=ipv6_packet[offset + 8 : udp_end],
    )


def summarize_observed_udp(packet: ObservedUdpPacket, config: LiveDecodeConfig) -> str | None:
    if config.coap_port not in {packet.src_port, packet.dst_port}:
        return None

    try:
        coap = parse_coap_message(packet.payload)
    except CoapParseError as exc:
        if not config.show_non_pyronet:
            return None
        return _format_prefix(packet) + f" non-pyronet invalid-coap error={exc}"

    coap_code = _format_coap_code(coap.code)
    path = "/" + "/".join(coap.uri_path) if coap.uri_path else "/"
    token = coap.token.hex() or "-"

    if coap.code == COAP_METHOD_POST and coap.uri_path == config.uplink_resource_path:
        return _format_prefix(packet) + " " + _summarize_uplink(coap.payload, path=path, code=coap_code, token=token)
    if coap.code == COAP_METHOD_POST and coap.uri_path == config.downlink_resource_path:
        return _format_prefix(packet) + " " + _summarize_downlink(coap.payload, path=path, code=coap_code, token=token)

    if coap.msg_type == 2:
        if not config.show_acks:
            return None
        ack_name = {
            _format_coap_code(0): "EMPTY",
            _format_coap_code(COAP_CODE_CHANGED): "CHANGED",
            _format_coap_code(COAP_CODE_BAD_REQUEST): "BAD_REQUEST",
        }.get(coap_code, coap_code)
        return _format_prefix(packet) + f" ack code={ack_name} msg_id={coap.message_id} token={token}"

    if not config.show_non_pyronet:
        return None
    return (
        _format_prefix(packet)
        + f" coap code={coap_code} path={path} msg_id={coap.message_id} token={token} payload_len={len(coap.payload)}"
    )


def _format_prefix(packet: ObservedUdpPacket) -> str:
    timestamp = packet.timestamp.strftime("%H:%M:%S.%f")[:-3]
    return (
        f"{timestamp} {packet.src_ipv6}:{packet.src_port} -> {packet.dst_ipv6}:{packet.dst_port}"
    )


def _format_coap_code(code: int) -> str:
    return f"{code >> 5}.{code & 0x1F:02d}"


def _summarize_uplink(payload: bytes, *, path: str, code: str, token: str) -> str:
    return f"uplink code={code} path={path} token={token} {_summarize_node_payload(payload)}"


def _summarize_node_payload(payload: bytes) -> str:
    try:
        msg_type = packet_type(payload)
    except PacketParseError as exc:
        return f"invalid-pyronet error={exc}"

    if msg_type == TYPE_NEIGHBOR_ALERT:
        return "type=NEIGHBOR_ALERT ignored_by_gateway=true"

    try:
        decoded = parse_node_packet(payload)
    except PacketParseError as exc:
        return f"invalid-pyronet type=0x{msg_type:02x} error={exc}"

    if isinstance(decoded, RegistrationPacket):
        parent = decoded.parent_ipv6 or "-"
        return (
            f"type=REGISTRATION"
            f" node={decoded.node_id} ver={decoded.version}"
            f" lat={decoded.latitude:.4f} lon={decoded.longitude:.4f}"
            f" fw=0x{decoded.fw_version:04x} batt={decoded.battery_pct}% parent={parent}"
        )
    if isinstance(decoded, SensorPacket):
        packet_name = "SENSOR_ALERT" if decoded.packet_type == TYPE_SENSOR_ALERT else "SENSOR_REPORT"
        return (
            f"type={packet_name}"
            f" node={decoded.node_id} ver={decoded.version} ts={decoded.timestamp}"
            f" risk={decoded.risk_level} temp_raw={decoded.temperature}"
            f" humidity_raw={decoded.humidity} voc_iaq={decoded.voc_iaq}"
            f" pm25={decoded.pm25} batt={decoded.battery_pct}%"
        )
    if isinstance(decoded, ParentUpdatePacket):
        parent = decoded.parent_ipv6 or "-"
        return (
            f"type=PARENT_UPDATE"
            f" node={decoded.node_id} ver={decoded.version}"
            f" ts={decoded.timestamp} parent={parent}"
        )
    return "type=UNKNOWN"


def _summarize_downlink(payload: bytes, *, path: str, code: str, token: str) -> str:
    if not payload:
        return f"downlink code={code} path={path} token={token} empty-payload"

    msg_type = payload[0]
    if msg_type == 0x04:
        if len(payload) < NN_TABLE_HEADER.size:
            return f"downlink code={code} path={path} token={token} type=NN_TABLE invalid-length={len(payload)}"
        decoded_type, version, target_node_id, count = NN_TABLE_HEADER.unpack(payload[: NN_TABLE_HEADER.size])
        expected_len = NN_TABLE_HEADER.size + 16 * count
        if len(payload) != expected_len:
            return (
                f"downlink code={code} path={path} token={token} type=NN_TABLE"
                f" target={target_node_id} ver={version} count={count} invalid-length={len(payload)} expected={expected_len}"
            )
        neighbors = [
            str(ipaddress.IPv6Address(payload[offset : offset + 16]))
            for offset in range(NN_TABLE_HEADER.size, expected_len, 16)
        ]
        return (
            f"downlink code={code} path={path} token={token} type=NN_TABLE"
            f" target={target_node_id} ver={version} count={count} neighbors={neighbors}"
        )

    if msg_type == 0x05:
        if len(payload) != TIME_SYNC.size:
            return f"downlink code={code} path={path} token={token} type=TIME_SYNC invalid-length={len(payload)}"
        _decoded_type, version, epoch = TIME_SYNC.unpack(payload)
        return (
            f"downlink code={code} path={path} token={token} type=TIME_SYNC"
            f" ver={version} epoch={epoch}"
        )

    if msg_type == 0x06:
        if len(payload) != CONFIG_UPDATE.size:
            return f"downlink code={code} path={path} token={token} type=CONFIG invalid-length={len(payload)}"
        unpacked = CONFIG_UPDATE.unpack(payload)
        return (
            f"downlink code={code} path={path} token={token} type=CONFIG"
            f" ver={unpacked[1]} config_id={unpacked[2]}"
            f" l2_temp={unpacked[3]} l2_humidity={unpacked[4]} l2_voc={unpacked[5]}"
            f" l3_temp={unpacked[6]} l3_humidity={unpacked[7]} l3_voc={unpacked[8]}"
            f" l4_voc={unpacked[9]} l5_voc={unpacked[10]} l5_pm25={unpacked[11]}"
        )

    return f"downlink code={code} path={path} token={token} unknown-type=0x{msg_type:02x} payload_len={len(payload)}"


def _format_local_time(timestamp: dt.datetime) -> str:
    return timestamp.strftime("%H:%M:%S.%f")[:-3]


def _timestamp_from_epoch(epoch_seconds: int | None) -> str:
    if epoch_seconds is None:
        return _format_local_time(dt.datetime.now())
    return _format_local_time(dt.datetime.fromtimestamp(epoch_seconds))


def summarize_backhaul_changes(
    previous_pending: dict[int, BackhaulPendingSnapshot],
    current_pending: dict[int, BackhaulPendingSnapshot],
    previous_dead_letter: dict[int, BackhaulDeadLetterSnapshot],
    current_dead_letter: dict[int, BackhaulDeadLetterSnapshot],
    *,
    observed_at: dt.datetime,
) -> list[str]:
    lines: list[str] = []

    for uplink_id in sorted(current_dead_letter):
        if uplink_id in previous_dead_letter:
            continue
        dead_letter = current_dead_letter[uplink_id]
        pending = current_pending.get(uplink_id) or previous_pending.get(uplink_id)
        lines.append(_format_backhaul_dead_letter(dead_letter, pending))

    for uplink_id in sorted(current_pending):
        pending = current_pending[uplink_id]
        previous = previous_pending.get(uplink_id)
        if previous is None:
            lines.append(_format_backhaul_enqueued(pending))
            continue
        if (
            pending.attempt_count != previous.attempt_count
            or pending.last_attempt_at != previous.last_attempt_at
            or pending.last_error != previous.last_error
        ):
            lines.append(_format_backhaul_retry(pending))

    for uplink_id in sorted(previous_pending):
        if uplink_id in current_pending or uplink_id in current_dead_letter:
            continue
        lines.append(_format_backhaul_delivered(previous_pending[uplink_id], observed_at=observed_at))

    return lines


def _format_backhaul_enqueued(record: BackhaulPendingSnapshot) -> str:
    return (
        f"{_timestamp_from_epoch(record.received_at)} backhaul enqueue"
        f" uplink_id={record.uplink_id} gateway={record.gateway_id}"
        f" src={record.observed_src_ipv6} {_summarize_node_payload(record.payload_bytes)}"
    )


def _format_backhaul_retry(record: BackhaulPendingSnapshot) -> str:
    next_attempt = dt.datetime.fromtimestamp(record.next_attempt_at).strftime("%H:%M:%S")
    return (
        f"{_timestamp_from_epoch(record.last_attempt_at)} backhaul retry"
        f" uplink_id={record.uplink_id} attempts={record.attempt_count}"
        f" next_at={next_attempt} error={record.last_error or '-'}"
    )


def _format_backhaul_dead_letter(
    record: BackhaulDeadLetterSnapshot,
    pending: BackhaulPendingSnapshot | None,
) -> str:
    details = ""
    if pending is not None:
        details = (
            f" gateway={pending.gateway_id} src={pending.observed_src_ipv6}"
            f" {_summarize_node_payload(pending.payload_bytes)}"
        )
    status = "PERMANENT_REJECT" if record.receipt_status == RECEIPT_PERMANENT_REJECT else f"0x{record.receipt_status:02x}"
    return (
        f"{_timestamp_from_epoch(record.moved_at)} backhaul dead-letter"
        f" uplink_id={record.uplink_id} receipt_status={status}"
        f" reason={record.reason}{details}"
    )


def _format_backhaul_delivered(record: BackhaulPendingSnapshot, *, observed_at: dt.datetime) -> str:
    return (
        f"{_format_local_time(observed_at)} backhaul delivered"
        f" uplink_id={record.uplink_id} attempts={record.attempt_count}"
        f" gateway={record.gateway_id} src={record.observed_src_ipv6}"
        f" {_summarize_node_payload(record.payload_bytes)}"
    )


def _load_backhaul_pending_snapshot(connection: sqlite3.Connection) -> dict[int, BackhaulPendingSnapshot]:
    rows = connection.execute(
        """
        SELECT
            uplink_id,
            gateway_id,
            received_at,
            observed_src_ipv6,
            payload_bytes,
            attempt_count,
            last_attempt_at,
            next_attempt_at,
            last_error
        FROM outbox
        WHERE state = 'pending'
        ORDER BY uplink_id ASC
        """
    ).fetchall()
    return {
        int(row["uplink_id"]): BackhaulPendingSnapshot(
            uplink_id=row["uplink_id"],
            gateway_id=row["gateway_id"],
            received_at=row["received_at"],
            observed_src_ipv6=row["observed_src_ipv6"],
            payload_bytes=row["payload_bytes"],
            attempt_count=row["attempt_count"],
            last_attempt_at=row["last_attempt_at"],
            next_attempt_at=row["next_attempt_at"],
            last_error=row["last_error"],
        )
        for row in rows
    }


def _load_backhaul_dead_letter_snapshot(connection: sqlite3.Connection) -> dict[int, BackhaulDeadLetterSnapshot]:
    rows = connection.execute(
        """
        SELECT
            uplink_id,
            receipt_status,
            reason,
            moved_at
        FROM dead_letter
        ORDER BY uplink_id ASC
        """
    ).fetchall()
    return {
        int(row["uplink_id"]): BackhaulDeadLetterSnapshot(
            uplink_id=row["uplink_id"],
            receipt_status=row["receipt_status"],
            reason=row["reason"],
            moved_at=row["moved_at"],
        )
        for row in rows
    }


class BackhaulMonitor:
    def __init__(self, db_path: Path) -> None:
        self._db_path = Path(db_path)
        self._connection: sqlite3.Connection | None = None
        self._warned_missing = False
        self._warned_error: str | None = None
        self._initialized = False
        self._previous_pending: dict[int, BackhaulPendingSnapshot] = {}
        self._previous_dead_letter: dict[int, BackhaulDeadLetterSnapshot] = {}

    @property
    def db_path(self) -> Path:
        return self._db_path

    def poll(self, *, observed_at: dt.datetime) -> list[str]:
        connection = self._ensure_connection()
        if connection is None:
            return []
        try:
            current_pending = _load_backhaul_pending_snapshot(connection)
            current_dead_letter = _load_backhaul_dead_letter_snapshot(connection)
        except sqlite3.Error as exc:
            if self._warned_error != str(exc):
                print(f"warning: backhaul monitor query failed: {exc}", file=sys.stderr)
                self._warned_error = str(exc)
            self._close_connection()
            return []

        self._warned_error = None
        if not self._initialized:
            self._previous_pending = current_pending
            self._previous_dead_letter = current_dead_letter
            self._initialized = True
            return []

        lines = summarize_backhaul_changes(
            self._previous_pending,
            current_pending,
            self._previous_dead_letter,
            current_dead_letter,
            observed_at=observed_at,
        )
        self._previous_pending = current_pending
        self._previous_dead_letter = current_dead_letter
        return lines

    def _ensure_connection(self) -> sqlite3.Connection | None:
        if self._connection is not None:
            return self._connection
        if not self._db_path.exists():
            if not self._warned_missing:
                print(f"warning: backhaul database not found at {self._db_path}", file=sys.stderr)
                self._warned_missing = True
            return None
        try:
            connection = sqlite3.connect(self._db_path, check_same_thread=False)
            connection.row_factory = sqlite3.Row
            connection.execute("PRAGMA query_only=ON")
        except sqlite3.Error as exc:
            if self._warned_error != str(exc):
                print(f"warning: failed to open backhaul database {self._db_path}: {exc}", file=sys.stderr)
                self._warned_error = str(exc)
            return None
        self._warned_missing = False
        self._warned_error = None
        self._connection = connection
        return connection

    def _close_connection(self) -> None:
        if self._connection is not None:
            self._connection.close()
            self._connection = None


def sniff_forever(*, interface: str, config: LiveDecodeConfig) -> int:
    if not hasattr(socket, "AF_PACKET"):
        print("live decode requires Linux AF_PACKET support", file=sys.stderr)
        return 2

    try:
        sock = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(ETH_P_ALL))
        sock.bind((interface, 0))
        sock.settimeout(0.25)
    except PermissionError:
        print("live decode needs root privileges; run with sudo", file=sys.stderr)
        return 2
    except OSError as exc:
        print(f"failed to open interface {interface}: {exc}", file=sys.stderr)
        return 2

    print(
        f"listening on {interface} for CoAP UDP/{config.coap_port}"
        f" uplink=/{'/'.join(config.uplink_resource_path)}"
        f" downlink=/{'/'.join(config.downlink_resource_path)}",
        flush=True,
    )
    backhaul_monitor: BackhaulMonitor | None = None
    if config.show_backhaul:
        if config.backhaul_db_path is None:
            print("showing backhaul requires --config or --db-path", file=sys.stderr)
            return 2
        backhaul_monitor = BackhaulMonitor(config.backhaul_db_path)
        print(f"monitoring backhaul outbox db={config.backhaul_db_path}", flush=True)

    try:
        next_backhaul_poll = time.monotonic()
        while True:
            now_monotonic = time.monotonic()
            if backhaul_monitor is not None and now_monotonic >= next_backhaul_poll:
                for line in backhaul_monitor.poll(observed_at=dt.datetime.now()):
                    print(line, flush=True)
                next_backhaul_poll = now_monotonic + 0.5

            try:
                frame, _sockaddr = sock.recvfrom(65535)
            except TimeoutError:
                continue

            packet = parse_observed_udp_packet(frame)
            if packet is not None:
                line = summarize_observed_udp(packet, config)
                if line:
                    print(line, flush=True)
    except KeyboardInterrupt:
        return 0
    finally:
        sock.close()


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Passive live decoder for PyroNet CoAP packets")
    parser.add_argument("--config", help="Optional TOML config file used for CoAP port and resource paths")
    parser.add_argument("--interface", default="tun0", help="Linux interface to sniff, default: tun0")
    parser.add_argument("--port", type=int, help="Override CoAP UDP port")
    parser.add_argument("--uplink-resource", help="Override uplink resource path, default: /uplink")
    parser.add_argument("--downlink-resource", help="Override downlink resource path, default: /downlink")
    parser.add_argument("--show-acks", action="store_true", help="Show CoAP ACK packets in addition to POST traffic")
    parser.add_argument(
        "--show-backhaul",
        action="store_true",
        help="Also watch the gateway SQLite outbox and show backhaul queue/retry/delivery events",
    )
    parser.add_argument(
        "--db-path",
        help="Override SQLite database path used for backhaul monitoring; defaults to runtime.db_path from --config",
    )
    parser.add_argument(
        "--show-non-pyronet",
        action="store_true",
        help="Show other CoAP packets on the same port instead of only recognized PyroNet traffic",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)
    if os.geteuid() != 0:
        print("warning: passive sniffing usually requires sudo", file=sys.stderr)
    config = build_live_decode_config(args)
    return sniff_forever(interface=args.interface, config=config)


if __name__ == "__main__":
    raise SystemExit(main())
