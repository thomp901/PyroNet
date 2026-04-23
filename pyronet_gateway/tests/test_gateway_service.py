from __future__ import annotations

import ipaddress
import struct
import tempfile
import unittest
import urllib.error
import urllib.request
from pathlib import Path

from pyronet_gateway.backhaul_client import HTTPBackhaulClient, PermanentBackhaulError, TransientBackhaulError
from pyronet_gateway.coap_intake import COAP_CODE_BAD_REQUEST, COAP_CODE_CHANGED, CoapIntakeService
from pyronet_gateway.config import BackhaulConfig, CoapConfig, GatewayConfig, RuntimeConfig
from pyronet_gateway.packet_codec import (
    GatewayRegistration,
    NodeUplinkEnvelope,
    PacketParseError,
    UplinkReceipt,
    node_uplink_header_size,
    parse_node_packet,
)
from pyronet_gateway.protocol.backhaul import (
    RECEIPT_DURABLE_INGEST,
    RECEIPT_PERMANENT_REJECT,
)
from pyronet_gateway.registration_worker import RegistrationWorker
from pyronet_gateway.retry_worker import OutboxRetryWorker, RetryPolicy
from pyronet_gateway.service import GatewayService

REGISTRATION_STRUCT = struct.Struct("<BBHffHB16s")
REPORT_STRUCT = struct.Struct("<BBHIBhHHHB")
PARENT_UPDATE_STRUCT = struct.Struct("<BBHI16s")
NEIGHBOR_ALERT_STRUCT = struct.Struct("<BBHIB")


class FakeBackhaulClient:
    def __init__(self, *, registration_plan=None, uplink_plan=None) -> None:
        self.registration_plan = list(registration_plan or [])
        self.uplink_plan = list(uplink_plan or [])
        self.registration_calls = []
        self.uplink_calls = []

    def send_gateway_registration(self, registration) -> None:
        self.registration_calls.append(registration)
        action = self.registration_plan.pop(0) if self.registration_plan else None
        if isinstance(action, Exception):
            raise action

    def send_uplink(self, envelope):
        self.uplink_calls.append(envelope)
        action = self.uplink_plan.pop(0) if self.uplink_plan else None
        if isinstance(action, Exception):
            raise action
        if callable(action):
            return action(envelope)
        if action is None:
            return UplinkReceipt(
                version=envelope.version,
                gateway_id=envelope.gateway_id,
                uplink_id=envelope.uplink_id,
                status=RECEIPT_DURABLE_INGEST,
            )
        return action


class CodecTests(unittest.TestCase):
    def test_gateway_registration_codec_round_trip(self) -> None:
        original = GatewayRegistration(
            version=1,
            gateway_id=7,
            timestamp=1_700_000_000,
            latitude=39.7684,
            longitude=-86.1581,
            sw_version=0x0102,
        )
        encoded = original.to_bytes()
        decoded = GatewayRegistration.from_bytes(encoded)
        self.assertEqual(18, len(encoded))
        self.assertEqual(original.gateway_id, decoded.gateway_id)
        self.assertEqual(original.sw_version, decoded.sw_version)

    def test_node_uplink_envelope_codec_round_trip(self) -> None:
        original = NodeUplinkEnvelope(
            version=1,
            gateway_id=7,
            uplink_id=42,
            received_at=1_700_000_001,
            observed_src_ipv6="fd12:3456::abcd",
            payload=build_report_packet(node_id=1001),
        )
        encoded = original.to_bytes()
        decoded = NodeUplinkEnvelope.from_bytes(encoded)
        self.assertEqual(original.uplink_id, decoded.uplink_id)
        self.assertEqual(original.payload, decoded.payload)
        self.assertEqual("fd12:3456::abcd", decoded.observed_src_ipv6)

    def test_uplink_receipt_codec_round_trip(self) -> None:
        original = UplinkReceipt(version=1, gateway_id=7, uplink_id=9, status=RECEIPT_DURABLE_INGEST)
        encoded = original.to_bytes()
        decoded = UplinkReceipt.from_bytes(encoded)
        self.assertEqual(original, decoded)

    def test_node_uplink_header_size_is_34_bytes(self) -> None:
        self.assertEqual(34, node_uplink_header_size())


