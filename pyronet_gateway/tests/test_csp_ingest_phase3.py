from __future__ import annotations

import copy
import ipaddress
import struct
import unittest
from contextlib import contextmanager

from pyronet_gateway.alert_store import AlertRecord
from pyronet_gateway.backhaul_binary_codec import parse_gateway_registration
from pyronet_gateway.backhaul_http_api import BackhaulHttpApi
from pyronet_gateway.db.migrations import LATEST_SCHEMA_VERSION, MIGRATIONS
from pyronet_gateway.db.postgres import PostgresDatabase
from pyronet_gateway.gateway_store import GatewayRecord, GatewayRegistrationRecord, PostgresGatewayStore
from pyronet_gateway.idempotency_store import (
    MalformedEnvelopeRejectRecord,
    PostgresIdempotencyStore,
    StoredEnvelopeRecord,
    StoredReceiptRecord,
)
from pyronet_gateway.ingest_service import IngestService
from pyronet_gateway.node_store import NodeRecord
from pyronet_gateway.protocol.backhaul import (
    RECEIPT_DURABLE_INGEST,
    RECEIPT_PERMANENT_REJECT,
    GatewayRegistration,
    NodeUplinkEnvelope,
    UplinkReceipt,
)
from pyronet_gateway.protocol.node_packets import TYPE_PARENT_UPDATE, TYPE_REGISTRATION, TYPE_SENSOR_ALERT
from pyronet_gateway.sensor_history_store import SensorReadingRecord
from pyronet_gateway.topology_store import TopologyEventRecord


REGISTRATION_STRUCT = struct.Struct("<BBHffHB16s")
REPORT_STRUCT = struct.Struct("<BBHIBhHHHB")
PARENT_UPDATE_STRUCT = struct.Struct("<BBHI16s")
ENVELOPE_HEADER = struct.Struct("<BBHQI16sH")


