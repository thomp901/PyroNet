from __future__ import annotations

import ipaddress
import os
import struct
import time
import unittest

from pyronet_gateway.backhaul_client import HTTPBackhaulClient
from pyronet_gateway.packet_codec import GatewayRegistration, NodeUplinkEnvelope, UplinkReceipt
from pyronet_gateway.protocol.backhaul import RECEIPT_DURABLE_INGEST, RECEIPT_PERMANENT_REJECT

REGISTRATION_STRUCT = struct.Struct("<BBHffHB16s")
SENSOR_STRUCT = struct.Struct("<BBHIBhHHHB")
PARENT_UPDATE_STRUCT = struct.Struct("<BBHI16s")


def _live_tests_enabled() -> bool:
    return os.environ.get("PYRONET_RUN_LIVE_BACKHAUL_TESTS") == "1"


def _require_env(name: str) -> str:
    value = os.environ.get(name)
    if not value:
        raise unittest.SkipTest(f"{name} must be set for live backhaul tests")
    return value


@unittest.skipUnless(_live_tests_enabled(), "set PYRONET_RUN_LIVE_BACKHAUL_TESTS=1 to send to a real backhaul")
class LiveBackhaulEndpointTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        base_url = _require_env("PYRONET_LIVE_BACKHAUL_BASE_URL")
        timeout_seconds = float(os.environ.get("PYRONET_LIVE_BACKHAUL_TIMEOUT_SECONDS", "5.0"))
        cls.client = HTTPBackhaulClient(base_url=base_url, timeout_seconds=timeout_seconds)
        cls.gateway_id = int(os.environ.get("PYRONET_LIVE_BACKHAUL_GATEWAY_ID", "7"))
        cls.version = int(os.environ.get("PYRONET_LIVE_BACKHAUL_VERSION", "1"))
        cls.latitude = float(os.environ.get("PYRONET_LIVE_BACKHAUL_LATITUDE", "39.7684"))
        cls.longitude = float(os.environ.get("PYRONET_LIVE_BACKHAUL_LONGITUDE", "-86.1581"))
        cls.sw_version = int(os.environ.get("PYRONET_LIVE_BACKHAUL_SW_VERSION", "258"))
        cls.observed_src_ipv6 = os.environ.get("PYRONET_LIVE_BACKHAUL_OBSERVED_SRC_IPV6", "fd12:3456::abcd")
        cls.node_id = int(os.environ.get("PYRONET_LIVE_BACKHAUL_NODE_ID", "1001"))
        cls.parent_ipv6 = os.environ.get("PYRONET_LIVE_BACKHAUL_PARENT_IPV6", "fd12:3456::1")
        cls.followup_packet_type = int(os.environ.get("PYRONET_LIVE_BACKHAUL_FOLLOWUP_PACKET_TYPE", "2"), 0)
        cls.receipt_statuses = {RECEIPT_DURABLE_INGEST, RECEIPT_PERMANENT_REJECT}

    def test_gateway_registration_reaches_real_endpoint(self) -> None:
        registration = GatewayRegistration(
            version=self.version,
            gateway_id=self.gateway_id,
            timestamp=int(time.time()),
            latitude=self.latitude,
            longitude=self.longitude,
            sw_version=self.sw_version,
        )

        self.client.send_gateway_registration(registration)

    def test_node_registration_then_followup_uplink_reaches_real_endpoint(self) -> None:
        now = int(time.time())
        self.client.send_gateway_registration(
            GatewayRegistration(
                version=self.version,
                gateway_id=self.gateway_id,
                timestamp=now,
                latitude=self.latitude,
                longitude=self.longitude,
                sw_version=self.sw_version,
            )
        )

        registration_envelope = NodeUplinkEnvelope(
            version=self.version,
            gateway_id=self.gateway_id,
            uplink_id=now,
            received_at=now,
            observed_src_ipv6=self.observed_src_ipv6,
            payload=_build_registration_packet(node_id=self.node_id, parent_ipv6=self.parent_ipv6),
        )
        registration_receipt = self.client.send_uplink(registration_envelope)
        self._assert_valid_receipt(registration_envelope, registration_receipt)

        followup_envelope = NodeUplinkEnvelope(
            version=self.version,
            gateway_id=self.gateway_id,
            uplink_id=now + 1,
            received_at=now + 1,
            observed_src_ipv6=self.observed_src_ipv6,
            payload=_build_followup_packet(
                packet_type=self.followup_packet_type,
                node_id=self.node_id,
                timestamp=now + 1,
                parent_ipv6=self.parent_ipv6,
            ),
        )
        followup_receipt = self.client.send_uplink(followup_envelope)
        self._assert_valid_receipt(followup_envelope, followup_receipt)

    def _assert_valid_receipt(self, envelope: NodeUplinkEnvelope, receipt: UplinkReceipt) -> None:
        self.assertIsInstance(receipt, UplinkReceipt)
        self.assertEqual(envelope.version, receipt.version)
        self.assertEqual(envelope.gateway_id, receipt.gateway_id)
        self.assertEqual(envelope.uplink_id, receipt.uplink_id)
        self.assertIn(receipt.status, self.receipt_statuses)


def _build_registration_packet(*, node_id: int, parent_ipv6: str) -> bytes:
    payload = REGISTRATION_STRUCT.pack(
        0x01,
        0x01,
        node_id,
        39.0,
        -86.0,
        0x0102,
        88,
        ipaddress.IPv6Address(parent_ipv6).packed,
    )
    assert len(payload) == 31
    return payload


def _build_followup_packet(*, packet_type: int, node_id: int, timestamp: int, parent_ipv6: str) -> bytes:
    if packet_type in {0x02, 0x03}:
        payload = SENSOR_STRUCT.pack(
            packet_type,
            0x01,
            node_id,
            timestamp,
            3,
            2450,
            5000,
            123,
            55,
            90,
        )
        assert len(payload) == 18
        return payload
    if packet_type == 0x08:
        payload = PARENT_UPDATE_STRUCT.pack(
            0x08,
            0x01,
            node_id,
            timestamp,
            ipaddress.IPv6Address(parent_ipv6).packed,
        )
        assert len(payload) == 24
        return payload
    raise ValueError("PYRONET_LIVE_BACKHAUL_FOLLOWUP_PACKET_TYPE must be 0x02, 0x03, or 0x08")


if __name__ == "__main__":
    unittest.main()
