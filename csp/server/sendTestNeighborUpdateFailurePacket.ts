import { isIP } from "node:net";

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

function encodeIpv6(value: string) {
  if (isIP(value) !== 6) {
    throw new Error(`Invalid IPv6 address: ${value}`);
  }

  const [headRaw, tailRaw] = value.split("::");
  const head = headRaw ? headRaw.split(":").filter(Boolean) : [];
  const tail = tailRaw ? tailRaw.split(":").filter(Boolean) : [];
  const missingCount = 8 - (head.length + tail.length);
  const groups = [
    ...head.map((group) => Number.parseInt(group, 16)),
    ...Array.from({ length: Math.max(missingCount, 0) }, () => 0),
    ...tail.map((group) => Number.parseInt(group, 16)),
  ];

  if (groups.length !== 8 || groups.some((group) => !Number.isInteger(group) || group < 0 || group > 0xffff)) {
    throw new Error(`Unable to encode IPv6 address: ${value}`);
  }

  const buffer = Buffer.alloc(16);
  groups.forEach((group, index) => buffer.writeUInt16BE(group, index * 2));
  return buffer;
}

function buildNeighborDistributionPayload(nodeId: number, neighborIpv6Addresses: string[]) {
  const buffer = Buffer.alloc(5 + neighborIpv6Addresses.length * 16);
  buffer.writeUInt8(0x04, 0);
  buffer.writeUInt8(0x01, 1);
  buffer.writeUInt16LE(nodeId, 2);
  buffer.writeUInt8(neighborIpv6Addresses.length, 4);
  neighborIpv6Addresses.forEach((ipv6Address, index) => {
    encodeIpv6(ipv6Address).copy(buffer, 5 + index * 16);
  });
  return buffer.toString("hex");
}

async function loadNeighborIpv6Addresses(
  apiBaseUrl: string,
  targetNodeId: number,
  neighborNodeIdsCsv: string | undefined,
) {
  const response = await fetch(`${apiBaseUrl}/api/nodes`);
  const raw = await response.text();
  if (!response.ok) {
    throw new Error(`Unable to load nodes with ${response.status}: ${raw}`);
  }

  const nodes = JSON.parse(raw) as Array<{ nodeId: number; ipv6Address: string | null }>;
  const requestedNodeIds = neighborNodeIdsCsv
    ? neighborNodeIdsCsv
        .split(",")
        .map((value) => clamp(Math.round(toNumber(value.trim(), NaN)), 1, 65_535))
        .filter((value) => Number.isFinite(value))
    : [];

  const candidates = nodes.filter((node) => node.nodeId !== targetNodeId && typeof node.ipv6Address === "string");
  const selected = requestedNodeIds.length
    ? requestedNodeIds
        .map((nodeId) => candidates.find((node) => node.nodeId === nodeId))
        .filter((node): node is { nodeId: number; ipv6Address: string } => Boolean(node?.ipv6Address))
    : candidates.slice(0, 2).filter((node): node is { nodeId: number; ipv6Address: string } => Boolean(node.ipv6Address));

  if (selected.length === 0) {
    throw new Error("No neighbor IPv6 addresses are available. Register at least one other node before running this test.");
  }

  return selected.map((node) => node.ipv6Address);
}

async function main() {
  const nodeId = clamp(Math.round(toNumber(process.argv[2], 2)), 1, 65_535);
  const apiBaseUrl = process.argv[3]?.trim() || `http://127.0.0.1:${defaultApiPort}`;
  const minutesAgo = Math.max(DOWNLINK_FAILURE_TIMEOUT_MINUTES + 1, Math.round(toNumber(process.argv[4], 20)));
  const neighborNodeIdsCsv = process.argv[5]?.trim();
  const receivedAt = new Date(Date.now() - minutesAgo * 60 * 1000);
  const neighborIpv6Addresses = await loadNeighborIpv6Addresses(apiBaseUrl, nodeId, neighborNodeIdsCsv);
  const payload = buildNeighborDistributionPayload(nodeId, neighborIpv6Addresses);

  console.info("[neighbor-update:packet] Sending stale 0x04 neighbor update packet", {
    nodeId,
    apiBaseUrl,
    minutesAgo,
    receivedAt: receivedAt.toISOString(),
    neighborIpv6Addresses,
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
      receivedAt: receivedAt.toISOString(),
    }),
  });

  const ingestText = await ingestResponse.text();
  if (!ingestResponse.ok) {
    throw new Error(`Packet ingest failed with ${ingestResponse.status}: ${ingestText}`);
  }

  console.info("[neighbor-update:packet] Neighbor update packet ingest accepted");

  const notificationsResponse = await fetch(`${apiBaseUrl}/api/notifications`);
  const notificationsText = await notificationsResponse.text();
  if (!notificationsResponse.ok) {
    throw new Error(`Notification refresh failed with ${notificationsResponse.status}: ${notificationsText}`);
  }

  console.info("[neighbor-update:packet] Triggered notification evaluation via /api/notifications");
  console.info("[neighbor-update:packet] If neighbor update failure email is enabled, you should now see a new delivery attempt.");
}

main().catch((error) => {
  console.error("[neighbor-update:packet] Unable to create neighbor update failure scenario", error);
  process.exitCode = 1;
});
