from __future__ import annotations

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
    validate_node_downlink_payload,
)
from pyronet_gateway.downlink_request_validation import validate_config_update_request
from pyronet_gateway.protocol.backhaul import (
    DOWNLINK_STATUS_DELIVERED,
    DOWNLINK_STATUS_MESH_DELIVERY_FAILED,
    DOWNLINK_STATUS_PERMANENT_REJECT,
    DOWNLINK_STATUS_UNKNOWN_NODE,
    DownlinkRequest,
    DownlinkResult,
    downlink_request_header_size,
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

    def test_downlink_request_round_trip(self) -> None:
        original = DownlinkRequest(
            version=1,
            gateway_id=7,
            downlink_id=9,
            target_node_id=42,
            created_at=123,
            payload=encode_time_sync(version=1, epoch=55),
        )
        decoded = DownlinkRequest.from_bytes(original.to_bytes())
        self.assertEqual(original, decoded)

    def test_downlink_result_round_trip(self) -> None:
        original = DownlinkResult(
            version=1,
            gateway_id=7,
            downlink_id=9,
            target_node_id=42,
            status=DOWNLINK_STATUS_DELIVERED,
            completed_at=456,
        )
        decoded = DownlinkResult.from_bytes(original.to_bytes())
        self.assertEqual(original, decoded)

    def test_downlink_request_header_size_is_20_bytes(self) -> None:
        self.assertEqual(20, downlink_request_header_size())

    def test_validate_node_downlink_payload_rejects_bad_nn_length(self) -> None:
        with self.assertRaises(ValueError):
            validate_node_downlink_payload(b"\x04\x01\x2a\x00\x02" + (b"\x00" * 16))


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
            backhaul=BackhaulConfig(base_url="http://backhaul.example"),
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

    def _delivery_service(self, coap_client) -> DownlinkDeliveryService:
        return DownlinkDeliveryService(
            gateway_id=self.config.gateway_id,
            backhaul_version=self.config.backhaul_version,
            node_state_store=self.service.node_state_store,
            result_store=self.service.downlink_audit_store,
            coap_downlink_client=coap_client,
        )

    def _api(self, coap_client) -> DownlinkHttpApi:
        return DownlinkHttpApi(delivery_service=self._delivery_service(coap_client), max_request_body_bytes=4096)

    def test_successful_delivery_returns_terminal_0x85(self) -> None:
        self._register_node(10, "fd12:3456::10")
        payload = encode_time_sync(version=1, epoch=1234)
        request = DownlinkRequest(
            version=1,
            gateway_id=7,
            downlink_id=100,
            target_node_id=10,
            created_at=1_700_000_001,
            payload=payload,
        )
        coap = FakeCoapClient()
        api = self._api(coap)

        status, headers, body = api.handle_request(
            method="POST",
            path="/api/v1/downlinks",
            content_type="application/octet-stream",
            body=request.to_bytes(),
            now=1_700_000_002,
        )

        result = DownlinkResult.from_bytes(body)
        self.assertEqual(200, status)
        self.assertEqual("application/octet-stream", headers["Content-Type"])
        self.assertEqual(DOWNLINK_STATUS_DELIVERED, result.status)
        self.assertEqual("fd12:3456::10", coap.calls[0][0])
        self.assertEqual(payload, coap.calls[0][1])
        stored = self.service.downlink_audit_store.list_terminal_results()
        self.assertEqual(1, len(stored))
        self.assertEqual(body, stored[0].result_body)

    def test_unknown_target_returns_terminal_unknown_node(self) -> None:
        request = DownlinkRequest(
            version=1,
            gateway_id=7,
            downlink_id=101,
            target_node_id=77,
            created_at=1_700_000_001,
            payload=encode_time_sync(version=1, epoch=1234),
        )
        api = self._api(FakeCoapClient())

        status, _headers, body = api.handle_request(
            method="POST",
            path="/api/v1/downlinks",
            content_type="application/octet-stream",
            body=request.to_bytes(),
            now=1_700_000_002,
        )

        result = DownlinkResult.from_bytes(body)
        self.assertEqual(200, status)
        self.assertEqual(DOWNLINK_STATUS_UNKNOWN_NODE, result.status)

    def test_nn_table_target_mismatch_returns_permanent_reject(self) -> None:
        self._register_node(10, "fd12:3456::10")
        payload = encode_nn_table_update(version=1, target_node_id=11, neighbor_ipv6s=[])
        request = DownlinkRequest(
            version=1,
            gateway_id=7,
            downlink_id=102,
            target_node_id=10,
            created_at=1_700_000_001,
            payload=payload,
        )
        api = self._api(FakeCoapClient())

        status, _headers, body = api.handle_request(
            method="POST",
            path="/api/v1/downlinks",
            content_type="application/octet-stream",
            body=request.to_bytes(),
            now=1_700_000_002,
        )

        result = DownlinkResult.from_bytes(body)
        self.assertEqual(200, status)
        self.assertEqual(DOWNLINK_STATUS_PERMANENT_REJECT, result.status)

    def test_mesh_delivery_failure_returns_terminal_failure(self) -> None:
        self._register_node(10, "fd12:3456::10")
        failure = type(
            "Result",
            (),
            {"success": False, "error_category": "target_stale_or_unreachable", "error_detail": "timeout"},
        )()
        request = DownlinkRequest(
            version=1,
            gateway_id=7,
            downlink_id=103,
            target_node_id=10,
            created_at=1_700_000_001,
            payload=encode_time_sync(version=1, epoch=55),
        )
        api = self._api(FakeCoapClient([failure]))

        status, _headers, body = api.handle_request(
            method="POST",
            path="/api/v1/downlinks",
            content_type="application/octet-stream",
            body=request.to_bytes(),
            now=1_700_000_002,
        )

        result = DownlinkResult.from_bytes(body)
        self.assertEqual(200, status)
        self.assertEqual(DOWNLINK_STATUS_MESH_DELIVERY_FAILED, result.status)

    def test_internal_gateway_failure_is_non_terminal_and_retryable(self) -> None:
        self._register_node(10, "fd12:3456::10")
        request = DownlinkRequest(
            version=1,
            gateway_id=7,
            downlink_id=104,
            target_node_id=10,
            created_at=1_700_000_001,
            payload=encode_time_sync(version=1, epoch=55),
        )
        coap = FakeCoapClient([RuntimeError("socket setup failed"), None])
        api = self._api(coap)

        first_status, _headers, first_body = api.handle_request(
            method="POST",
            path="/api/v1/downlinks",
            content_type="application/octet-stream",
            body=request.to_bytes(),
            now=1_700_000_002,
        )
        second_status, _headers, second_body = api.handle_request(
            method="POST",
            path="/api/v1/downlinks",
            content_type="application/octet-stream",
            body=request.to_bytes(),
            now=1_700_000_003,
        )

        result = DownlinkResult.from_bytes(second_body)
        self.assertEqual(503, first_status)
        self.assertEqual(b"", first_body)
        self.assertEqual(200, second_status)
        self.assertEqual(DOWNLINK_STATUS_DELIVERED, result.status)
        self.assertEqual(2, len(coap.calls))

    def test_duplicate_terminal_retry_replays_same_0x85_without_resending(self) -> None:
        self._register_node(10, "fd12:3456::10")
        request = DownlinkRequest(
            version=1,
            gateway_id=7,
            downlink_id=105,
            target_node_id=10,
            created_at=1_700_000_001,
            payload=encode_time_sync(version=1, epoch=55),
        )
        coap = FakeCoapClient()
        api = self._api(coap)

        first_status, _headers, first_body = api.handle_request(
            method="POST",
            path="/api/v1/downlinks",
            content_type="application/octet-stream",
            body=request.to_bytes(),
            now=1_700_000_002,
        )
        second_status, _headers, second_body = api.handle_request(
            method="POST",
            path="/api/v1/downlinks",
            content_type="application/octet-stream",
            body=request.to_bytes(),
            now=1_700_000_099,
        )

        self.assertEqual(200, first_status)
        self.assertEqual(200, second_status)
        self.assertEqual(first_body, second_body)
        self.assertEqual(1, len(coap.calls))

    def test_wrong_content_type_returns_415(self) -> None:
        request = DownlinkRequest(
            version=1,
            gateway_id=7,
            downlink_id=106,
            target_node_id=10,
            created_at=1_700_000_001,
            payload=encode_time_sync(version=1, epoch=55),
        )
        api = self._api(FakeCoapClient())

        status, _headers, body = api.handle_request(
            method="POST",
            path="/api/v1/downlinks",
            content_type="application/json",
            body=request.to_bytes(),
            now=1_700_000_002,
        )

        self.assertEqual(415, status)
        self.assertEqual(b"", body)

    def test_malformed_request_header_returns_400_without_terminal_result(self) -> None:
        api = self._api(FakeCoapClient())

        status, _headers, body = api.handle_request(
            method="POST",
            path="/api/v1/downlinks",
            content_type="application/octet-stream",
            body=b"\x84\x01\x07",
            now=1_700_000_002,
        )

        self.assertEqual(400, status)
        self.assertEqual(b"", body)
        self.assertEqual([], self.service.downlink_audit_store.list_terminal_results())


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
