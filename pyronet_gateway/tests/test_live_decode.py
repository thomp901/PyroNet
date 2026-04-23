from __future__ import annotations

import datetime as dt
import ipaddress
import struct
import unittest

from pyronet_gateway.live_decode import (
    BackhaulDeadLetterSnapshot,
    BackhaulPendingSnapshot,
    LiveDecodeConfig,
    ObservedUdpPacket,
    summarize_backhaul_changes,
    summarize_observed_udp,
)


def _build_coap_post(*, path: str, payload: bytes, token: bytes = b"\x01\x02\x03\x04", message_id: int = 0x1234) -> bytes:
    first = (1 << 6) | len(token)
    datagram = bytes([first, 2]) + message_id.to_bytes(2, "big") + token
    previous_option = 0
    for segment in [part for part in path.strip("/").split("/") if part]:
        encoded = segment.encode("utf-8")
        datagram += bytes([((11 - previous_option) << 4) | len(encoded)]) + encoded
        previous_option = 11
    return datagram + b"\xFF" + payload


def _build_coap_ack(*, code: int = 68, token: bytes = b"\x01\x02\x03\x04", message_id: int = 0x1234) -> bytes:
    first = (1 << 6) | (2 << 4) | len(token)
    return bytes([first, code]) + message_id.to_bytes(2, "big") + token


def _packet(*, src_port: int, dst_port: int, payload: bytes) -> ObservedUdpPacket:
    return ObservedUdpPacket(
        timestamp=dt.datetime(2026, 4, 23, 12, 34, 56, 123000),
        src_ipv6="fd12:3456::10",
        dst_ipv6="fd12:3456::1",
        src_port=src_port,
        dst_port=dst_port,
        payload=payload,
    )


def _registration_payload() -> bytes:
    return (
        b"\x01\x01"
        + (1001).to_bytes(2, "little")
        + struct.pack("<ffHB", 39.0, -86.0, 0x0102, 88)
        + ipaddress.IPv6Address("fd12:3456::2").packed
    )


def _backhaul_pending(
    *,
    uplink_id: int,
    attempt_count: int = 0,
    last_attempt_at: int | None = None,
    next_attempt_at: int = 1713875700,
    last_error: str | None = None,
) -> BackhaulPendingSnapshot:
    return BackhaulPendingSnapshot(
        uplink_id=uplink_id,
        gateway_id=77,
        received_at=1713875696,
        observed_src_ipv6="fd12:3456::10",
        payload_bytes=_registration_payload(),
        attempt_count=attempt_count,
        last_attempt_at=last_attempt_at,
        next_attempt_at=next_attempt_at,
        last_error=last_error,
    )


