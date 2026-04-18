import { isIP } from "node:net";
import type {
  DecodedConfigUpdatePacket,
  DecodedNeighborAlertPacket,
  DecodedNeighborDistributionPacket,
  DecodedPacket,
  DecodedParentUpdatePacket,
  DecodedRegistrationPacket,
  DecodedSensorPacket,
  DecodedTimeSyncPacket,
  PacketEncoding,
  PacketIngestRequest,
} from "../src/api/packetIngest";
import { parseNodeId } from "../src/lib/nodeId";

export class PacketIngestError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "PacketIngestError";
  }
}

function decodePayload(payload: string, encoding: PacketEncoding) {
  if (typeof payload !== "string" || payload.trim() === "") {
    throw new PacketIngestError("payload is required.");
  }

  if (encoding === "hex") {
    const normalized = payload.trim().replace(/^0x/i, "").replace(/\s+/g, "");
    if (normalized === "" || normalized.length % 2 !== 0 || /[^a-fA-F0-9]/.test(normalized)) {
      throw new PacketIngestError("Hex payload must contain an even number of hexadecimal characters.");
    }
    return Buffer.from(normalized, "hex");
  }

  const buffer = Buffer.from(payload.trim(), "base64");
  if (buffer.length === 0) {
    throw new PacketIngestError("Base64 payload could not be decoded.");
  }

  return buffer;
}

function requireLength(buffer: Buffer, expected: number, packetCode: string) {
  if (buffer.length !== expected) {
    throw new PacketIngestError(`${packetCode} packets must be exactly ${expected} bytes.`);
  }
}

function requireNodeId(value: number, fieldName: string) {
  const nodeId = parseNodeId(value);
  if (nodeId === null) {
    throw new PacketIngestError(`${fieldName} must be a valid node id.`);
  }
  return nodeId;
}

function requireIpv6(value: string | null | undefined, fieldName: string) {
  if (typeof value !== "string" || value.trim() === "") {
    throw new PacketIngestError(`${fieldName} is required.`);
  }

  const trimmed = value.trim();
  if (isIP(trimmed) !== 6) {
    throw new PacketIngestError(`${fieldName} must be a valid IPv6 address.`);
  }

  return trimmed;
}

function parseIsoTimestamp(value: string | null | undefined, fieldName: string) {
  if (value == null || value === "") {
    return null;
  }

  const timestamp = new Date(value);
  if (Number.isNaN(timestamp.getTime())) {
    throw new PacketIngestError(`${fieldName} must be a valid ISO-8601 timestamp.`);
  }

  return timestamp.toISOString();
}

function timestampFromSeconds(seconds: number) {
  const timestamp = new Date(seconds * 1000);
  if (Number.isNaN(timestamp.getTime())) {
    throw new PacketIngestError("Packet timestamp must be a valid Unix epoch second.");
  }
  return timestamp.toISOString();
}

function formatFirmwareVersion(packedVersion: number) {
  const major = (packedVersion >> 8) & 0xff;
  const minor = packedVersion & 0xff;
  return `${major}.${minor}`;
}

function formatIpv6(bytes: Buffer) {
  if (bytes.every((value: number) => value === 0)) {
    return null;
  }

  const groups = Array.from({ length: 8 }, (_, index) => bytes.readUInt16BE(index * 2).toString(16));
  let bestStart = -1;
  let bestLength = 0;
  let currentStart = -1;
  let currentLength = 0;

  groups.forEach((group: string, index: number) => {
    if (group === "0") {
      if (currentStart === -1) {
        currentStart = index;
      }
      currentLength += 1;
      if (currentLength > bestLength) {
        bestStart = currentStart;
        bestLength = currentLength;
      }
      return;
    }

    currentStart = -1;
    currentLength = 0;
  });

  if (bestLength < 2) {
    return groups.join(":");
  }

  const left = groups.slice(0, bestStart).join(":");
  const right = groups.slice(bestStart + bestLength).join(":");
  if (!left && !right) {
    return "::";
  }
  if (!left) {
    return `::${right}`;
  }
  if (!right) {
    return `${left}::`;
  }
  return `${left}::${right}`;
}

function buildBase<TPacketCode extends DecodedPacket["packetCode"]>(buffer: Buffer, packetCode: TPacketCode) {
  return {
    packetCode,
    version: buffer.readUInt8(1),
    payloadHex: buffer.toString("hex"),
    payloadSizeBytes: buffer.length,
  };
}

