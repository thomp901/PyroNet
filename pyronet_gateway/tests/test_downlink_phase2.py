from __future__ import annotations

import json
import socket
import tempfile
import unittest
from pathlib import Path

from pyronet_gateway.coap_downlink_client import CoapDownlinkClient
from pyronet_gateway.config import BackhaulConfig, CoapConfig, GatewayConfig, HttpApiConfig, RuntimeConfig
from pyronet_gateway.downlink_delivery_service import DownlinkDeliveryService
from pyronet_gateway.downlink_http_api import DownlinkHttpApi
from pyronet_gateway.downlink_packet_codec import (
    CONFIG_UPDATE,
    NN_TABLE_HEADER,
    TIME_SYNC,
    encode_config_update,
    encode_nn_table_update,
    encode_time_sync,
)
from pyronet_gateway.downlink_request_validation import (
    DownlinkValidationError,
    validate_config_update_request,
    validate_nn_table_request,
    validate_time_sync_request,
)
from pyronet_gateway.service import GatewayService


class FakeCoapClient:
    def __init__(self, scripted_results=None) -> None:
        self.scripted_results = list(scripted_results or [])
        self.calls = []

    def send_confirmable(self, *, target_ipv6: str, payload: bytes):
        self.calls.append((target_ipv6, payload))
        action = self.scripted_results.pop(0) if self.scripted_results else None
        if isinstance(action, Exception):
            raise action
        if action is None:
            return type("Result", (), {"success": True, "error_category": None, "error_detail": None})()
        return action


class ScriptedSocket:
    def __init__(self, script, *, send_error: OSError | None = None) -> None:
        self.script = list(script)
        self.send_error = send_error
        self.sent_datagrams = []
        self.timeout = None
        self.closed = False

    def sendto(self, datagram, address) -> None:
        self.sent_datagrams.append((datagram, address))
        if self.send_error is not None:
            raise self.send_error

    def settimeout(self, timeout) -> None:
        self.timeout = timeout

    def recvfrom(self, _size):
        if not self.script:
            raise socket.timeout()
        action = self.script.pop(0)
        if isinstance(action, Exception):
            raise action
        if callable(action):
            return action(self), ("fd12:3456::1", 5683)
        return action, ("fd12:3456::1", 5683)

    def close(self) -> None:
        self.closed = True


class DownlinkCodecTests(unittest.TestCase):
    def test_encode_nn_table_update(self) -> None:
        payload = encode_nn_table_update(
            version=1,
            target_node_id=42,
            neighbor_ipv6s=["fd12:3456::1", "fd12:3456::2"],
        )
        self.assertEqual(5 + 32, len(payload))
        decoded = NN_TABLE_HEADER.unpack(payload[: NN_TABLE_HEADER.size])
        self.assertEqual((0x04, 1, 42, 2), decoded)

    def test_encode_time_sync(self) -> None:
        payload = encode_time_sync(version=1, epoch=1234)
        self.assertEqual(TIME_SYNC.size, len(payload))
        self.assertEqual((0x05, 1, 1234), TIME_SYNC.unpack(payload))

    def test_encode_config_update(self) -> None:
        request = validate_config_update_request(
            {
                "target_node_id": 10,
                "config_id": 77,
                "l2_temp_thresh": -100,
                "l2_humidity_thresh": 1,
                "l2_voc_thresh": 2,
                "l3_temp_thresh": -200,
                "l3_humidity_thresh": 3,
                "l3_voc_thresh": 4,
                "l4_voc_thresh": 5,
                "l5_voc_thresh": 6,
                "l5_pm25_thresh": 7,
            }
        )
        payload = encode_config_update(version=1, request=request)
        self.assertEqual(CONFIG_UPDATE.size, len(payload))
        self.assertEqual(0x06, payload[0])


class DownlinkValidationTests(unittest.TestCase):
    def test_validate_nn_table_request(self) -> None:
        request = validate_nn_table_request({"target_node_id": 9, "neighbor_node_ids": [1, 2, 3]})
        self.assertEqual([1, 2, 3], request.neighbor_node_ids)

    def test_validate_time_sync_request(self) -> None:
        request = validate_time_sync_request({"target_node_id": 9, "epoch": 123})
        self.assertEqual(123, request.epoch)

    def test_validate_config_update_request(self) -> None:
        request = validate_config_update_request(
            {
                "target_node_id": 9,
                "config_id": 1,
                "l2_temp_thresh": 0,
                "l2_humidity_thresh": 0,
                "l2_voc_thresh": 0,
                "l3_temp_thresh": 0,
                "l3_humidity_thresh": 0,
                "l3_voc_thresh": 0,
                "l4_voc_thresh": 0,
                "l5_voc_thresh": 0,
                "l5_pm25_thresh": 0,
            }
        )
        self.assertEqual(1, request.config_id)

    def test_invalid_neighbor_request_raises(self) -> None:
        with self.assertRaises(DownlinkValidationError):
            validate_nn_table_request({"target_node_id": 9, "neighbor_node_ids": ["bad"]})


