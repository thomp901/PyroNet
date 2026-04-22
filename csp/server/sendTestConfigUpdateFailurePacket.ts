export {};

const defaultApiPort = Number(process.env.API_PORT ?? "4000");
const DOWNLINK_FAILURE_TIMEOUT_MINUTES = 15;

function toNumber(value: string | undefined, fallback: number) {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : fallback;
}

function clamp(value: number, minimum: number, maximum: number) {
  return Math.min(maximum, Math.max(minimum, value));
}

function buildConfigUpdatePayload(configId: number) {
  const buffer = Buffer.alloc(24);
  buffer.writeUInt8(0x06, 0);
  buffer.writeUInt8(0x01, 1);
  buffer.writeUInt32LE(configId, 2);
  buffer.writeInt16LE(Math.round(55 * 100), 6);
  buffer.writeUInt16LE(Math.round(65 * 100), 8);
  buffer.writeUInt16LE(250, 10);
  buffer.writeInt16LE(Math.round(65 * 100), 12);
  buffer.writeUInt16LE(Math.round(75 * 100), 14);
  buffer.writeUInt16LE(325, 16);
  buffer.writeUInt16LE(500, 18);
  buffer.writeUInt16LE(650, 20);
  buffer.writeUInt16LE(Math.round(85 * 10), 22);
  return buffer.toString("hex");
}

async function main() {
  const nodeId = clamp(Math.round(toNumber(process.argv[2], 2)), 1, 65_535);
  const apiBaseUrl = process.argv[3]?.trim() || `http://127.0.0.1:${defaultApiPort}`;
  const minutesAgo = Math.max(DOWNLINK_FAILURE_TIMEOUT_MINUTES + 1, Math.round(toNumber(process.argv[4], 20)));
  const defaultConfigId = Math.floor(Date.now() / 1000);
  const configId = clamp(Math.round(toNumber(process.argv[5], defaultConfigId)), 1, 0xffff_ffff);
  const receivedAt = new Date(Date.now() - minutesAgo * 60 * 1000);
  const payload = buildConfigUpdatePayload(configId);

  console.info("[config-update:packet] Sending stale 0x06 config update packet", {
    nodeId,
    apiBaseUrl,
    minutesAgo,
    configId,
    receivedAt: receivedAt.toISOString(),
    timeoutThresholdMinutes: DOWNLINK_FAILURE_TIMEOUT_MINUTES,
    payload,
  });

  const ingestResponse = await fetch(`${apiBaseUrl}/api/packets/ingest`, {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    body: JSON.stringify({
      payload,
      encoding: "hex",
      targetNodeId: nodeId,
      receivedAt: receivedAt.toISOString(),
    }),
  });

  const ingestText = await ingestResponse.text();
  if (!ingestResponse.ok) {
    throw new Error(`Packet ingest failed with ${ingestResponse.status}: ${ingestText}`);
  }

  console.info("[config-update:packet] Config update packet ingest accepted");

  const notificationsResponse = await fetch(`${apiBaseUrl}/api/notifications`);
  const notificationsText = await notificationsResponse.text();
  if (!notificationsResponse.ok) {
    throw new Error(`Notification refresh failed with ${notificationsResponse.status}: ${notificationsText}`);
  }

  console.info("[config-update:packet] Triggered notification evaluation via /api/notifications");
  console.info("[config-update:packet] If config update failure email is enabled, you should now see a new delivery attempt.");
}

main().catch((error) => {
  console.error("[config-update:packet] Unable to create config update failure scenario", error);
  process.exitCode = 1;
});