export function decodePacket(request: PacketIngestRequest): DecodedPacket {
  const buffer = decodePayload(request.payload, request.encoding);
  if (buffer.length < 2) {
    throw new PacketIngestError("Packet payload must include the 2-byte type/version header.");
  }

  const packetType = buffer.readUInt8(0);

  switch (packetType) {
    case 0x01: {
      requireLength(buffer, 31, "0x01");
      const nodeId = requireNodeId(buffer.readUInt16LE(2), "node_id");
      const sourceIpv6 = requireIpv6(request.sourceIpv6, "sourceIpv6");
      const packet: DecodedRegistrationPacket = {
        ...buildBase(buffer, "0x01"),
        eventType: "registration",
        direction: "uplink",
        nodeId,
        latitude: buffer.readFloatLE(4),
        longitude: buffer.readFloatLE(8),
        firmwareVersion: formatFirmwareVersion(buffer.readUInt16LE(12)),
        batteryPct: buffer.readUInt8(14),
        sourceIpv6,
        parentIpv6: formatIpv6(buffer.subarray(15, 31)),
      };
      return packet;
    }
    case 0x02:
    case 0x03: {
      requireLength(buffer, 18, `0x${packetType.toString(16).padStart(2, "0")}`);
      const riskLevel = buffer.readUInt8(8);
      if (riskLevel < 1 || riskLevel > 5) {
        throw new PacketIngestError("risk_level must be between 1 and 5.");
      }
      const packet: DecodedSensorPacket = {
        ...buildBase(buffer, packetType === 0x02 ? "0x02" : "0x03"),
        eventType: packetType === 0x02 ? "periodic_report" : "critical_alert",
        direction: "uplink",
        nodeId: requireNodeId(buffer.readUInt16LE(2), "node_id"),
        occurredAt: timestampFromSeconds(buffer.readUInt32LE(4)),
        riskLevel,
        temperatureC: buffer.readInt16LE(9) / 100,
        humidityPct: buffer.readUInt16LE(11) / 100,
        vocIaq: buffer.readUInt16LE(13),
        pm25UgM3: buffer.readUInt16LE(15) / 10,
        batteryPct: buffer.readUInt8(17) === 0xff ? null : buffer.readUInt8(17),
      };
      return packet;
    }
    case 0x04: {
      if (buffer.length < 5) {
        throw new PacketIngestError("0x04 packets must be at least 5 bytes.");
      }
      const nnCount = buffer.readUInt8(4);
      const expectedLength = 5 + nnCount * 16;
      if (buffer.length !== expectedLength) {
        throw new PacketIngestError(`0x04 packets with nn_count=${nnCount} must be ${expectedLength} bytes.`);
      }
      const neighborIpv6Addresses = Array.from({ length: nnCount }, (_, index) => {
        const address = formatIpv6(buffer.subarray(5 + index * 16, 21 + index * 16));
        if (!address) {
          throw new PacketIngestError("0x04 neighbor IPv6 entries may not be all-zero.");
        }
        return address;
      });
      const packet: DecodedNeighborDistributionPacket = {
        ...buildBase(buffer, "0x04"),
        eventType: "neighbor_distribution",
        direction: "downlink",
        nodeId: requireNodeId(buffer.readUInt16LE(2), "target_node_id"),
        neighborIpv6Addresses,
      };
      return packet;
    }
    case 0x05: {
      requireLength(buffer, 6, "0x05");
      const nodeId = request.targetNodeId;
      if (nodeId == null) {
        throw new PacketIngestError("targetNodeId is required for 0x05 packets.");
      }
      const packet: DecodedTimeSyncPacket = {
        ...buildBase(buffer, "0x05"),
        eventType: "time_sync",
        direction: "downlink",
        nodeId: requireNodeId(nodeId, "targetNodeId"),
        occurredAt: timestampFromSeconds(buffer.readUInt32LE(2)),
      };
      return packet;
    }
    case 0x06: {
      requireLength(buffer, 24, "0x06");
      const nodeId = request.targetNodeId;
      if (nodeId == null) {
        throw new PacketIngestError("targetNodeId is required for 0x06 packets.");
      }
      const packet: DecodedConfigUpdatePacket = {
        ...buildBase(buffer, "0x06"),
        eventType: "config_deployment",
        direction: "downlink",
        nodeId: requireNodeId(nodeId, "targetNodeId"),
        configId: buffer.readUInt32LE(2),
        thresholds: {
          l2TempThresh: buffer.readInt16LE(6) / 100,
          l2HumidityThresh: buffer.readUInt16LE(8) / 100,
          l2VocThresh: buffer.readUInt16LE(10),
          l3TempThresh: buffer.readInt16LE(12) / 100,
          l3HumidityThresh: buffer.readUInt16LE(14) / 100,
          l3VocThresh: buffer.readUInt16LE(16),
          l4VocThresh: buffer.readUInt16LE(18),
          l5VocThresh: buffer.readUInt16LE(20),
          l5Pm25Thresh: buffer.readUInt16LE(22) / 10,
        },
      };
      return packet;
    }
    case 0x07: {
      requireLength(buffer, 9, "0x07");
      const riskLevel = buffer.readUInt8(4);
      if (riskLevel < 1 || riskLevel > 5) {
        throw new PacketIngestError("0x07 risk_level must be between 1 and 5.");
      }
      const targetNodeId =
        request.targetNodeId == null ? null : requireNodeId(request.targetNodeId, "targetNodeId");
      const packet: DecodedNeighborAlertPacket = {
        ...buildBase(buffer, "0x07"),
        eventType: "neighbor_alert",
        direction: "lateral",
        nodeId: requireNodeId(buffer.readUInt16LE(2), "node_id"),
        occurredAt: timestampFromSeconds(buffer.readUInt32LE(5)),
        riskLevel,
        targetNodeId,
      };
      return packet;
    }
    case 0x08: {
      requireLength(buffer, 24, "0x08");
      const packet: DecodedParentUpdatePacket = {
        ...buildBase(buffer, "0x08"),
        eventType: "parent_update",
        direction: "uplink",
        nodeId: requireNodeId(buffer.readUInt16LE(2), "node_id"),
        occurredAt: timestampFromSeconds(buffer.readUInt32LE(4)),
        parentIpv6: formatIpv6(buffer.subarray(8, 24)),
      };
      return packet;
    }
    default:
      throw new PacketIngestError(`Unsupported packet type 0x${packetType.toString(16).padStart(2, "0")}.`);
  }
}

export function resolvePacketReceivedAt(request: PacketIngestRequest) {
  return parseIsoTimestamp(request.receivedAt, "receivedAt") ?? new Date().toISOString();
}