class InMemoryCspPersistence:
    def __init__(self) -> None:
        self.gateways = {}
        self.gateway_registrations = []
        self.receipts = {}
        self.envelopes = {}
        self.malformed_rejects = {}
        self.nodes = {}
        self.sensor_readings = []
        self.topology_events = []
        self.alerts = []
        self.fail_operation = None
        self._sensor_id = 1
        self._topology_id = 1
        self._alert_id = 1

    @contextmanager
    def transaction(self):
        snapshot = copy.deepcopy(
            (
                self.gateways,
                self.gateway_registrations,
                self.receipts,
                self.envelopes,
                self.malformed_rejects,
                self.nodes,
                self.sensor_readings,
                self.topology_events,
                self.alerts,
                self._sensor_id,
                self._topology_id,
                self._alert_id,
            )
        )
        try:
            yield self
        except Exception:
            (
                self.gateways,
                self.gateway_registrations,
                self.receipts,
                self.envelopes,
                self.malformed_rejects,
                self.nodes,
                self.sensor_readings,
                self.topology_events,
                self.alerts,
                self._sensor_id,
                self._topology_id,
                self._alert_id,
            ) = snapshot
            raise

    def _maybe_fail(self, operation: str) -> None:
        if self.fail_operation == operation:
            self.fail_operation = None
            raise RuntimeError("database unavailable")

    def get_receipt(self, gateway_id: int, uplink_id: int, *, connection=None):
        self._maybe_fail("get_receipt")
        return self.receipts.get((gateway_id, uplink_id))

    def store_envelope(
        self,
        envelope: NodeUplinkEnvelope,
        *,
        raw_envelope: bytes,
        payload_type: int,
        node_id: int,
        created_at: int,
        connection=None,
    ) -> None:
        self._maybe_fail("store_envelope")
        key = (envelope.gateway_id, envelope.uplink_id)
        self.envelopes.setdefault(
            key,
            StoredEnvelopeRecord(
                gateway_id=envelope.gateway_id,
                uplink_id=envelope.uplink_id,
                version=envelope.version,
                received_at=envelope.received_at,
                observed_src_ipv6=envelope.observed_src_ipv6,
                payload_type=payload_type,
                node_id=node_id,
                raw_envelope=raw_envelope,
                payload_bytes=envelope.payload,
                created_at=created_at,
            ),
        )

    def store_malformed_envelope_reject(
        self,
        *,
        gateway_id: int,
        uplink_id: int,
        version: int | None,
        raw_request_body: bytes,
        reject_reason: str,
        created_at: int,
        connection=None,
    ) -> None:
        self._maybe_fail("store_malformed_envelope_reject")
        key = (gateway_id, uplink_id)
        self.malformed_rejects.setdefault(
            key,
            MalformedEnvelopeRejectRecord(
                gateway_id=gateway_id,
                uplink_id=uplink_id,
                version=version,
                raw_request_body=raw_request_body,
                reject_reason=reject_reason,
                created_at=created_at,
            ),
        )

    def store_receipt(self, receipt, *, receipt_bytes: bytes, detail: str | None, created_at: int, connection=None):
        self._maybe_fail("store_receipt")
        key = (receipt.gateway_id, receipt.uplink_id)
        self.receipts.setdefault(
            key,
            StoredReceiptRecord(
                gateway_id=receipt.gateway_id,
                uplink_id=receipt.uplink_id,
                status=receipt.status,
                receipt_bytes=receipt_bytes,
                detail=detail,
                created_at=created_at,
            ),
        )

    def ensure_placeholder(self, gateway_id: int, *, seen_at: int, connection=None) -> None:
        self._maybe_fail("ensure_placeholder")
        existing = self.gateways.get(gateway_id)
        if existing is None:
            self.gateways[gateway_id] = GatewayRecord(
                gateway_id=gateway_id,
                latitude=None,
                longitude=None,
                sw_version=None,
                last_registered_at=None,
                created_at=seen_at,
                updated_at=seen_at,
            )
            return
        self.gateways[gateway_id] = GatewayRecord(
            gateway_id=existing.gateway_id,
            latitude=existing.latitude,
            longitude=existing.longitude,
            sw_version=existing.sw_version,
            last_registered_at=existing.last_registered_at,
            created_at=existing.created_at,
            updated_at=seen_at,
        )

    def upsert_registration_node(self, packet, *, envelope, connection=None) -> None:
        existing = self.nodes.get(packet.node_id)
        created_at = existing.created_at if existing is not None else envelope.received_at
        self.nodes[packet.node_id] = NodeRecord(
            node_id=packet.node_id,
            current_ipv6=envelope.observed_src_ipv6,
            latitude=packet.latitude,
            longitude=packet.longitude,
            firmware_version=packet.fw_version,
            latest_battery=packet.battery_pct,
            latest_risk_level=existing.latest_risk_level if existing is not None else None,
            latest_temperature=existing.latest_temperature if existing is not None else None,
            latest_humidity=existing.latest_humidity if existing is not None else None,
            latest_voc=existing.latest_voc if existing is not None else None,
            latest_pm25=existing.latest_pm25 if existing is not None else None,
            last_seen=envelope.received_at,
            last_gateway_id=envelope.gateway_id,
            latest_parent_ipv6=packet.parent_ipv6,
            created_at=created_at,
            updated_at=envelope.received_at,
        )

    def upsert_sensor_state(self, packet, *, envelope, connection=None) -> None:
        existing = self.nodes.get(packet.node_id)
        created_at = existing.created_at if existing is not None else envelope.received_at
        self.nodes[packet.node_id] = NodeRecord(
            node_id=packet.node_id,
            current_ipv6=envelope.observed_src_ipv6,
            latitude=existing.latitude if existing is not None else None,
            longitude=existing.longitude if existing is not None else None,
            firmware_version=existing.firmware_version if existing is not None else None,
            latest_battery=packet.battery_pct,
            latest_risk_level=packet.risk_level,
            latest_temperature=packet.temperature,
            latest_humidity=packet.humidity,
            latest_voc=packet.voc_iaq,
            latest_pm25=packet.pm25,
            last_seen=envelope.received_at,
            last_gateway_id=envelope.gateway_id,
            latest_parent_ipv6=existing.latest_parent_ipv6 if existing is not None else None,
            created_at=created_at,
            updated_at=envelope.received_at,
        )

    def upsert_parent_update(self, packet, *, envelope, connection=None) -> None:
        existing = self.nodes.get(packet.node_id)
        created_at = existing.created_at if existing is not None else envelope.received_at
        self.nodes[packet.node_id] = NodeRecord(
            node_id=packet.node_id,
            current_ipv6=envelope.observed_src_ipv6,
            latitude=existing.latitude if existing is not None else None,
            longitude=existing.longitude if existing is not None else None,
            firmware_version=existing.firmware_version if existing is not None else None,
            latest_battery=existing.latest_battery if existing is not None else None,
            latest_risk_level=existing.latest_risk_level if existing is not None else None,
            latest_temperature=existing.latest_temperature if existing is not None else None,
            latest_humidity=existing.latest_humidity if existing is not None else None,
            latest_voc=existing.latest_voc if existing is not None else None,
            latest_pm25=existing.latest_pm25 if existing is not None else None,
            last_seen=envelope.received_at,
            last_gateway_id=envelope.gateway_id,
            latest_parent_ipv6=packet.parent_ipv6,
            created_at=created_at,
            updated_at=envelope.received_at,
        )

    def get(self, node_id: int, *, connection=None):
        return self.nodes.get(node_id)

    def append_reading(self, packet, *, envelope, connection=None) -> None:
        self.sensor_readings.append(
            SensorReadingRecord(
                id=self._sensor_id,
                gateway_id=envelope.gateway_id,
                uplink_id=envelope.uplink_id,
                packet_type=packet.packet_type,
                node_id=packet.node_id,
                node_event_time=packet.timestamp,
                gateway_received_at=envelope.received_at,
                observed_src_ipv6=envelope.observed_src_ipv6,
                temperature=packet.temperature,
                humidity=packet.humidity,
                voc=packet.voc_iaq,
                pm25=packet.pm25,
                risk_level=packet.risk_level,
                battery_pct=packet.battery_pct,
                raw_payload=envelope.payload,
                created_at=envelope.received_at,
            )
        )
        self._sensor_id += 1

    def append_event(
        self,
        *,
        packet_type: int,
        node_id: int,
        parent_ipv6: str | None,
        node_event_time: int | None,
        envelope,
        raw_payload: bytes,
        connection=None,
    ) -> None:
        self.topology_events.append(
            TopologyEventRecord(
                id=self._topology_id,
                gateway_id=envelope.gateway_id,
                uplink_id=envelope.uplink_id,
                packet_type=packet_type,
                node_id=node_id,
                parent_ipv6=parent_ipv6,
                node_event_time=node_event_time,
                gateway_received_at=envelope.received_at,
                raw_payload=raw_payload,
                created_at=envelope.received_at,
            )
        )
        self._topology_id += 1

    def append_alert(self, packet, *, envelope, connection=None) -> None:
        self.alerts.append(
            AlertRecord(
                id=self._alert_id,
                gateway_id=envelope.gateway_id,
                uplink_id=envelope.uplink_id,
                node_id=packet.node_id,
                node_event_time=packet.timestamp,
                gateway_received_at=envelope.received_at,
                risk_level=packet.risk_level,
                battery_pct=packet.battery_pct,
                raw_payload=envelope.payload,
                created_at=envelope.received_at,
            )
        )
        self._alert_id += 1

    def upsert_registration(self, packet, *, envelope=None, raw_payload=None, now=None, connection=None):  # type: ignore[override]
        if envelope is None:
            return self._upsert_gateway_registration(packet, raw_payload=raw_payload, now=now)
        return self.upsert_registration_node(packet, envelope=envelope, connection=connection)

    def _upsert_gateway_registration(self, registration, *, raw_payload: bytes, now: int):
        self._maybe_fail("upsert_registration")
        existing = self.gateways.get(registration.gateway_id)
        created_at = existing.created_at if existing is not None else now
        self.gateways[registration.gateway_id] = GatewayRecord(
            gateway_id=registration.gateway_id,
            latitude=registration.latitude,
            longitude=registration.longitude,
            sw_version=registration.sw_version,
            last_registered_at=registration.timestamp,
            created_at=created_at,
            updated_at=now,
        )
        self.gateway_registrations.append(
            GatewayRegistrationRecord(
                gateway_id=registration.gateway_id,
                timestamp=registration.timestamp,
                raw_payload=raw_payload,
                created_at=now,
            )
        )