class CoapDownlinkClientTests(unittest.TestCase):
    def test_success_path(self) -> None:
        scripted_socket = ScriptedSocket([lambda sock: build_empty_ack(sock.sent_datagrams[-1][0])])
        client = CoapDownlinkClient(
            port=5683,
            resource_path="/downlink",
            ack_timeout_seconds=0.1,
            max_retransmit=1,
            socket_factory=lambda *_args: scripted_socket,
            monotonic=DeterministicClock(),
        )
        result = client.send_confirmable(target_ipv6="fd12:3456::1", payload=b"\x05\x01\x00\x00\x00\x00")
        self.assertTrue(result.success)
        self.assertEqual(1, len(scripted_socket.sent_datagrams))

    def test_retry_exhaustion_returns_failure(self) -> None:
        scripted_socket = ScriptedSocket([socket.timeout(), socket.timeout(), socket.timeout()])
        client = CoapDownlinkClient(
            port=5683,
            resource_path="/downlink",
            ack_timeout_seconds=0.1,
            max_retransmit=2,
            socket_factory=lambda *_args: scripted_socket,
            monotonic=DeterministicClock(),
        )
        result = client.send_confirmable(target_ipv6="fd12:3456::1", payload=b"\x05\x01\x00\x00\x00\x00")
        self.assertFalse(result.success)
        self.assertEqual("target_stale_or_unreachable", result.error_category)
        self.assertEqual(3, len(scripted_socket.sent_datagrams))

    def test_unreachable_send_returns_failure(self) -> None:
        scripted_socket = ScriptedSocket([], send_error=OSError("network unreachable"))
        client = CoapDownlinkClient(
            port=5683,
            resource_path="/downlink",
            ack_timeout_seconds=0.1,
            max_retransmit=1,
            socket_factory=lambda *_args: scripted_socket,
            monotonic=DeterministicClock(),
        )
        result = client.send_confirmable(target_ipv6="fd12:3456::1", payload=b"\x05\x01\x00\x00\x00\x00")
        self.assertFalse(result.success)
        self.assertEqual("target_unreachable", result.error_category)


class DownlinkServiceIntegrationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory()
        self.db_path = Path(self.tempdir.name) / "gateway.sqlite3"
        self.config = GatewayConfig(
            gateway_id=7,
            latitude=39.7684,
            longitude=-86.1581,
            sw_version=0x0102,
            coap=CoapConfig(bind_host="::", port=5683, resource_path="/uplink"),
            backhaul=BackhaulConfig(base_url="http://csp.example"),
            runtime=RuntimeConfig(db_path=self.db_path),
            http_api=HttpApiConfig(bind_host="::1", port=8081, max_request_body_bytes=65536),
        )
        self.service = GatewayService.open(self.config, opened_at=1_700_000_000)

    def tearDown(self) -> None:
        self.service.close()
        self.tempdir.cleanup()

    def _register_node(self, node_id: int, ipv6: str) -> None:
        self.service.handle_node_packet(
            observed_src_ipv6=ipv6,
            raw_node_packet=build_registration_packet(node_id),
            received_at=1_700_000_000,
        )

    def test_target_mapping_lookup_and_success_delivery(self) -> None:
        self._register_node(10, "fd12:3456::10")
        coap = FakeCoapClient()
        delivery = DownlinkDeliveryService(
            node_state_store=self.service.node_state_store,
            audit_store=self.service.downlink_audit_store,
            coap_downlink_client=coap,
            node_packet_version=1,
        )
        api = DownlinkHttpApi(delivery_service=delivery, max_request_body_bytes=4096)
        status, _headers, body = api.handle_request(
            method="POST",
            path="/api/v1/downlinks/time-sync",
            body=json.dumps({"target_node_id": 10, "epoch": 1234}).encode("utf-8"),
            now=1_700_000_001,
        )
        payload = json.loads(body)
        self.assertEqual(200, status)
        self.assertEqual("accepted_and_delivered", payload["delivery_result"])
        self.assertEqual("fd12:3456::10", payload["target_ipv6"])
        self.assertEqual("fd12:3456::10", coap.calls[0][0])
        self.assertEqual(1, len(self.service.downlink_audit_store.list_attempts()))

    def test_unknown_target_returns_404(self) -> None:
        coap = FakeCoapClient()
        delivery = DownlinkDeliveryService(
            node_state_store=self.service.node_state_store,
            audit_store=self.service.downlink_audit_store,
            coap_downlink_client=coap,
            node_packet_version=1,
        )
        api = DownlinkHttpApi(delivery_service=delivery, max_request_body_bytes=4096)
        status, _headers, body = api.handle_request(
            method="POST",
            path="/api/v1/downlinks/time-sync",
            body=json.dumps({"target_node_id": 77, "epoch": 1234}).encode("utf-8"),
            now=1_700_000_001,
        )
        payload = json.loads(body)
        self.assertEqual(404, status)
        self.assertEqual("target_unknown", payload["delivery_result"])

    def test_neighbor_resolution_failure_returns_409(self) -> None:
        self._register_node(10, "fd12:3456::10")
        self._register_node(11, "fd12:3456::11")
        delivery = DownlinkDeliveryService(
            node_state_store=self.service.node_state_store,
            audit_store=self.service.downlink_audit_store,
            coap_downlink_client=FakeCoapClient(),
            node_packet_version=1,
        )
        api = DownlinkHttpApi(delivery_service=delivery, max_request_body_bytes=4096)
        status, _headers, body = api.handle_request(
            method="POST",
            path="/api/v1/downlinks/nn-table",
            body=json.dumps({"target_node_id": 10, "neighbor_node_ids": [11, 12]}).encode("utf-8"),
            now=1_700_000_001,
        )
        payload = json.loads(body)
        self.assertEqual(409, status)
        self.assertEqual("stale_precondition", payload["delivery_result"])
        self.assertEqual([12], payload["missing_neighbor_node_ids"])

    def test_fresh_registration_changes_delivery_ipv6_immediately(self) -> None:
        self._register_node(10, "fd12:3456::10")
        self.service.handle_node_packet(
            observed_src_ipv6="fd12:3456::99",
            raw_node_packet=build_registration_packet(10),
            received_at=1_700_000_100,
        )
        coap = FakeCoapClient()
        delivery = DownlinkDeliveryService(
            node_state_store=self.service.node_state_store,
            audit_store=self.service.downlink_audit_store,
            coap_downlink_client=coap,
            node_packet_version=1,
        )
        result = delivery.handle_request(
            request_type="time-sync",
            request_body=json.dumps({"target_node_id": 10, "epoch": 55}).encode("utf-8"),
            now=1_700_000_101,
        )
        self.assertEqual(200, result.status_code)
        self.assertEqual("fd12:3456::99", result.body["target_ipv6"])
        self.assertEqual("fd12:3456::99", coap.calls[0][0])

    def test_coap_delivery_failure_becomes_502_and_audit_row(self) -> None:
        self._register_node(10, "fd12:3456::10")
        failure = type(
            "Result",
            (),
            {"success": False, "error_category": "target_stale_or_unreachable", "error_detail": "timeout"},
        )()
        delivery = DownlinkDeliveryService(
            node_state_store=self.service.node_state_store,
            audit_store=self.service.downlink_audit_store,
            coap_downlink_client=FakeCoapClient([failure]),
            node_packet_version=1,
        )
        result = delivery.handle_request(
            request_type="time-sync",
            request_body=json.dumps({"target_node_id": 10, "epoch": 55}).encode("utf-8"),
            now=1_700_000_101,
        )
        self.assertEqual(502, result.status_code)
        attempts = self.service.downlink_audit_store.list_attempts()
        self.assertEqual(1, len(attempts))
        self.assertEqual("target_stale_or_unreachable", attempts[0].status)

    def test_invalid_request_returns_400(self) -> None:
        delivery = DownlinkDeliveryService(
            node_state_store=self.service.node_state_store,
            audit_store=self.service.downlink_audit_store,
            coap_downlink_client=FakeCoapClient(),
            node_packet_version=1,
        )
        api = DownlinkHttpApi(delivery_service=delivery, max_request_body_bytes=4096)
        status, _headers, body = api.handle_request(
            method="POST",
            path="/api/v1/downlinks/config",
            body=b"[]",
            now=1_700_000_001,
        )
        payload = json.loads(body)
        self.assertEqual(400, status)
        self.assertEqual("invalid_request", payload["delivery_result"])


class DeterministicClock:
    def __init__(self) -> None:
        self.now = 0.0

    def __call__(self) -> float:
        self.now += 0.01
        return self.now


def build_empty_ack(sent_datagram: bytes) -> bytes:
    token_length = sent_datagram[0] & 0x0F
    token = sent_datagram[4 : 4 + token_length]
    message_id = sent_datagram[2:4]
    return bytes([(1 << 6) | (2 << 4) | len(token), 0x00]) + message_id + token


def build_registration_packet(node_id: int) -> bytes:
    return (
        b"\x01\x01"
        + node_id.to_bytes(2, "little")
        + b"\x00\x00\x00\x00"
        + b"\x00\x00\x00\x00"
        + b"\x00\x00"
        + b"\x64"
        + (b"\x00" * 16)
    )


if __name__ == "__main__":
    unittest.main()
