export {};

const defaultApiPort = Number(process.env.API_PORT ?? "4000");

function toNumber(value: string | undefined, fallback: number) {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : fallback;
}

function clamp(value: number, minimum: number, maximum: number) {
  return Math.min(maximum, Math.max(minimum, value));
}

function buildCriticalRiskPayload({
  nodeId,
  occurredAt,
  riskLevel,
  temperatureC,
  humidityPct,
  vocIaq,
  pm25UgM3,
  batteryPct,
}: {
  nodeId: number;
  occurredAt: Date;
  riskLevel: number;
  temperatureC: number;
  humidityPct: number;
  vocIaq: number;
  pm25UgM3: number;
  batteryPct: number | null;
}) {
  const buffer = Buffer.alloc(18);
  buffer.writeUInt8(0x03, 0);
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
  const nodeId = clamp(Math.round(toNumber(process.argv[2], 2)), 1, 65_535);
  const riskLevel = clamp(Math.round(toNumber(process.argv[3], 5)), 1, 5);
  const apiBaseUrl = process.argv[4]?.trim() || `http://127.0.0.1:${defaultApiPort}`;
  const occurredAt = new Date();
  const payload = buildCriticalRiskPayload({
    nodeId,
    occurredAt,
    riskLevel,
    temperatureC: 82.4,
    humidityPct: 11.8,
    vocIaq: 412,
    pm25UgM3: 96.3,
    batteryPct: 74,
  });

  console.info("[risk:packet] Sending 0x03 critical-risk packet", {
    nodeId,
    riskLevel,
    apiBaseUrl,
    occurredAt: occurredAt.toISOString(),
    payload,
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

  console.info("[risk:packet] Packet ingest accepted", body);
}

main().catch((error) => {
  console.error("[risk:packet] Unable to send 0x03 critical-risk packet", error);
  process.exitCode = 1;
});
