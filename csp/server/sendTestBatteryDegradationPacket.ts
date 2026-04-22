export {};

const defaultApiPort = Number(process.env.API_PORT ?? "4000");

const BATTERY_DEGRADATION_OPEN_THRESHOLD_PCT = 20;
const BATTERY_DEGRADATION_CLEAR_THRESHOLD_PCT = 25;

function toNumber(value: string | undefined, fallback: number) {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : fallback;
}

function clamp(value: number, minimum: number, maximum: number) {
  return Math.min(maximum, Math.max(minimum, value));
}

function buildBatteryTelemetryPayload({
  nodeId,
  occurredAt,
  batteryPct,
  riskLevel,
  temperatureC,
  humidityPct,
  vocIaq,
  pm25UgM3,
}: {
  nodeId: number;
  occurredAt: Date;
  batteryPct: number | null;
  riskLevel: number;
  temperatureC: number;
  humidityPct: number;
  vocIaq: number;
  pm25UgM3: number;
}) {
  const buffer = Buffer.alloc(18);
  buffer.writeUInt8(0x02, 0);
  buffer.writeUInt8(0x01, 1);
  buffer.writeUInt16LE(nodeId, 2);
  buffer.writeUInt32LE(Math.floor(occurredAt.getTime() / 1000), 4);
  buffer.writeUInt8(riskLevel, 8);
  buffer.writeInt16LE(Math.round(temperatureC * 100), 9);
  buffer.writeUInt16LE(Math.round(humidityPct * 100), 11);
  buffer.writeUInt16LE(vocIaq, 13);
  buffer.writeUInt16LE(Math.round(pm25UgM3 * 10), 15);
  buffer.writeUInt8(batteryPct == null ? 0xff : batteryPct, 17);
  return buffer.toString("hex");
}

async function main() {
  const nodeId = clamp(Math.round(toNumber(process.argv[2], 27)), 1, 65_535);
  const batteryPct = clamp(Math.round(toNumber(process.argv[3], 18)), 0, 100);
  const apiBaseUrl = process.argv[4]?.trim() || `http://127.0.0.1:${defaultApiPort}`;
  const occurredAt = new Date();
  const payload = buildBatteryTelemetryPayload({
    nodeId,
    occurredAt,
    batteryPct,
    riskLevel: 2,
    temperatureC: 27.4,
    humidityPct: 48.5,
    vocIaq: 155,
    pm25UgM3: 12.1,
  });
  const shouldOpen = batteryPct <= BATTERY_DEGRADATION_OPEN_THRESHOLD_PCT;
  const shouldClear = batteryPct >= BATTERY_DEGRADATION_CLEAR_THRESHOLD_PCT;

  console.info("[battery:packet] Sending 0x02 battery telemetry packet", {
    nodeId,
    batteryPct,
    apiBaseUrl,
    occurredAt: occurredAt.toISOString(),
    payload,
    expectedBehavior: shouldOpen
      ? "open battery degradation alert"
      : shouldClear
        ? "clear active battery degradation alert"
        : "leave battery degradation alert state unchanged",
  });

  const response = await fetch(`${apiBaseUrl}/api/packets/ingest`, {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    body: JSON.stringify({
      payload,
      encoding: "hex",
      receivedAt: occurredAt.toISOString(),
    }),
  });

  const raw = await response.text();
  let body: unknown = raw;
  try {
    body = JSON.parse(raw);
  } catch {
    // Keep raw text when the response is not JSON.
  }

  if (!response.ok) {
    throw new Error(`Packet ingest failed with ${response.status}: ${typeof body === "string" ? body : JSON.stringify(body)}`);
  }

  console.info("[battery:packet] Packet ingest accepted", body);
  if (shouldOpen) {
    console.info("[battery:packet] If battery degradation email is enabled, you should now see a new delivery attempt.");
  } else if (shouldClear) {
    console.info("[battery:packet] Any open battery degradation alert for this node should now clear.");
  } else {
    console.info("[battery:packet] Battery value is between the open and clear thresholds, so alert state should stay unchanged.");
  }
}

main().catch((error) => {
  console.error("[battery:packet] Unable to send 0x02 battery telemetry packet", error);
  process.exitCode = 1;
});
