from __future__ import annotations

from .backhaul_client import PermanentBackhaulError, TransientBackhaulError
from .retry_worker import RetryPolicy


class RegistrationWorker:
    def __init__(self, *, service, backhaul_client, retry_policy: RetryPolicy) -> None:
        self._service = service
        self._backhaul_client = backhaul_client
        self._retry_policy = retry_policy
        self._connected = False
        self._attempt_count = 0
        self._next_attempt_at = 0
        self._last_error: str | None = None

    @property
    def connected(self) -> bool:
        return self._connected

    @property
    def last_error(self) -> str | None:
        return self._last_error

    def run_once(self, now: int) -> bool:
        if self._connected or now < self._next_attempt_at:
            return False
        try:
            self._backhaul_client.send_gateway_registration(self._service.build_gateway_registration(now))
        except (TransientBackhaulError, PermanentBackhaulError) as exc:
            self._connected = False
            self._last_error = str(exc)
            self._next_attempt_at = now + self._retry_policy.next_delay(self._attempt_count)
            self._attempt_count += 1
            return False

        self._connected = True
        self._last_error = None
        self._attempt_count = 0
        self._next_attempt_at = now
        return True

    def note_backhaul_failure(self, now: int) -> None:
        self._connected = False
        self._next_attempt_at = min(self._next_attempt_at, now) if self._next_attempt_at else now
