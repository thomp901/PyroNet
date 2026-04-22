from __future__ import annotations

import urllib.error
import urllib.request

from .protocol.backhaul import UplinkReceipt


class TransientBackhaulError(RuntimeError):
    """Raised when the backhaul did not produce a terminal receipt."""


class PermanentBackhaulError(RuntimeError):
    """Raised when the backhaul returned a terminal client/configuration failure."""


class HTTPBackhaulClient:
    def __init__(self, *, base_url: str, timeout_seconds: float) -> None:
        self._base_url = base_url.rstrip("/")
        self._timeout_seconds = timeout_seconds

    def send_gateway_registration(self, registration) -> None:
        self._post("/api/v1/gateways/register", registration.to_bytes(), expect_receipt=False)

    def send_uplink(self, envelope) -> UplinkReceipt:
        body = self._post("/api/v1/uplinks", envelope.to_bytes(), expect_receipt=True)
        try:
            receipt = UplinkReceipt.from_bytes(body)
        except ValueError as exc:
            raise TransientBackhaulError(f"invalid uplink receipt: {exc}") from exc
        if receipt.gateway_id != envelope.gateway_id or receipt.uplink_id != envelope.uplink_id:
            raise TransientBackhaulError("uplink receipt does not match the posted envelope")
        return receipt

    def _post(self, path: str, body: bytes, *, expect_receipt: bool) -> bytes:
        request = urllib.request.Request(
            url=f"{self._base_url}{path}",
            data=body,
            headers={"Content-Type": "application/octet-stream"},
            method="POST",
        )
        try:
            with urllib.request.urlopen(request, timeout=self._timeout_seconds) as response:
                status = getattr(response, "status", response.getcode())
                response_body = response.read()
        except urllib.error.HTTPError as exc:
            if 400 <= exc.code < 500:
                raise PermanentBackhaulError(f"HTTP {exc.code}") from exc
            raise TransientBackhaulError(f"HTTP {exc.code}") from exc
        except urllib.error.URLError as exc:
            raise TransientBackhaulError(f"connection failure: {exc.reason}") from exc
        except TimeoutError as exc:
            raise TransientBackhaulError("network timeout") from exc

        if status < 200 or status >= 300:
            if 400 <= status < 500:
                raise PermanentBackhaulError(f"HTTP {status}")
            raise TransientBackhaulError(f"HTTP {status}")
        if expect_receipt and not response_body:
            raise TransientBackhaulError("missing uplink receipt body")
        return response_body