class LiveDecodeSummaryTests(unittest.TestCase):
    def setUp(self) -> None:
        self.config = LiveDecodeConfig(
            coap_port=5683,
            uplink_resource_path=("uplink",),
            downlink_resource_path=("downlink",),
        )

    def test_registration_uplink_summary(self) -> None:
        payload = _registration_payload()
        line = summarize_observed_udp(
            _packet(src_port=61616, dst_port=5683, payload=_build_coap_post(path="/uplink", payload=payload)),
            self.config,
        )
        self.assertIsNotNone(line)
        assert line is not None
        self.assertIn("type=REGISTRATION", line)
        self.assertIn("node=1001", line)
        self.assertIn("fw=0x0102", line)
        self.assertIn("parent=fd12:3456::2", line)

    def test_sensor_alert_uplink_summary(self) -> None:
        payload = struct.pack("<BBHIBhHHHB", 0x03, 0x01, 1002, 123456, 4, 2450, 5000, 123, 55, 90)
        line = summarize_observed_udp(
            _packet(src_port=61616, dst_port=5683, payload=_build_coap_post(path="/uplink", payload=payload)),
            self.config,
        )
        self.assertIsNotNone(line)
        assert line is not None
        self.assertIn("type=SENSOR_ALERT", line)
        self.assertIn("temp_raw=2450", line)
        self.assertIn("humidity_raw=5000", line)
        self.assertIn("batt=90%", line)

    def test_downlink_nn_table_summary(self) -> None:
        payload = (
            b"\x04\x01"
            + (1003).to_bytes(2, "little")
            + b"\x02"
            + ipaddress.IPv6Address("fd12:3456::10").packed
            + ipaddress.IPv6Address("fd12:3456::11").packed
        )
        line = summarize_observed_udp(
            _packet(src_port=40000, dst_port=5683, payload=_build_coap_post(path="/downlink", payload=payload)),
            self.config,
        )
        self.assertIsNotNone(line)
        assert line is not None
        self.assertIn("type=NN_TABLE", line)
        self.assertIn("target=1003", line)
        self.assertIn("count=2", line)
        self.assertIn("fd12:3456::10", line)
        self.assertIn("fd12:3456::11", line)

    def test_ack_hidden_by_default(self) -> None:
        line = summarize_observed_udp(
            _packet(src_port=5683, dst_port=61616, payload=_build_coap_ack()),
            self.config,
        )
        self.assertIsNone(line)

    def test_ack_can_be_shown(self) -> None:
        config = LiveDecodeConfig(
            coap_port=5683,
            uplink_resource_path=("uplink",),
            downlink_resource_path=("downlink",),
            show_acks=True,
        )
        line = summarize_observed_udp(
            _packet(src_port=5683, dst_port=61616, payload=_build_coap_ack()),
            config,
        )
        self.assertIsNotNone(line)
        assert line is not None
        self.assertIn("ack code=CHANGED", line)

    def test_backhaul_enqueue_retry_and_delivery_events(self) -> None:
        enqueue = summarize_backhaul_changes(
            {},
            {7: _backhaul_pending(uplink_id=7)},
            {},
            {},
            observed_at=dt.datetime(2026, 4, 23, 12, 35, 0),
        )
        self.assertEqual(1, len(enqueue))
        self.assertIn("backhaul enqueue uplink_id=7 gateway=77", enqueue[0])
        self.assertIn("type=REGISTRATION node=1001", enqueue[0])

        retry = summarize_backhaul_changes(
            {7: _backhaul_pending(uplink_id=7)},
            {
                7: _backhaul_pending(
                    uplink_id=7,
                    attempt_count=1,
                    last_attempt_at=1713875701,
                    next_attempt_at=1713875706,
                    last_error="HTTP 503",
                )
            },
            {},
            {},
            observed_at=dt.datetime(2026, 4, 23, 12, 35, 1),
        )
        self.assertEqual(1, len(retry))
        self.assertIn("backhaul retry uplink_id=7 attempts=1", retry[0])
        self.assertIn("error=HTTP 503", retry[0])

        delivered = summarize_backhaul_changes(
            {
                7: _backhaul_pending(
                    uplink_id=7,
                    attempt_count=1,
                    last_attempt_at=1713875701,
                    next_attempt_at=1713875706,
                    last_error="HTTP 503",
                )
            },
            {},
            {},
            {},
            observed_at=dt.datetime(2026, 4, 23, 12, 35, 2, 456000),
        )
        self.assertEqual(1, len(delivered))
        self.assertIn("12:35:02.456 backhaul delivered uplink_id=7 attempts=1", delivered[0])
        self.assertIn("type=REGISTRATION node=1001", delivered[0])

    def test_backhaul_dead_letter_event(self) -> None:
        line = summarize_backhaul_changes(
            {9: _backhaul_pending(uplink_id=9)},
            {},
            {},
            {9: BackhaulDeadLetterSnapshot(uplink_id=9, receipt_status=0x01, reason="HTTP 400", moved_at=1713875703)},
            observed_at=dt.datetime(2026, 4, 23, 12, 35, 3),
        )
        self.assertEqual(1, len(line))
        self.assertIn("backhaul dead-letter uplink_id=9 receipt_status=PERMANENT_REJECT", line[0])
        self.assertIn("reason=HTTP 400", line[0])
        self.assertIn("type=REGISTRATION node=1001", line[0])


if __name__ == "__main__":
    unittest.main()
