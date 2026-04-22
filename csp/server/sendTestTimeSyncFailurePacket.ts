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

function buildTimeSyncPayload(targetTime: Date) {
  const buffer = Buffer.alloc(6);
  buffer.writeUInt8(0x05, 0);
  buffer.writeUInt8(0x01, 1);
  buffer.writeUInt32LE(Math.floor(targetTime.getTime() / 1000), 2);
  return buffer.toString("hex");
}

async function main() {
  const nodeId = clamp(Math.round(toNumber(process.argv[2], 2)), 1, 65_535);
  const apiBaseUrl = process.argv[3]?.trim() || `http://127.0.0.1:${defaultApiPort}`;
  const minutesAgo = Math.max(DOWNLINK_FAILURE_TIMEOUT_MINUTES + 1, Math.round(toNumber(process.argv[4], 20)));
  const receivedAt = new Date(Date.now() - minutesAgo * 60 * 1000);
  const targetTime = new Date(receivedAt.getTime() + 5 * 60 * 1000);
  const payload = buildTimeSyncPayload(targetTime);

  console.info("[time-sync:packet] Sending stale 0x05 time sync packet", {
    nodeId,
    apiBaseUrl,
    minutesAgo,
    receivedAt: receivedAt.toISOString(),
    targetTime: targetTime.toISOString(),
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

  console.info("[time-sync:packet] Time sync packet ingest accepted");

  const notificationsResponse = await fetch(`${apiBaseUrl}/api/notifications`);
  const notificationsText = await notificationsResponse.text();
  if (!notificationsResponse.ok) {
    throw new Error(`Notification refresh failed with ${notificationsResponse.status}: ${notificationsText}`);
  }

  console.info("[time-sync:packet] Triggered notification evaluation via /api/notifications");
  console.info("[time-sync:packet] If time sync failure email is enabled, you should now see a new delivery attempt.");
}

main().catch((error) => {
  console.error("[time-sync:packet] Unable to create time sync failure scenario", error);
  process.exitCode = 1;
});
