import "./loadEnv";
import { Client } from "pg";
import { isIP } from "node:net";

export const defaultApiPort = Number(process.env.API_PORT ?? "4000");
export const defaultDummyDownlinkUrl = "http://127.0.0.1:9/api/v1/downlinks";

export function toNumber(value: string | undefined, fallback: number) {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : fallback;
}

export function clamp(value: number, minimum: number, maximum: number) {
  return Math.min(maximum, Math.max(minimum, value));
}

export function encodeFirmwareVersion(value: string) {
  const [majorRaw, minorRaw] = value.split(".");
  const major = clamp(Math.round(toNumber(majorRaw, 3)), 0, 255);
  const minor = clamp(Math.round(toNumber(minorRaw, 0)), 0, 255);
  return (major << 8) | minor;
}

export function encodeIpv6(value: string | null) {
  if (!value) {
    return Buffer.alloc(16);
  }

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

export function buildGatewayRegistrationMessage({
  gatewayId,
  reportedAt,
  wisunIpv6,
  latitude,
  longitude,
  softwareVersion,
}: {
  gatewayId: number;
  reportedAt: Date;
  wisunIpv6: string;
  latitude: number;
  longitude: number;
  softwareVersion: string;
}) {
  const buffer = Buffer.alloc(34);
  buffer.writeUInt8(0x81, 0);
  buffer.writeUInt8(0x01, 1);
  buffer.writeUInt16LE(gatewayId, 2);
  buffer.writeUInt32LE(Math.floor(reportedAt.getTime() / 1000), 4);
  encodeIpv6(wisunIpv6).copy(buffer, 8);
  buffer.writeFloatLE(latitude, 24);
  buffer.writeFloatLE(longitude, 28);
  buffer.writeUInt16LE(encodeFirmwareVersion(softwareVersion), 32);
  return buffer;
}

export function buildNodeUplinkEnvelope({
  gatewayId,
  uplinkId,
  receivedAt,
  observedSrcIpv6,
  payload,
}: {
  gatewayId: number;
  uplinkId: bigint;
  receivedAt: Date;
  observedSrcIpv6: string;
  payload: Buffer;
}) {
  const envelope = Buffer.alloc(34 + payload.length);
  envelope.writeUInt8(0x82, 0);
  envelope.writeUInt8(0x01, 1);
  envelope.writeUInt16LE(gatewayId, 2);
  envelope.writeBigUInt64LE(uplinkId, 4);
  envelope.writeUInt32LE(Math.floor(receivedAt.getTime() / 1000), 12);
  encodeIpv6(observedSrcIpv6).copy(envelope, 16);
  envelope.writeUInt16LE(payload.length, 32);
  payload.copy(envelope, 34);
  return envelope;
}

export function buildSensorPayload({
  packetType,
  nodeId,
  occurredAt,
  riskLevel,
  temperatureC,
  humidityPct,
  vocIaq,
  pm25UgM3,
  batteryPct,
}: {
  packetType: 0x02 | 0x03;
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
  buffer.writeUInt8(packetType, 0);
  buffer.writeUInt8(0x01, 1);
  buffer.writeUInt16LE(nodeId, 2);
  buffer.writeUInt32LE(Math.floor(occurredAt.getTime() / 1000), 4);
  buffer.writeUInt8(riskLevel, 8);
  buffer.writeInt16LE(Math.round(temperatureC * 100), 9);
  buffer.writeUInt16LE(Math.round(humidityPct * 100), 11);
  buffer.writeUInt16LE(vocIaq, 13);
  buffer.writeUInt16LE(Math.round(pm25UgM3 * 10), 15);
  buffer.writeUInt8(batteryPct == null ? 0xff : batteryPct, 17);
  return buffer;
}

export function nextUplinkId() {
  return BigInt(Date.now()) * 1000n + BigInt(Math.floor(Math.random() * 1000));
}

export async function postGatewayRegistration(
  apiBaseUrl: string,
  message: Buffer,
  downlinkUrl = defaultDummyDownlinkUrl,
) {
  const response = await fetch(`${apiBaseUrl}/api/v1/gateways/register`, {
    method: "POST",
    headers: {
      "Content-Type": "application/octet-stream",
      "X-PyroNet-Downlink-Url": downlinkUrl,
    },
    body: message,
  });

  const raw = await response.text();
  if (!response.ok) {
    throw new Error(`Gateway registration failed with ${response.status}: ${raw}`);
  }
}

export async function postNodeUplink(apiBaseUrl: string, envelope: Buffer, downlinkUrl = defaultDummyDownlinkUrl) {
  const response = await fetch(`${apiBaseUrl}/api/v1/uplinks`, {
    method: "POST",
    headers: {
      "Content-Type": "application/octet-stream",
      "X-PyroNet-Downlink-Url": downlinkUrl,
    },
    body: envelope,
  });

  const raw = Buffer.from(await response.arrayBuffer());
  if (!response.ok) {
    throw new Error(`Node uplink failed with ${response.status}: ${raw.toString("hex")}`);
  }

  return raw;
}

export async function fetchFirstNodeId(apiBaseUrl: string) {
  const response = await fetch(`${apiBaseUrl}/api/nodes`);
  const raw = await response.text();
  if (!response.ok) {
    throw new Error(`Unable to load nodes with ${response.status}: ${raw}`);
  }

  const nodes = JSON.parse(raw) as Array<{ nodeId: number }>;
  const firstNodeId = nodes[0]?.nodeId;
  if (!Number.isInteger(firstNodeId)) {
    throw new Error("No registered nodes are available. Run the registration test first.");
  }

  return firstNodeId;
}

export async function withDbClient<T>(task: (client: Client) => Promise<T>) {
  const databaseUrl = process.env.DATABASE_URL?.trim();
  if (!databaseUrl) {
    throw new Error("DATABASE_URL is not configured.");
  }

  const client = new Client({ connectionString: databaseUrl });
  await client.connect();
  try {
    return await task(client);
  } finally {
    await client.end();
  }
}