class NodePacketHandlingTests(unittest.TestCase):
    def test_node_packet_type_acceptance_and_rejection(self) -> None:
        accepted_packets = [
            build_registration_packet(node_id=101, parent_ipv6="fd12:3456::10"),
            build_report_packet(node_id=102, packet_type=0x02),
            build_report_packet(node_id=103, packet_type=0x03),
            build_parent_update_packet(node_id=104, parent_ipv6="fd12:3456::20"),
        ]
        for packet in accepted_packets:
            parsed = parse_node_packet(packet)
            self.assertIn(parsed.packet_type, {0x01, 0x02, 0x03, 0x08})

        with self.assertRaises(PacketParseError):
            parse_node_packet(bytes([0x04, 0x01, 0x00]))

        with self.assertRaises(PacketParseError):
            parse_node_packet(build_neighbor_alert_packet(node_id=105))


class GatewayServiceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory()
        self.db_path = Path(self.tempdir.name) / "gateway.sqlite3"
        self.config = GatewayConfig(
            gateway_id=7,
            latitude=39.7684,
            longitude=-86.1581,
            sw_version=0x0102,
            coap=CoapConfig(bind_host="::", port=5683, resource_path="/uplink"),
            backhaul=BackhaulConfig(
                base_url="http://backhaul.example",
                http_timeout_seconds=5.0,
                registration_retry_base_delay_seconds=1,
                registration_retry_max_delay_seconds=10,
                uplink_retry_base_delay_seconds=1,
                uplink_retry_max_delay_seconds=10,
            ),
            runtime=RuntimeConfig(
                db_path=self.db_path,
                log_level="INFO",
                worker_poll_interval_seconds=0.1,
            ),
        )

    def tearDown(self) -> None:
        self.tempdir.cleanup()

    def test_outbox_persistence_across_restart(self) -> None:
        service = GatewayService.open(self.config, opened_at=1_700_000_000)
        decision = service.handle_node_packet(
            observed_src_ipv6="fd12:3456::1",
            raw_node_packet=build_report_packet(node_id=2001),
            received_at=1_700_000_000,
        )
        envelope_before = service.outbox_store.load_envelope(decision.uplink_id)
        service.close()

        restarted = GatewayService.open(self.config, opened_at=1_700_000_001)
        envelope_after = restarted.outbox_store.load_envelope(decision.uplink_id)
        self.assertEqual(envelope_before.payload, envelope_after.payload)
        self.assertEqual([decision.uplink_id], restarted.outbox_store.pending_uplink_ids())
        restarted.close()

    def test_duplicate_delivery_retries_same_uplink_id(self) -> None:
        service = GatewayService.open(self.config, opened_at=1_700_000_000)
        decision = service.handle_node_packet(
            observed_src_ipv6="fd12:3456::2",
            raw_node_packet=build_registration_packet(node_id=3001, parent_ipv6=None),
            received_at=1_700_000_000,
        )
        backhaul = FakeBackhaulClient(
            uplink_plan=[
                TransientBackhaulError("network timeout"),
                None,
            ]
        )
        registration_worker = RegistrationWorker(
            service=service,
            backhaul_client=backhaul,
            retry_policy=RetryPolicy(1, 10),
        )
        retry_worker = OutboxRetryWorker(
            outbox_store=service.outbox_store,
            backhaul_client=backhaul,
            retry_policy=RetryPolicy(1, 10),
            registration_worker=registration_worker,
        )

        retry_worker.run_once(1_700_000_000)
        retry_worker.run_once(1_700_000_001)

        self.assertEqual([], service.outbox_store.pending_uplink_ids())
        self.assertEqual(2, len(backhaul.uplink_calls))
        self.assertEqual(decision.uplink_id, backhaul.uplink_calls[0].uplink_id)
        self.assertEqual(backhaul.uplink_calls[0].uplink_id, backhaul.uplink_calls[1].uplink_id)
        service.close()

    def test_permanent_reject_vs_transient_failure_behavior(self) -> None:
        service = GatewayService.open(self.config, opened_at=1_700_000_010)
        decision = service.handle_node_packet(
            observed_src_ipv6="fd12:3456::3",
            raw_node_packet=build_report_packet(node_id=3002),
            received_at=1_700_000_010,
        )
        backhaul = FakeBackhaulClient(
            uplink_plan=[
                lambda envelope: UplinkReceipt(
                    version=envelope.version,
                    gateway_id=envelope.gateway_id,
                    uplink_id=envelope.uplink_id,
                    status=0x7F,
                ),
                lambda envelope: UplinkReceipt(
                    version=envelope.version,
                    gateway_id=envelope.gateway_id,
                    uplink_id=envelope.uplink_id,
                    status=RECEIPT_PERMANENT_REJECT,
                ),
            ]
        )
        retry_worker = OutboxRetryWorker(
            outbox_store=service.outbox_store,
            backhaul_client=backhaul,
            retry_policy=RetryPolicy(1, 10),
        )

        retry_worker.run_once(1_700_000_010)
        self.assertEqual([decision.uplink_id], service.outbox_store.pending_uplink_ids())
        retry_worker.run_once(1_700_000_011)
        self.assertEqual([], service.outbox_store.pending_uplink_ids())
        self.assertEqual([decision.uplink_id], service.outbox_store.dead_letter_uplink_ids())
        service.close()

    def test_node_state_overwrite_on_fresh_registration(self) -> None:
        service = GatewayService.open(self.config, opened_at=1_700_000_020)
        service.handle_node_packet(
            observed_src_ipv6="fd12:3456::100",
            raw_node_packet=build_registration_packet(node_id=4001, parent_ipv6="fd12:3456::10"),
            received_at=1_700_000_020,
        )
        service.handle_node_packet(
            observed_src_ipv6="fd12:3456::101",
            raw_node_packet=build_registration_packet(node_id=4001, parent_ipv6="fd12:3456::11"),
            received_at=1_700_000_021,
        )
        state = service.node_state_store.get(4001)
        self.assertEqual("fd12:3456::101", state.current_ipv6)
        self.assertEqual("fd12:3456::101", state.last_registration_ipv6)
        self.assertEqual("fd12:3456::11", state.parent_ipv6)
        service.close()

    def test_restart_recovery_preserves_monotonic_uplink_sequence(self) -> None:
        service = GatewayService.open(self.config, opened_at=1_700_000_030)
        first = service.handle_node_packet(
            observed_src_ipv6="fd12:3456::a",
            raw_node_packet=build_report_packet(node_id=5001),
            received_at=1_700_000_030,
        )
        service.close()

        restarted = GatewayService.open(self.config, opened_at=1_700_000_031)
        second = restarted.handle_node_packet(
            observed_src_ipv6="fd12:3456::b",
            raw_node_packet=build_report_packet(node_id=5002),
            received_at=1_700_000_031,
        )
        self.assertGreater(second.uplink_id, first.uplink_id)
        restarted.close()

    def test_backhaul_recovery_triggers_gateway_registration(self) -> None:
        service = GatewayService.open(self.config, opened_at=1_700_000_040)
        service.handle_node_packet(
            observed_src_ipv6="fd12:3456::c",
            raw_node_packet=build_report_packet(node_id=6001),
            received_at=1_700_000_040,
        )
        backhaul = FakeBackhaulClient(
            registration_plan=[
                None,
                None,
            ],
            uplink_plan=[
                TransientBackhaulError("dns failure"),
            ],
        )
        registration_worker = RegistrationWorker(
            service=service,
            backhaul_client=backhaul,
            retry_policy=RetryPolicy(1, 10),
        )
        retry_worker = OutboxRetryWorker(
            outbox_store=service.outbox_store,
            backhaul_client=backhaul,
            retry_policy=RetryPolicy(1, 10),
            registration_worker=registration_worker,
        )

        self.assertTrue(registration_worker.run_once(1_700_000_040))
        retry_worker.run_once(1_700_000_041)
        self.assertFalse(registration_worker.connected)
        self.assertTrue(registration_worker.run_once(1_700_000_041))
        self.assertEqual(2, len(backhaul.registration_calls))
        service.close()

    def test_coap_intake_service_validates_path_and_ignores_neighbor_alert(self) -> None:
        service = GatewayService.open(self.config, opened_at=1_700_000_050)
        intake = CoapIntakeService(
            gateway_service=service,
            resource_path="/uplink",
            duplicate_cache_ttl_seconds=60.0,
            duplicate_cache_max_entries=64,
        )

        good = intake.handle_datagram(
            src_ipv6="fd12:3456::d",
            datagram=build_coap_post("/uplink", build_report_packet(node_id=7001)),
            received_at=1_700_000_050,
        )
        self.assertTrue(good.accepted)
        self.assertEqual(COAP_CODE_CHANGED, good.response_bytes[1])
        pending_before_neighbor = service.outbox_store.pending_uplink_ids()

        wrong_path = intake.handle_datagram(
            src_ipv6="fd12:3456::d",
            datagram=build_coap_post("/wrong", build_report_packet(node_id=7002)),
            received_at=1_700_000_051,
        )
        self.assertFalse(wrong_path.accepted)

        neighbor = intake.handle_datagram(
            src_ipv6="fd12:3456::d",
            datagram=build_coap_post("/uplink", build_neighbor_alert_packet(node_id=7003)),
            received_at=1_700_000_052,
        )
        self.assertFalse(neighbor.accepted)
        self.assertEqual(COAP_CODE_BAD_REQUEST, neighbor.response_bytes[1])
        self.assertEqual(pending_before_neighbor, service.outbox_store.pending_uplink_ids())
        self.assertIsNone(service.node_state_store.get(7003))
        service.close()

    def test_malformed_node_payload_returns_bad_request(self) -> None:
        service = GatewayService.open(self.config, opened_at=1_700_000_060)
        intake = CoapIntakeService(gateway_service=service, resource_path="/uplink")
        result = intake.handle_datagram(
            src_ipv6="fd12:3456::e",
            datagram=build_coap_post("/uplink", b"\x01\x01"),
            received_at=1_700_000_060,
        )
        self.assertFalse(result.accepted)
        self.assertEqual(COAP_CODE_BAD_REQUEST, result.response_bytes[1])
        service.close()

    def test_duplicate_confirmable_uplink_is_enqueued_once(self) -> None:
        service = GatewayService.open(self.config, opened_at=1_700_000_070)
        intake = CoapIntakeService(
            gateway_service=service,
            resource_path="/uplink",
            duplicate_cache_ttl_seconds=60.0,
            duplicate_cache_max_entries=64,
        )
        datagram = build_coap_post(
            "/uplink",
            build_report_packet(node_id=8001),
            message_id=0x1234,
            token=b"\xaa",
            msg_type=0,
        )

        first = intake.handle_datagram(
            src_ipv6="fd12:3456::8",
            datagram=datagram,
            received_at=1_700_000_070,
        )
        second = intake.handle_datagram(
            src_ipv6="fd12:3456::8",
            datagram=datagram,
            received_at=1_700_000_071,
        )

        self.assertTrue(first.accepted)
        self.assertTrue(second.accepted)
        self.assertEqual(first.response_bytes, second.response_bytes)
        self.assertEqual(first.uplink_id, second.uplink_id)
        self.assertEqual([first.uplink_id], service.outbox_store.pending_uplink_ids())
        service.close()

    def test_different_message_id_or_token_is_new_request(self) -> None:
        service = GatewayService.open(self.config, opened_at=1_700_000_080)
        intake = CoapIntakeService(
            gateway_service=service,
            resource_path="/uplink",
            duplicate_cache_ttl_seconds=60.0,
            duplicate_cache_max_entries=64,
        )
        base_payload = build_report_packet(node_id=8002)
        first = intake.handle_datagram(
            src_ipv6="fd12:3456::9",
            datagram=build_coap_post("/uplink", base_payload, message_id=0x1111, token=b"\xaa"),
            received_at=1_700_000_080,
        )
        second = intake.handle_datagram(
            src_ipv6="fd12:3456::9",
            datagram=build_coap_post("/uplink", base_payload, message_id=0x1112, token=b"\xaa"),
            received_at=1_700_000_081,
        )
        third = intake.handle_datagram(
            src_ipv6="fd12:3456::9",
            datagram=build_coap_post("/uplink", base_payload, message_id=0x1112, token=b"\xbb"),
            received_at=1_700_000_082,
        )

        self.assertNotEqual(first.uplink_id, second.uplink_id)
        self.assertNotEqual(second.uplink_id, third.uplink_id)
        self.assertEqual(
            [first.uplink_id, second.uplink_id, third.uplink_id],
            service.outbox_store.pending_uplink_ids(),
        )
        service.close()

    def test_http_500_and_malformed_receipt_schedule_retry(self) -> None:
        service = GatewayService.open(self.config, opened_at=1_700_000_090)
        decision_500 = service.handle_node_packet(
            observed_src_ipv6="fd12:3456::10",
            raw_node_packet=build_report_packet(node_id=9001),
            received_at=1_700_000_090,
        )
        decision_malformed = service.handle_node_packet(
            observed_src_ipv6="fd12:3456::11",
            raw_node_packet=build_report_packet(node_id=9002),
            received_at=1_700_000_091,
        )
        backhaul = FakeBackhaulClient(
            uplink_plan=[
                TransientBackhaulError("HTTP 500"),
                TransientBackhaulError("invalid uplink receipt: malformed"),
            ]
        )
        retry_worker = OutboxRetryWorker(
            outbox_store=service.outbox_store,
            backhaul_client=backhaul,
            retry_policy=RetryPolicy(1, 10),
        )

        retry_worker.run_once(1_700_000_092, limit=2)
        pending = service.outbox_store.pending_uplink_ids()
        self.assertEqual([decision_500.uplink_id, decision_malformed.uplink_id], pending)
        rows = service.database.connection.execute(
            "SELECT uplink_id, last_error FROM outbox ORDER BY uplink_id ASC"
        ).fetchall()
        self.assertEqual("HTTP 500", rows[0]["last_error"])
        self.assertIn("invalid uplink receipt", rows[1]["last_error"])
        service.close()

    def test_http_4xx_moves_to_dead_letter_with_reason(self) -> None:
        service = GatewayService.open(self.config, opened_at=1_700_000_100)
        uplink_ids = []
        for offset, code in enumerate([400, 401, 403, 404, 415]):
            decision = service.handle_node_packet(
                observed_src_ipv6=f"fd12:3456::{20 + offset}",
                raw_node_packet=build_report_packet(node_id=9100 + offset),
                received_at=1_700_000_100 + offset,
            )
            uplink_ids.append((decision.uplink_id, code))
        backhaul = FakeBackhaulClient(
            uplink_plan=[PermanentBackhaulError(f"HTTP {code}") for _uplink_id, code in uplink_ids]
        )
        retry_worker = OutboxRetryWorker(
            outbox_store=service.outbox_store,
            backhaul_client=backhaul,
            retry_policy=RetryPolicy(1, 10),
        )

        retry_worker.run_once(1_700_000_110, limit=10)
        self.assertEqual([], service.outbox_store.pending_uplink_ids())
        records = service.dead_letter_store.list_records()
        self.assertEqual([uplink_id for uplink_id, _code in uplink_ids], [record.uplink_id for record in records])
        for record, (_uplink_id, code) in zip(records, uplink_ids, strict=True):
            self.assertIn(f"HTTP {code}", record.reason)
        service.close()


class HTTPBackhaulClientClassificationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.client = HTTPBackhaulClient(base_url="http://backhaul.example", timeout_seconds=5.0)
        self.original_urlopen = urllib.request.urlopen

    def tearDown(self) -> None:
        urllib.request.urlopen = self.original_urlopen

    def test_registration_success_posts_expected_request(self) -> None:
        seen = {}

        def fake_urlopen(request, timeout=None):
            seen["request"] = request
            seen["timeout"] = timeout
            return FakeHTTPResponse(status=204, body=b"")

        urllib.request.urlopen = fake_urlopen
        registration = GatewayRegistration(
            version=1,
            gateway_id=7,
            timestamp=1,
            latitude=0.0,
            longitude=0.0,
            sw_version=0x0102,
        )

        self.client.send_gateway_registration(registration)

        request = seen["request"]
        self.assertEqual("http://backhaul.example/api/v1/gateways/register", request.full_url)
        self.assertEqual("POST", request.get_method())
        self.assertEqual("application/octet-stream", request.get_header("Content-type"))
        self.assertEqual(registration.to_bytes(), request.data)
        self.assertEqual(5.0, seen["timeout"])

    def test_uplink_success_returns_receipt(self) -> None:
        envelope = NodeUplinkEnvelope(
            version=1,
            gateway_id=7,
            uplink_id=42,
            received_at=1_700_000_001,
            observed_src_ipv6="fd12:3456::abcd",
            payload=build_report_packet(node_id=1001),
        )
        receipt = UplinkReceipt(
            version=envelope.version,
            gateway_id=envelope.gateway_id,
            uplink_id=envelope.uplink_id,
            status=RECEIPT_DURABLE_INGEST,
        )

        urllib.request.urlopen = lambda _request, timeout=None: FakeHTTPResponse(status=200, body=receipt.to_bytes())

        actual = self.client.send_uplink(envelope)

        self.assertEqual(receipt, actual)

    def test_connection_failure_is_transient(self) -> None:
        def fake_urlopen(_request, timeout=None):
            raise urllib.error.URLError("dns failure")

        urllib.request.urlopen = fake_urlopen

        with self.assertRaisesRegex(TransientBackhaulError, "connection failure: dns failure"):
            self.client.send_gateway_registration(
                GatewayRegistration(
                    version=1,
                    gateway_id=7,
                    timestamp=1,
                    latitude=0.0,
                    longitude=0.0,
                    sw_version=0x0102,
                )
            )

    def test_timeout_is_transient(self) -> None:
        def fake_urlopen(_request, timeout=None):
            raise TimeoutError()

        urllib.request.urlopen = fake_urlopen

        with self.assertRaisesRegex(TransientBackhaulError, "network timeout"):
            self.client.send_gateway_registration(
                GatewayRegistration(
                    version=1,
                    gateway_id=7,
                    timestamp=1,
                    latitude=0.0,
                    longitude=0.0,
                    sw_version=0x0102,
                )
            )

    def test_missing_uplink_receipt_body_is_transient(self) -> None:
        envelope = NodeUplinkEnvelope(
            version=1,
            gateway_id=7,
            uplink_id=42,
            received_at=1_700_000_001,
            observed_src_ipv6="fd12:3456::abcd",
            payload=build_report_packet(node_id=1001),
        )

        urllib.request.urlopen = lambda _request, timeout=None: FakeHTTPResponse(status=200, body=b"")

        with self.assertRaisesRegex(TransientBackhaulError, "missing uplink receipt body"):
            self.client.send_uplink(envelope)

    def test_malformed_uplink_receipt_is_transient(self) -> None:
        envelope = NodeUplinkEnvelope(
            version=1,
            gateway_id=7,
            uplink_id=42,
            received_at=1_700_000_001,
            observed_src_ipv6="fd12:3456::abcd",
            payload=build_report_packet(node_id=1001),
        )

        urllib.request.urlopen = lambda _request, timeout=None: FakeHTTPResponse(status=200, body=b"\x00")

        with self.assertRaisesRegex(TransientBackhaulError, "invalid uplink receipt"):
            self.client.send_uplink(envelope)

    def test_mismatched_uplink_receipt_is_transient(self) -> None:
        envelope = NodeUplinkEnvelope(
            version=1,
            gateway_id=7,
            uplink_id=42,
            received_at=1_700_000_001,
            observed_src_ipv6="fd12:3456::abcd",
            payload=build_report_packet(node_id=1001),
        )
        mismatched = UplinkReceipt(
            version=envelope.version,
            gateway_id=envelope.gateway_id,
            uplink_id=envelope.uplink_id + 1,
            status=RECEIPT_DURABLE_INGEST,
        )

        urllib.request.urlopen = lambda _request, timeout=None: FakeHTTPResponse(
            status=200, body=mismatched.to_bytes()
        )

        with self.assertRaisesRegex(
            TransientBackhaulError, "uplink receipt does not match the posted envelope"
        ):
            self.client.send_uplink(envelope)

    def test_http_500_is_transient(self) -> None:
        def fake_urlopen(_request, timeout=None):
            raise urllib.error.HTTPError(
                url="http://backhaul.example/api/v1/uplinks",
                code=500,
                msg="server error",
                hdrs=None,
                fp=None,
            )

        urllib.request.urlopen = fake_urlopen
        with self.assertRaises(TransientBackhaulError):
            self.client.send_gateway_registration(
                GatewayRegistration(
                    version=1,
                    gateway_id=7,
                    timestamp=1,
                    latitude=0.0,
                    longitude=0.0,
                    sw_version=0x0102,
                )
            )

    def test_http_400_is_permanent(self) -> None:
        def fake_urlopen(_request, timeout=None):
            raise urllib.error.HTTPError(
                url="http://backhaul.example/api/v1/uplinks",
                code=400,
                msg="bad request",
                hdrs=None,
                fp=None,
            )

        urllib.request.urlopen = fake_urlopen
        with self.assertRaises(PermanentBackhaulError):
            self.client.send_gateway_registration(
                GatewayRegistration(
                    version=1,
                    gateway_id=7,
                    timestamp=1,
                    latitude=0.0,
                    longitude=0.0,
                    sw_version=0x0102,
                )
            )


