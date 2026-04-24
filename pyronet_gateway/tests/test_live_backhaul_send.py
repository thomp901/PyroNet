from __future__ import annotations

import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from pyronet_gateway.live_backhaul_send import (
    build_followup_packet,
    build_node_registration_envelope,
    main,
)
from pyronet_gateway.protocol.backhaul import UplinkReceipt
from pyronet_gateway.protocol.node_packets import parse_node_packet
from pyronet_gateway.protocol.node_packets import (
    TYPE_PARENT_UPDATE,
    TYPE_REGISTRATION,
    TYPE_SENSOR_REPORT,
    TYPE_SENSOR_ALERT,
)


class FakeBackhaulClient:
    instances: list["FakeBackhaulClient"] = []

    def __init__(self, *, base_url: str, timeout_seconds: float) -> None:
        self.base_url = base_url
        self.timeout_seconds = timeout_seconds
        self.gateway_registrations = []
        self.uplinks = []
        FakeBackhaulClient.instances.append(self)

    def send_gateway_registration(self, registration) -> None:
        self.gateway_registrations.append(registration)

    def send_uplink(self, envelope):
        self.uplinks.append(envelope)
        return UplinkReceipt(
            version=envelope.version,
            gateway_id=envelope.gateway_id,
            uplink_id=envelope.uplink_id,
            status=0x00,
        )


class LiveBackhaulSendTests(unittest.TestCase):
    def setUp(self) -> None:
        FakeBackhaulClient.instances.clear()
        self.tempdir = tempfile.TemporaryDirectory()
        self.config_path = Path(self.tempdir.name) / "gateway.toml"
        self.config_path.write_text(
            "\n".join(
                [
                    "[gateway]",
                    "gateway_id = 7",
                    "latitude = 40.42873081269003",
                    'longitude = -86.91197196095088',
                    'sw_version_override = "1.2"',
                    "",
                    "[coap]",
                    'resource_path = "/uplink"',
                    "",
                    "[backhaul]",
                    'base_url = "http://csp.example:4000"',
                    "http_timeout_seconds = 5.0",
                    "",
                    "[runtime]",
                    'db_path = "./gateway.sqlite3"',
                ]
            )
        )

    def tearDown(self) -> None:
        self.tempdir.cleanup()

    def test_main_sends_registration_then_followups(self) -> None:
        stdout = io.StringIO()
        with contextlib.redirect_stdout(stdout), patch(
            "pyronet_gateway.live_backhaul_send.HTTPBackhaulClient",
            FakeBackhaulClient,
        ):
            exit_code = main(
                [
                    "--config",
                    str(self.config_path),
                    "--node-id",
                    "4242",
                    "--observed-src-ipv6",
                    "fd12:3456::4242",
                    "--parent-ipv6",
                    "fd12:3456::1",
                    "--received-at",
                    "1700000100",
                    "--followup-packet-type",
                    "0x03",
                ]
            )

        self.assertEqual(0, exit_code)
        self.assertEqual(1, len(FakeBackhaulClient.instances))
        client = FakeBackhaulClient.instances[0]
        self.assertEqual("http://csp.example:4000", client.base_url)
        self.assertEqual(1, len(client.gateway_registrations))
        self.assertEqual(2, len(client.uplinks))
        self.assertEqual(TYPE_REGISTRATION, client.uplinks[0].payload[0])
        self.assertEqual(TYPE_SENSOR_ALERT, client.uplinks[1].payload[0])
        self.assertEqual(1700000100, client.uplinks[0].uplink_id)
        self.assertEqual(1700000101, client.uplinks[1].uplink_id)

        lines = [json.loads(line) for line in stdout.getvalue().splitlines()]
        self.assertEqual(
            ["gateway_registration", "node_registration", "node_followup"],
            [line["kind"] for line in lines],
        )

    def test_main_can_skip_gateway_and_node_registration(self) -> None:
        with patch("pyronet_gateway.live_backhaul_send.HTTPBackhaulClient", FakeBackhaulClient):
            exit_code = main(
                [
                    "--config",
                    str(self.config_path),
                    "--no-send-registration",
                    "--no-send-node-registration",
                    "--received-at",
                    "1700000200",
                ]
            )

        self.assertEqual(0, exit_code)
        client = FakeBackhaulClient.instances[0]
        self.assertEqual([], client.gateway_registrations)
        self.assertEqual(1, len(client.uplinks))
        self.assertEqual(TYPE_SENSOR_REPORT, client.uplinks[0].payload[0])

    def test_build_followup_packet_supports_parent_update(self) -> None:
        packet = build_followup_packet(
            version=1,
            packet_type=TYPE_PARENT_UPDATE,
            node_id=9001,
            timestamp=1700000000,
            parent_ipv6="fd12:3456::99",
        )
        self.assertEqual(TYPE_PARENT_UPDATE, packet[0])
        self.assertEqual(24, len(packet))

    def test_build_node_registration_envelope_uses_gateway_versions(self) -> None:
        from pyronet_gateway.config import load_config

        config = load_config(self.config_path)
        envelope = build_node_registration_envelope(
            config=config,
            node_id=777,
            observed_src_ipv6="fd12:3456::777",
            parent_ipv6="fd12:3456::1",
            received_at=1700000300,
            uplink_id=1700000300,
        )
        packet = parse_node_packet(envelope.payload)
        self.assertEqual(1, envelope.version)
        self.assertEqual(TYPE_REGISTRATION, envelope.payload[0])
        self.assertAlmostEqual(config.latitude + 0.0012, packet.latitude, places=4)
        self.assertAlmostEqual(config.longitude + 0.0012, packet.longitude, places=4)

    def test_invalid_followup_type_raises(self) -> None:
        with self.assertRaisesRegex(ValueError, "follow-up packet type must be 0x02, 0x03, or 0x08"):
            build_followup_packet(
                version=1,
                packet_type=0x07,
                node_id=9002,
                timestamp=1700000000,
                parent_ipv6="fd12:3456::99",
            )


if __name__ == "__main__":
    unittest.main()