class CspIngestTests(unittest.TestCase):
    def setUp(self) -> None:
        self.persistence = InMemoryCspPersistence()
        self.service = IngestService(
            transaction_manager=self.persistence,
            idempotency_store=self.persistence,
            gateway_store=self.persistence,
            node_store=self.persistence,
            sensor_history_store=self.persistence,
            topology_store=self.persistence,
            alert_store=self.persistence,
        )
        self.api = BackhaulHttpApi(ingest_service=self.service, max_request_body_bytes=65536)

    def test_gateway_registration_upsert(self) -> None:
        body = GatewayRegistration(
            version=1,
            gateway_id=7,
            timestamp=1_700_000_000,
            latitude=39.76,
            longitude=-86.15,
            sw_version=0x0102,
        ).to_bytes()
        status, headers, response = self.api.handle_request(
            method="POST",
            path="/api/v1/gateways/register",
            headers={"content-type": "application/octet-stream"},
            body=body,
            now=1_700_000_001,
        )
        self.assertEqual(204, status)
        self.assertEqual(b"", response)
        stored = self.persistence.gateways[7]
        self.assertAlmostEqual(39.76, stored.latitude, places=4)
        self.assertEqual(0x0102, stored.sw_version)
        self.assertEqual(1, len(self.persistence.gateway_registrations))
        self.assertEqual(7, parse_gateway_registration(body).gateway_id)
        self.assertEqual("0", headers["Content-Length"])

    def test_uplink_success_generates_terminal_receipt(self) -> None:
        envelope_bytes = build_envelope(
            gateway_id=9,
            uplink_id=42,
            observed_src_ipv6="fd12:3456::99",
            payload=build_report_packet(node_id=1001, packet_type=0x02),
        )
        status, headers, response = self.api.handle_request(
            method="POST",
            path="/api/v1/uplinks",
            headers={"content-type": "application/octet-stream"},
            body=envelope_bytes,
            now=1_700_000_010,
        )
        receipt = UplinkReceipt.from_bytes(response)
        self.assertEqual(200, status)
        self.assertEqual("application/octet-stream", headers["Content-Type"])
        self.assertEqual(RECEIPT_DURABLE_INGEST, receipt.status)
        self.assertEqual(1, len(self.persistence.sensor_readings))
        self.assertEqual(envelope_bytes, self.persistence.envelopes[(9, 42)].raw_envelope)

    def test_duplicate_uplink_returns_same_receipt_without_duplicate_rows(self) -> None:
        envelope_bytes = build_envelope(
            gateway_id=9,
            uplink_id=77,
            observed_src_ipv6="fd12:3456::77",
            payload=build_report_packet(node_id=1002, packet_type=0x02),
        )
        first = self.api.handle_request(
            method="POST",
            path="/api/v1/uplinks",
            headers={"content-type": "application/octet-stream"},
            body=envelope_bytes,
            now=1_700_000_020,
        )
        second = self.api.handle_request(
            method="POST",
            path="/api/v1/uplinks",
            headers={"content-type": "application/octet-stream"},
            body=envelope_bytes,
            now=1_700_000_021,
        )
        self.assertEqual(first[2], second[2])
        self.assertEqual(1, len(self.persistence.sensor_readings))
        self.assertEqual(1, len(self.persistence.envelopes))
        self.assertEqual(1, len(self.persistence.receipts))

    def test_malformed_uplink_envelope_is_permanent_reject(self) -> None:
        good = build_envelope(
            gateway_id=9,
            uplink_id=88,
            observed_src_ipv6="fd12:3456::88",
            payload=build_report_packet(node_id=1003, packet_type=0x02),
        )
        malformed = good[:-1]
        status, headers, response = self.api.handle_request(
            method="POST",
            path="/api/v1/uplinks",
            headers={"content-type": "application/octet-stream"},
            body=malformed,
            now=1_700_000_030,
        )
        receipt = UplinkReceipt.from_bytes(response)
        self.assertEqual(200, status)
        self.assertEqual("application/octet-stream", headers["Content-Type"])
        self.assertEqual(RECEIPT_PERMANENT_REJECT, receipt.status)
        self.assertEqual("invalid uplink envelope: payload length does not match envelope header", self.persistence.receipts[(9, 88)].detail)
        reject_record = self.persistence.malformed_rejects[(9, 88)]
        self.assertEqual(malformed, reject_record.raw_request_body)
        self.assertEqual(1, reject_record.version)

    def test_duplicate_malformed_uplink_returns_same_receipt_without_duplicate_reject_record(self) -> None:
        good = build_envelope(
            gateway_id=9,
            uplink_id=188,
            observed_src_ipv6="fd12:3456::188",
            payload=build_report_packet(node_id=1008, packet_type=0x02),
        )
        malformed = good[:-3]
        first = self.api.handle_request(
            method="POST",
            path="/api/v1/uplinks",
            headers={"content-type": "application/octet-stream"},
            body=malformed,
            now=1_700_000_030,
        )
        second = self.api.handle_request(
            method="POST",
            path="/api/v1/uplinks",
            headers={"content-type": "application/octet-stream"},
            body=malformed,
            now=1_700_000_031,
        )
        self.assertEqual(first[2], second[2])
        self.assertEqual(1, len(self.persistence.malformed_rejects))
        self.assertEqual(1, len(self.persistence.receipts))

    def test_malformed_embedded_node_packet_is_permanent_reject(self) -> None:
        envelope_bytes = build_envelope(
            gateway_id=9,
            uplink_id=89,
            observed_src_ipv6="fd12:3456::89",
            payload=b"\x02\x01",
        )
        status, _headers, response = self.api.handle_request(
            method="POST",
            path="/api/v1/uplinks",
            headers={"content-type": "application/octet-stream"},
            body=envelope_bytes,
            now=1_700_000_031,
        )
        receipt = UplinkReceipt.from_bytes(response)
        self.assertEqual(200, status)
        self.assertEqual(RECEIPT_PERMANENT_REJECT, receipt.status)
        self.assertEqual(0, len(self.persistence.sensor_readings))
        self.assertEqual(envelope_bytes, self.persistence.envelopes[(9, 89)].raw_envelope)
        self.assertIsNone(self.persistence.envelopes[(9, 89)].node_id)

    def test_valid_packet_still_stores_real_node_id(self) -> None:
        envelope_bytes = build_envelope(
            gateway_id=21,
            uplink_id=90,
            observed_src_ipv6="fd12:3456::21",
            payload=build_report_packet(node_id=4321, packet_type=0x02),
        )
        self.api.handle_request(
            method="POST",
            path="/api/v1/uplinks",
            headers={"content-type": "application/octet-stream"},
            body=envelope_bytes,
            now=1_700_000_033,
        )
        self.assertEqual(4321, self.persistence.envelopes[(21, 90)].node_id)

    def test_transient_database_failure_returns_non_terminal_failure(self) -> None:
        self.persistence.fail_operation = "store_envelope"
        envelope_bytes = build_envelope(
            gateway_id=9,
            uplink_id=90,
            observed_src_ipv6="fd12:3456::90",
            payload=build_report_packet(node_id=1004, packet_type=0x02),
        )
        status, headers, response = self.api.handle_request(
            method="POST",
            path="/api/v1/uplinks",
            headers={"content-type": "application/octet-stream"},
            body=envelope_bytes,
            now=1_700_000_032,
        )
        self.assertEqual(503, status)
        self.assertEqual("text/plain; charset=utf-8", headers["Content-Type"])
        self.assertIn(b"transient uplink failure", response)
        self.assertEqual({}, self.persistence.receipts)
        self.assertEqual({}, self.persistence.envelopes)

    def test_registration_packet_updates_node_identity_location_ipv6_and_parent(self) -> None:
        envelope_bytes = build_envelope(
            gateway_id=11,
            uplink_id=1,
            observed_src_ipv6="fd12:3456::11",
            payload=build_registration_packet(
                node_id=2001,
                latitude=40.1,
                longitude=-85.2,
                fw_version=0x0203,
                battery_pct=88,
                parent_ipv6="fd12:3456::1",
            ),
        )
        self.api.handle_request(
            method="POST",
            path="/api/v1/uplinks",
            headers={"content-type": "application/octet-stream"},
            body=envelope_bytes,
            now=1_700_000_040,
        )
        node = self.persistence.get(2001)
        self.assertEqual("fd12:3456::11", node.current_ipv6)
        self.assertAlmostEqual(40.1, node.latitude, places=4)
        self.assertAlmostEqual(-85.2, node.longitude, places=4)
        self.assertEqual(0x0203, node.firmware_version)
        self.assertEqual("fd12:3456::1", node.latest_parent_ipv6)
        self.assertEqual(1, len(self.persistence.topology_events))
        self.assertEqual(TYPE_REGISTRATION, self.persistence.topology_events[0].packet_type)

    def test_sensor_report_stores_history_and_updates_latest_node_state(self) -> None:
        self._ingest_registration(node_id=2001)
        envelope_bytes = build_envelope(
            gateway_id=11,
            uplink_id=2,
            observed_src_ipv6="fd12:3456::12",
            payload=build_report_packet(node_id=2001, packet_type=0x02, temperature=251, humidity=603, voc=444, pm25=12, risk_level=3, battery_pct=76),
        )
        self.api.handle_request(
            method="POST",
            path="/api/v1/uplinks",
            headers={"content-type": "application/octet-stream"},
            body=envelope_bytes,
            now=1_700_000_041,
        )
        node = self.persistence.get(2001)
        reading = self.persistence.sensor_readings[-1]
        self.assertEqual("fd12:3456::12", node.current_ipv6)
        self.assertEqual(251, node.latest_temperature)
        self.assertEqual(603, node.latest_humidity)
        self.assertEqual(444, node.latest_voc)
        self.assertEqual(12, node.latest_pm25)
        self.assertEqual(3, node.latest_risk_level)
        self.assertEqual(76, node.latest_battery)
        self.assertEqual(1_700_000_000, reading.node_event_time)
        self.assertEqual(1_700_000_041, reading.gateway_received_at)

    def test_sensor_alert_stores_alert_record(self) -> None:
        envelope_bytes = build_envelope(
            gateway_id=12,
            uplink_id=3,
            observed_src_ipv6="fd12:3456::13",
            payload=build_report_packet(node_id=3001, packet_type=0x03, risk_level=5),
        )
        self.api.handle_request(
            method="POST",
            path="/api/v1/uplinks",
            headers={"content-type": "application/octet-stream"},
            body=envelope_bytes,
            now=1_700_000_050,
        )
        self.assertEqual(1, len(self.persistence.alerts))
        self.assertEqual(TYPE_SENSOR_ALERT, self.persistence.sensor_readings[0].packet_type)
        self.assertEqual(5, self.persistence.alerts[0].risk_level)

    def test_parent_update_stores_topology_event_and_updates_latest_parent(self) -> None:
        registration = build_envelope(
            gateway_id=13,
            uplink_id=4,
            observed_src_ipv6="fd12:3456::14",
            payload=build_registration_packet(node_id=4001, parent_ipv6="fd12:3456::20"),
        )
        self.api.handle_request(
            method="POST",
            path="/api/v1/uplinks",
            headers={"content-type": "application/octet-stream"},
            body=registration,
            now=1_700_000_060,
        )
        update = build_envelope(
            gateway_id=13,
            uplink_id=5,
            observed_src_ipv6="fd12:3456::15",
            payload=build_parent_update_packet(node_id=4001, parent_ipv6="fd12:3456::21"),
        )
        self.api.handle_request(
            method="POST",
            path="/api/v1/uplinks",
            headers={"content-type": "application/octet-stream"},
            body=update,
            now=1_700_000_061,
        )
        node = self.persistence.get(4001)
        self.assertEqual("fd12:3456::21", node.latest_parent_ipv6)
        self.assertEqual("fd12:3456::15", node.current_ipv6)
        self.assertEqual(2, len(self.persistence.topology_events))
        self.assertEqual(TYPE_PARENT_UPDATE, self.persistence.topology_events[-1].packet_type)

    def test_unknown_gateway_is_placeholder_upserted_on_uplink(self) -> None:
        envelope_bytes = build_envelope(
            gateway_id=99,
            uplink_id=6,
            observed_src_ipv6="fd12:3456::16",
            payload=build_report_packet(node_id=5001, packet_type=0x02),
        )
        self.api.handle_request(
            method="POST",
            path="/api/v1/uplinks",
            headers={"content-type": "application/octet-stream"},
            body=envelope_bytes,
            now=1_700_000_070,
        )
        self.assertIn(99, self.persistence.gateways)
        self.assertIsNone(self.persistence.gateways[99].last_registered_at)

    def test_raw_envelope_storage_is_exact(self) -> None:
        envelope_bytes = build_envelope(
            gateway_id=98,
            uplink_id=7,
            observed_src_ipv6="fd12:3456::17",
            payload=build_report_packet(node_id=5002, packet_type=0x02),
        )
        self.api.handle_request(
            method="POST",
            path="/api/v1/uplinks",
            headers={"content-type": "application/octet-stream"},
            body=envelope_bytes,
            now=1_700_000_071,
        )
        self.assertEqual(envelope_bytes, self.persistence.envelopes[(98, 7)].raw_envelope)

    def _ingest_registration(self, *, node_id: int) -> None:
        envelope_bytes = build_envelope(
            gateway_id=11,
            uplink_id=1,
            observed_src_ipv6="fd12:3456::11",
            payload=build_registration_packet(
                node_id=node_id,
                latitude=40.1,
                longitude=-85.2,
                fw_version=0x0203,
                battery_pct=88,
                parent_ipv6="fd12:3456::1",
            ),
        )
        self.api.handle_request(
            method="POST",
            path="/api/v1/uplinks",
            headers={"content-type": "application/octet-stream"},
            body=envelope_bytes,
            now=1_700_000_040,
        )


