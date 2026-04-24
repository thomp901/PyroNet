from __future__ import annotations

from dataclasses import dataclass

from .config import GatewayConfig
from .protocol.backhaul import GatewayRegistration
from .protocol.node_packets import TYPE_NEIGHBOR_ALERT, packet_type, parse_node_packet
from .storage import (
    SQLiteDatabase,
    SQLiteDeadLetterStore,
    SQLiteDownlinkAuditStore,
    SQLiteNodeStateStore,
    SQLiteOutboxStore,
)


@dataclass(frozen=True)
class IntakeDecision:
    accepted: bool
    reason: str
    uplink_id: int | None = None


class GatewayService:
    def __init__(self, config: GatewayConfig, *, database: SQLiteDatabase) -> None:
        self.config = config
        self.database = database
        self.dead_letter_store = SQLiteDeadLetterStore(database)
        self.downlink_audit_store = SQLiteDownlinkAuditStore(database)
        self.node_state_store = SQLiteNodeStateStore(database)
        self.outbox_store = SQLiteOutboxStore(database, self.dead_letter_store)

    def handle_node_packet(
        self,
        *,
        observed_src_ipv6: str,
        raw_node_packet: bytes,
        received_at: int,
    ) -> IntakeDecision:
        msg_type = packet_type(raw_node_packet)
        if msg_type == TYPE_NEIGHBOR_ALERT:
            return IntakeDecision(accepted=False, reason="neighbor alert ignored for backhaul forwarding")

        parsed = parse_node_packet(raw_node_packet)

        with self.database.transaction() as connection:
            self.node_state_store.update_from_packet(
                parsed,
                observed_src_ipv6=observed_src_ipv6,
                received_at=received_at,
                connection=connection,
            )
            envelope = self.outbox_store.enqueue(
                gateway_id=self.config.gateway_id,
                observed_src_ipv6=observed_src_ipv6,
                payload_type=parsed.packet_type,
                received_at=received_at,
                payload=raw_node_packet,
                version=self.config.backhaul_version,
                connection=connection,
            )

        return IntakeDecision(True, "accepted and enqueued", envelope.uplink_id)

    def build_gateway_registration(self, now: int) -> GatewayRegistration:
        return GatewayRegistration(
            version=self.config.backhaul_version,
            gateway_id=self.config.gateway_id,
            timestamp=now,
            wisun_ipv6=self.config.wisun_ipv6,
            latitude=self.config.latitude,
            longitude=self.config.longitude,
            sw_version=self.config.sw_version,
        )

    @classmethod
    def open(cls, config: GatewayConfig, *, opened_at: int) -> "GatewayService":
        database = SQLiteDatabase(config.runtime.db_path)
        database.persist_gateway_runtime(config, updated_at=opened_at)
        return cls(config, database=database)

    def close(self) -> None:
        self.database.close()
