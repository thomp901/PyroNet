from __future__ import annotations

from dataclasses import dataclass

from .backhaul_client import PermanentBackhaulError, TransientBackhaulError
from .protocol.backhaul import RECEIPT_DURABLE_INGEST, RECEIPT_PERMANENT_REJECT


@dataclass(frozen=True)
class RetryPolicy:
    base_delay_seconds: int = 5
    max_delay_seconds: int = 300

    def next_delay(self, attempt_count: int) -> int:
        exponent = max(attempt_count, 0)
        return min(self.base_delay_seconds * (2**exponent), self.max_delay_seconds)


class OutboxRetryWorker:
    def __init__(
        self,
        *,
        outbox_store,
        backhaul_client,
        retry_policy: RetryPolicy,
        registration_worker=None,
    ) -> None:
        self._outbox_store = outbox_store
        self._backhaul_client = backhaul_client
        self._retry_policy = retry_policy
        self._registration_worker = registration_worker

    def run_once(self, now: int, *, limit: int = 100) -> int:
        processed = 0
        for record in self._outbox_store.list_due(now, limit=limit):
            try:
                receipt = self._backhaul_client.send_uplink(record.envelope)
            except TransientBackhaulError as exc:
                if self._registration_worker is not None:
                    self._registration_worker.note_backhaul_failure(now)
                self._outbox_store.mark_retry(
                    record.envelope.uplink_id,
                    attempted_at=now,
                    next_attempt_at=now + self._retry_policy.next_delay(record.attempt_count),
                    error=str(exc),
                )
                processed += 1
                continue
            except PermanentBackhaulError as exc:
                self._outbox_store.move_to_dead_letter(
                    record.envelope.uplink_id,
                    receipt_status=RECEIPT_PERMANENT_REJECT,
                    reason=str(exc),
                    finalized_at=now,
                )
                processed += 1
                continue

            if receipt.status == RECEIPT_DURABLE_INGEST:
                self._outbox_store.delete(record.envelope.uplink_id)
            elif receipt.status == RECEIPT_PERMANENT_REJECT:
                self._outbox_store.move_to_dead_letter(
                    record.envelope.uplink_id,
                    receipt_status=receipt.status,
                    reason="permanent reject from backhaul",
                    finalized_at=now,
                )
            else:
                self._outbox_store.mark_retry(
                    record.envelope.uplink_id,
                    attempted_at=now,
                    next_attempt_at=now + self._retry_policy.next_delay(record.attempt_count),
                    error=f"invalid terminal status 0x{receipt.status:02x}",
                )
            processed += 1
        return processed