def build_registration_packet(
    node_id: int,
    *,
    latitude: float = 39.0,
    longitude: float = -86.0,
    fw_version: int = 0x0102,
    battery_pct: int = 90,
    parent_ipv6: str | None = None,
) -> bytes:
    parent = b"\x00" * 16 if parent_ipv6 is None else ipaddress.IPv6Address(parent_ipv6).packed
    return REGISTRATION_STRUCT.pack(0x01, 1, node_id, latitude, longitude, fw_version, battery_pct, parent)


def build_report_packet(
    node_id: int,
    *,
    packet_type: int,
    timestamp: int = 1_700_000_000,
    risk_level: int = 2,
    temperature: int = 230,
    humidity: int = 550,
    voc: int = 333,
    pm25: int = 18,
    battery_pct: int = 81,
) -> bytes:
    return REPORT_STRUCT.pack(
        packet_type,
        1,
        node_id,
        timestamp,
        risk_level,
        temperature,
        humidity,
        voc,
        pm25,
        battery_pct,
    )


def build_parent_update_packet(node_id: int, *, timestamp: int = 1_700_000_123, parent_ipv6: str | None = None) -> bytes:
    parent = b"\x00" * 16 if parent_ipv6 is None else ipaddress.IPv6Address(parent_ipv6).packed
    return PARENT_UPDATE_STRUCT.pack(0x08, 1, node_id, timestamp, parent)