class FakeHTTPResponse:
    def __init__(self, *, status: int, body: bytes) -> None:
        self.status = status
        self._body = body

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        return None

    def getcode(self) -> int:
        return self.status

    def read(self) -> bytes:
        return self._body


def build_registration_packet(*, node_id: int, parent_ipv6: str | None) -> bytes:
    parent = ipaddress.IPv6Address(parent_ipv6).packed if parent_ipv6 else b"\x00" * 16
    return REGISTRATION_STRUCT.pack(
        0x01,
        0x01,
        node_id,
        39.0,
        -86.0,
        0x0102,
        88,
        parent,
    )


def build_report_packet(*, node_id: int, packet_type: int = 0x02) -> bytes:
    return REPORT_STRUCT.pack(
        packet_type,
        0x01,
        node_id,
        1_700_000_000,
        3,
        2450,
        5000,
        123,
        55,
        90,
    )


def build_parent_update_packet(*, node_id: int, parent_ipv6: str | None) -> bytes:
    parent = ipaddress.IPv6Address(parent_ipv6).packed if parent_ipv6 else b"\x00" * 16
    return PARENT_UPDATE_STRUCT.pack(0x08, 0x01, node_id, 1_700_000_000, parent)


def build_neighbor_alert_packet(*, node_id: int) -> bytes:
    return NEIGHBOR_ALERT_STRUCT.pack(0x07, 0x01, node_id, 1_700_000_000, 5)


def build_coap_post(
    path: str,
    payload: bytes,
    *,
    message_id: int = 0x1234,
    token: bytes = b"\xaa",
    msg_type: int = 0,
) -> bytes:
    header = bytes([(1 << 6) | (msg_type << 4) | len(token), 0x02]) + message_id.to_bytes(2, "big") + token
    options = b""
    previous_number = 0
    for segment in [part for part in path.strip("/").split("/") if part]:
        option_number = 11
        delta = option_number - previous_number
        value = segment.encode("utf-8")
        options += _encode_option(delta, value)
        previous_number = option_number
    return header + options + b"\xFF" + payload


def _encode_option(delta: int, value: bytes) -> bytes:
    if delta > 12 or len(value) > 12:
        raise ValueError("test helper only supports small options")
    return bytes([(delta << 4) | len(value)]) + value


if __name__ == "__main__":
    unittest.main()
