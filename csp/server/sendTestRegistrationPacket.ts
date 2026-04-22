import { isIP } from "node:net";

export {};

const defaultApiPort = Number(process.env.API_PORT ?? "4000");

function toNumber(value: string | undefined, fallback: number) {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : fallback;
}

function clamp(value: number, minimum: number, maximum: number) {
  return Math.min(maximum, Math.max(minimum, value));
}

function encodeFirmwareVersion(value: string) {
  const [majorRaw, minorRaw] = value.split(".");
  const major = clamp(Math.round(toNumber(majorRaw, 3)), 0, 255);
  const minor = clamp(Math.round(toNumber(minorRaw, 0)), 0, 255);
  return (major << 8) | minor;
}

function encodeIpv6(value: string | null) {
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

function buildRegistrationPayload({
  nodeId,
  latitude,
  longitude,
  firmwareVersion,
  batteryPct,
  parentIpv6,
}: {
  nodeId: number;
  latitude: number;
  longitude: number;
  firmwareVersion: string;
  batteryPct: number;
  parentIpv6: string | null;
}) {
  const buffer = Buffer.alloc(31);
  buffer.writeUInt8(0x01, 0);
  buffer.writeUInt8(0x01, 1);
  buffer.writeUInt16LE(nodeId, 2);
  buffer.writeFloatLE(latitude, 4);
  buffer.writeFloatLE(longitude, 8);
  buffer.writeUInt16LE(encodeFirmwareVersion(firmwareVersion), 12);
  buffer.writeUInt8(batteryPct, 14);
  encodeIpv6(parentIpv6).copy(buffer, 15);
  return buffer.toString("hex");
}

async function main() {
  const nodeId = clamp(Math.round(toNumber(process.argv[2], 25)), 1, 65_535);
  const apiBaseUrl = process.argv[3]?.trim() || `http://127.0.0.1:${defaultApiPort}`;
  const sourceIpv6 = process.argv[4]?.trim() || `2001:db8:100::${nodeId.toString(16)}`;
  const parentIpv6 = process.argv[5]?.trim() || "2001:db8:100::1";
  const receivedAt = new Date();
  const payload = buildRegistrationPayload({
    nodeId,
    latitude: 40.4237,
    longitude: -86.9212,
    firmwareVersion: "3.1",
    batteryPct: 96,
    parentIpv6: parentIpv6.toLowerCase() === "none" ? null : parentIpv6,
  });

  console.info("[registration:packet] Sending 0x01 registration packet", {
    nodeId,
    apiBaseUrl,
    sourceIpv6,
    parentIpv6: parentIpv6.toLowerCase() === "none" ? null : parentIpv6,
    receivedAt: receivedAt.toISOString(),
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
      sourceIpv6,
      receivedAt: receivedAt.toISOString(),
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

  console.info("[registration:packet] Packet ingest accepted", body);
}

main().catch((error) => {
  console.error("[registration:packet] Unable to send 0x01 registration packet", error);
  process.exitCode = 1;
});