def build_envelope(*, gateway_id: int, uplink_id: int, observed_src_ipv6: str, payload: bytes, received_at: int = 1_700_000_041) -> bytes:
    return NodeUplinkEnvelope(
        version=1,
        gateway_id=gateway_id,
        uplink_id=uplink_id,
        received_at=received_at,
        observed_src_ipv6=observed_src_ipv6,
        payload=payload,
    ).to_bytes()


class FakePostgresResult:
    def __init__(self, row=None) -> None:
        self._row = row

    def fetchone(self):
        return self._row


class FakePostgresConnection:
    def __init__(self, *, schema_version=None) -> None:
        self.schema_version = schema_version
        self.execute_calls = []
        self.committed = False
        self.rolled_back = False
        self.closed = False

    def execute(self, sql, params=None):
        self.execute_calls.append((sql, params))
        normalized = " ".join(sql.split())
        if normalized == "SELECT version FROM schema_meta LIMIT 1":
            if self.schema_version is None:
                return FakePostgresResult(None)
            return FakePostgresResult((self.schema_version,))
        return FakePostgresResult(None)

    def commit(self):
        self.committed = True

    def rollback(self):
        self.rolled_back = True

    def close(self):
        self.closed = True


class PostgresDatabaseTests(unittest.TestCase):
    def test_migration_runner_applies_schema_version_cleanly(self) -> None:
        connection = FakePostgresConnection(schema_version=None)
        database = PostgresDatabase("postgresql://ignored", connect=lambda _dsn: connection)
        database.apply_migrations()

        statements = [sql for sql, _params in connection.execute_calls]
        self.assertIn("CREATE TABLE IF NOT EXISTS schema_meta (version INTEGER NOT NULL)", statements)
        self.assertIn("DELETE FROM schema_meta", statements)
        self.assertIn("INSERT INTO schema_meta(version) VALUES (%s)", statements)
        self.assertIn("ALTER TABLE uplink_envelopes ALTER COLUMN node_id DROP NOT NULL", statements)
        self.assertTrue(
            any(statement.startswith("CREATE TABLE IF NOT EXISTS malformed_uplink_rejects (") for statement in statements)
        )
        self.assertTrue(connection.committed)
        self.assertEqual(LATEST_SCHEMA_VERSION, connection.execute_calls[-1][1][0])

    def test_transaction_manager_commits_on_success(self) -> None:
        connection = FakePostgresConnection()
        database = PostgresDatabase("postgresql://ignored", connect=lambda _dsn: connection)
        with database.transaction() as conn:
            conn.execute("SELECT 1")
        self.assertTrue(connection.committed)
        self.assertFalse(connection.rolled_back)
        self.assertTrue(connection.closed)

    def test_transaction_manager_rolls_back_on_exception(self) -> None:
        connection = FakePostgresConnection()
        database = PostgresDatabase("postgresql://ignored", connect=lambda _dsn: connection)
        with self.assertRaises(RuntimeError):
            with database.transaction():
                raise RuntimeError("boom")
        self.assertFalse(connection.committed)
        self.assertTrue(connection.rolled_back)
        self.assertTrue(connection.closed)


class PostgresStoreSqlTests(unittest.TestCase):
    def test_gateway_registration_upsert_sql_path(self) -> None:
        connection = FakePostgresConnection()
        registration = GatewayRegistration(
            version=1,
            gateway_id=55,
            timestamp=1_700_000_100,
            latitude=39.1,
            longitude=-86.1,
            sw_version=0x0102,
        )
        store = PostgresGatewayStore()
        store.upsert_registration(
            registration,
            raw_payload=registration.to_bytes(),
            now=1_700_000_101,
            connection=connection,
        )
        self.assertEqual(2, len(connection.execute_calls))
        self.assertIn("INSERT INTO gateways(", connection.execute_calls[0][0])
        self.assertEqual(55, connection.execute_calls[0][1][0])
        self.assertIn("INSERT INTO gateway_registrations", connection.execute_calls[1][0])

    def test_receipt_and_envelope_persistence_sql_path(self) -> None:
        connection = FakePostgresConnection()
        store = PostgresIdempotencyStore(database=None)
        envelope = NodeUplinkEnvelope(
            version=1,
            gateway_id=66,
            uplink_id=77,
            received_at=1_700_000_200,
            observed_src_ipv6="fd12:3456::66",
            payload=build_report_packet(node_id=9001, packet_type=0x02),
        )
        store.store_envelope(
            envelope,
            raw_envelope=envelope.to_bytes(),
            payload_type=0x02,
            node_id=None,
            created_at=1_700_000_201,
            connection=connection,
        )
        receipt = UplinkReceipt(version=1, gateway_id=66, uplink_id=77, status=RECEIPT_PERMANENT_REJECT)
        store.store_receipt(
            receipt,
            receipt_bytes=receipt.to_bytes(),
            detail="bad payload",
            created_at=1_700_000_202,
            connection=connection,
        )
        store.store_malformed_envelope_reject(
            gateway_id=66,
            uplink_id=78,
            version=1,
            raw_request_body=b"broken",
            reject_reason="invalid uplink envelope",
            created_at=1_700_000_203,
            connection=connection,
        )
        self.assertIn("INSERT INTO uplink_envelopes(", connection.execute_calls[0][0])
        self.assertIsNone(connection.execute_calls[0][1][6])
        self.assertIn("INSERT INTO uplink_receipts(", connection.execute_calls[1][0])
        self.assertIn("INSERT INTO malformed_uplink_rejects(", connection.execute_calls[2][0])
