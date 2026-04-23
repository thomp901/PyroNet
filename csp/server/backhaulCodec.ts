const MAX_UINT8 = 0xff;
const MAX_UINT16 = 0xffff;
const MAX_UINT32 = 0xffffffff;
const MAX_UINT64 = 18_446_744_073_709_551_615n;

export const CURRENT_BACKHAUL_VERSION = 1;
export const CURRENT_NODE_PACKET_VERSION = 1;

export const gatewayRegistrationMessageType = 0x81;
export const nodeUplinkEnvelopeMessageType = 0x82;
export const uplinkReceiptMessageType = 0x83;

export const registrationPacketType = 0x01;
export const sensorReportPacketType = 0x02;
export const sensorAlertPacketType = 0x03;
export const parentUpdatePacketType = 0x08;

export const durableIngestReceiptStatus = 0x00;
export const permanentRejectReceiptStatus = 0x01;

export type BackhaulMessageType =
  | typeof gatewayRegistrationMessageType
  | typeof nodeUplinkEnvelopeMessageType
  | typeof uplinkReceiptMessageType;

export type NodeUplinkPacketType =
  | typeof registrationPacketType
  | typeof sensorReportPacketType
  | typeof sensorAlertPacketType
  | typeof parentUpdatePacketType;

export type UplinkReceiptStatus =
  | typeof durableIngestReceiptStatus
  | typeof permanentRejectReceiptStatus;

export type ParseFailureCode =
  | "invalid_length"
  | "invalid_type"
  | "unsupported_version"
  | "invalid_field_value"
  | "unsupported_payload_type";

export class BackhaulCodecError extends Error {
  readonly code: ParseFailureCode;

  constructor(code: ParseFailureCode, message: string) {
    super(message);
    this.name = "BackhaulCodecError";
    this.code = code;
  }
}

export interface ParsedGatewayRegistration {
  type: typeof gatewayRegistrationMessageType;
  version: number;
  gatewayId: number;
  timestampEpochSeconds: number;
  latitude: number;
  longitude: number;
  swVersionPacked: number;
  rawMessage: Buffer;
}

export interface ParsedRegistrationPacket {
  type: typeof registrationPacketType;
  version: number;
  nodeId: number;
  latitude: number;
  longitude: number;
  fwVersionPacked: number;
  batteryPct: number;
  parentIpv6: string | null;
  rawPayload: Buffer;
}

export interface ParsedSensorPacket {
  type: typeof sensorReportPacketType | typeof sensorAlertPacketType;
  version: number;
  nodeId: number;
  timestampEpochSeconds: number;
  riskLevel: number;
  temperatureRaw: number;
  temperatureC: number;
  humidityRaw: number;
  humidityPct: number;
  bvocPpmRaw: number;
  pm25Raw: number;
  pm25UgM3: number;
  batteryPct: number | null;
  rawPayload: Buffer;
}

export interface ParsedParentUpdatePacket {
  type: typeof parentUpdatePacketType;
  version: number;
  nodeId: number;
  timestampEpochSeconds: number;
  parentIpv6: string | null;
  rawPayload: Buffer;
}

export type ParsedNodeUplinkPacket =
  | ParsedRegistrationPacket
  | ParsedSensorPacket
  | ParsedParentUpdatePacket;

export interface ParsedNodeUplinkEnvelope {
  type: typeof nodeUplinkEnvelopeMessageType;
  version: number;
  gatewayId: number;
  uplinkId: bigint;
  receivedAtEpochSeconds: number;
  observedSrcIpv6: string;
  payloadLength: number;
  payload: Buffer;
  decodedPayload: ParsedNodeUplinkPacket;
  rawEnvelope: Buffer;
}

export interface UplinkReceipt {
  type: typeof uplinkReceiptMessageType;
  version: number;
  gatewayId: number;
  uplinkId: bigint;
  status: UplinkReceiptStatus;
}

const supportedBackhaulVersions = new Set<number>([CURRENT_BACKHAUL_VERSION]);
const supportedNodePacketVersions = new Set<number>([CURRENT_NODE_PACKET_VERSION]);
const supportedReceiptStatuses = new Set<UplinkReceiptStatus>([
  durableIngestReceiptStatus,
  permanentRejectReceiptStatus,
]);

export function parseGatewayRegistrationMessage(input: Buffer | Uint8Array): ParsedGatewayRegistration {
  const rawMessage = asBuffer(input);
  assertExactLength(rawMessage, 18, "gateway registration");

  const type = rawMessage.readUInt8(0);
  if (type !== gatewayRegistrationMessageType) {
    throw new BackhaulCodecError("invalid_type", `expected message type 0x81, got ${formatByte(type)}`);
  }

  const version = rawMessage.readUInt8(1);
  assertSupportedVersion(version, supportedBackhaulVersions, "backhaul");

  const gatewayId = rawMessage.readUInt16LE(2);
  const timestampEpochSeconds = rawMessage.readUInt32LE(4);
  const latitude = rawMessage.readFloatLE(8);
  const longitude = rawMessage.readFloatLE(12);
  const swVersionPacked = rawMessage.readUInt16LE(16);

  assertFiniteCoordinate(latitude, "latitude", -90, 90);
  assertFiniteCoordinate(longitude, "longitude", -180, 180);

  return {
    type: gatewayRegistrationMessageType,
    version,
    gatewayId,
    timestampEpochSeconds,
    latitude,
    longitude,
    swVersionPacked,
    rawMessage,
  };
}

export function parseNodeUplinkEnvelopeMessage(input: Buffer | Uint8Array): ParsedNodeUplinkEnvelope {
  const rawEnvelope = asBuffer(input);
  const minimumLength = 34;
  if (rawEnvelope.length < minimumLength) {
    throw new BackhaulCodecError(
      "invalid_length",
      `node uplink envelope must be at least ${minimumLength} bytes, got ${rawEnvelope.length}`,
    );
  }

  const type = rawEnvelope.readUInt8(0);
  if (type !== nodeUplinkEnvelopeMessageType) {
    throw new BackhaulCodecError("invalid_type", `expected message type 0x82, got ${formatByte(type)}`);
  }

  const version = rawEnvelope.readUInt8(1);
  assertSupportedVersion(version, supportedBackhaulVersions, "backhaul");

  const gatewayId = rawEnvelope.readUInt16LE(2);
  const uplinkId = rawEnvelope.readBigUInt64LE(4);
  const receivedAtEpochSeconds = rawEnvelope.readUInt32LE(12);
  const observedSrcIpv6Bytes = rawEnvelope.subarray(16, 32);
  const payloadLength = rawEnvelope.readUInt16LE(32);
  const expectedLength = minimumLength + payloadLength;

  if (rawEnvelope.length !== expectedLength) {
    throw new BackhaulCodecError(
      "invalid_length",
      `node uplink envelope length ${rawEnvelope.length} does not match payload_len ${payloadLength} (${expectedLength} expected)`,
    );
  }

  if (payloadLength <= 0) {
    throw new BackhaulCodecError("invalid_field_value", "payload_len must be greater than zero");
  }

  assertGlobalUnicastIpv6(observedSrcIpv6Bytes, "observed_src_ipv6");

  const payload = rawEnvelope.subarray(minimumLength, expectedLength);
  const decodedPayload = parseNodeUplinkPacket(payload);

  return {
    type: nodeUplinkEnvelopeMessageType,
    version,
    gatewayId,
    uplinkId,
    receivedAtEpochSeconds,
    observedSrcIpv6: formatIpv6(observedSrcIpv6Bytes),
    payloadLength,
    payload,
    decodedPayload,
    rawEnvelope,
  };
}

export function parseNodeUplinkPacket(input: Buffer | Uint8Array): ParsedNodeUplinkPacket {
  const rawPayload = asBuffer(input);
  if (rawPayload.length < 2) {
    throw new BackhaulCodecError("invalid_length", "node uplink payload must include type and version");
  }

  const type = rawPayload.readUInt8(0);

  switch (type as NodeUplinkPacketType) {
    case registrationPacketType:
      return parseRegistrationPacket(rawPayload);
    case sensorReportPacketType:
    case sensorAlertPacketType:
      return parseSensorPacket(rawPayload);
    case parentUpdatePacketType:
      return parseParentUpdatePacket(rawPayload);
    default:
      throw new BackhaulCodecError(
        "unsupported_payload_type",
        `unsupported node uplink packet type ${formatByte(type)}`,
      );
  }
}

export function encodeUplinkReceipt(receipt: {
  version?: number;
  gatewayId: number;
  uplinkId: bigint;
  status: UplinkReceiptStatus;
}): Buffer {
  const version = receipt.version ?? CURRENT_BACKHAUL_VERSION;
  assertSupportedVersion(version, supportedBackhaulVersions, "backhaul");
  assertUnsignedInt(receipt.gatewayId, MAX_UINT16, "gatewayId");
  assertUint64(receipt.uplinkId, "uplinkId");

  if (!supportedReceiptStatuses.has(receipt.status)) {
    throw new BackhaulCodecError(
      "invalid_field_value",
      `unsupported uplink receipt status ${formatByte(receipt.status)}`,
    );
  }

  const output = Buffer.alloc(13);
  output.writeUInt8(uplinkReceiptMessageType, 0);
  output.writeUInt8(version, 1);
  output.writeUInt16LE(receipt.gatewayId, 2);
  output.writeBigUInt64LE(receipt.uplinkId, 4);
  output.writeUInt8(receipt.status, 12);
  return output;
}

export function parseUplinkReceiptMessage(input: Buffer | Uint8Array): UplinkReceipt {
  const rawMessage = asBuffer(input);
  assertExactLength(rawMessage, 13, "uplink receipt");

  const type = rawMessage.readUInt8(0);
  if (type !== uplinkReceiptMessageType) {
    throw new BackhaulCodecError("invalid_type", `expected message type 0x83, got ${formatByte(type)}`);
  }

  const version = rawMessage.readUInt8(1);
  assertSupportedVersion(version, supportedBackhaulVersions, "backhaul");

  const gatewayId = rawMessage.readUInt16LE(2);
  const uplinkId = rawMessage.readBigUInt64LE(4);
  const status = rawMessage.readUInt8(12) as UplinkReceiptStatus;

  if (!supportedReceiptStatuses.has(status)) {
    throw new BackhaulCodecError("invalid_field_value", `unsupported uplink receipt status ${formatByte(status)}`);
  }

  return {
    type: uplinkReceiptMessageType,
    version,
    gatewayId,
    uplinkId,
    status,
  };
}

function parseRegistrationPacket(rawPayload: Buffer): ParsedRegistrationPacket {
  assertExactLength(rawPayload, 31, "registration packet");
  const version = rawPayload.readUInt8(1);
  assertSupportedVersion(version, supportedNodePacketVersions, "node packet");

  const nodeId = rawPayload.readUInt16LE(2);
  const latitude = rawPayload.readFloatLE(4);
  const longitude = rawPayload.readFloatLE(8);
  const fwVersionPacked = rawPayload.readUInt16LE(12);
  const batteryPct = rawPayload.readUInt8(14);
  const parentIpv6Bytes = rawPayload.subarray(15, 31);

  assertFiniteCoordinate(latitude, "latitude", -90, 90);
  assertFiniteCoordinate(longitude, "longitude", -180, 180);
  assertUnsignedInt(batteryPct, 100, "battery_pct");

  if (!isZeroIpv6(parentIpv6Bytes)) {
    assertGlobalUnicastIpv6(parentIpv6Bytes, "parent_ipv6");
  }

  return {
    type: registrationPacketType,
    version,
    nodeId,
    latitude,
    longitude,
    fwVersionPacked,
    batteryPct,
    parentIpv6: isZeroIpv6(parentIpv6Bytes) ? null : formatIpv6(parentIpv6Bytes),
    rawPayload,
  };
}

function parseSensorPacket(rawPayload: Buffer): ParsedSensorPacket {
  assertExactLength(rawPayload, 18, "sensor packet");
  const type = rawPayload.readUInt8(0) as typeof sensorReportPacketType | typeof sensorAlertPacketType;
  const version = rawPayload.readUInt8(1);
  assertSupportedVersion(version, supportedNodePacketVersions, "node packet");

  const nodeId = rawPayload.readUInt16LE(2);
  const timestampEpochSeconds = rawPayload.readUInt32LE(4);
  const riskLevel = rawPayload.readUInt8(8);
  const temperatureRaw = rawPayload.readInt16LE(9);
  const humidityRaw = rawPayload.readUInt16LE(11);
  const bvocPpmRaw = rawPayload.readUInt16LE(13);
  const pm25Raw = rawPayload.readUInt16LE(15);
  const batteryRaw = rawPayload.readUInt8(17);

  if (riskLevel < 1 || riskLevel > 5) {
    throw new BackhaulCodecError("invalid_field_value", `risk_level must be between 1 and 5, got ${riskLevel}`);
  }

  if (humidityRaw > 10_000) {
    throw new BackhaulCodecError(
      "invalid_field_value",
      `humidity raw value ${humidityRaw} exceeds 100.00%RH encoding`,
    );
  }

  const batteryPct = batteryRaw === 0xff ? null : batteryRaw;
  if (batteryPct !== null) {
    assertUnsignedInt(batteryPct, 100, "battery_pct");
  }

  return {
    type,
    version,
    nodeId,
    timestampEpochSeconds,
    riskLevel,
    temperatureRaw,
    temperatureC: temperatureRaw / 100,
    humidityRaw,
    humidityPct: humidityRaw / 100,
    bvocPpmRaw,
    pm25Raw,
    pm25UgM3: pm25Raw / 10,
    batteryPct,
    rawPayload,
  };
}

function parseParentUpdatePacket(rawPayload: Buffer): ParsedParentUpdatePacket {
  assertExactLength(rawPayload, 24, "parent update packet");
  const version = rawPayload.readUInt8(1);
  assertSupportedVersion(version, supportedNodePacketVersions, "node packet");

  const nodeId = rawPayload.readUInt16LE(2);
  const timestampEpochSeconds = rawPayload.readUInt32LE(4);
  const parentIpv6Bytes = rawPayload.subarray(8, 24);

  if (!isZeroIpv6(parentIpv6Bytes)) {
    assertGlobalUnicastIpv6(parentIpv6Bytes, "parent_ipv6");
  }

  return {
    type: parentUpdatePacketType,
    version,
    nodeId,
    timestampEpochSeconds,
    parentIpv6: isZeroIpv6(parentIpv6Bytes) ? null : formatIpv6(parentIpv6Bytes),
    rawPayload,
  };
}

function asBuffer(input: Buffer | Uint8Array): Buffer {
  return Buffer.isBuffer(input) ? input : Buffer.from(input);
}

function assertExactLength(value: Buffer, expectedLength: number, label: string) {
  if (value.length !== expectedLength) {
    throw new BackhaulCodecError(
      "invalid_length",
      `${label} must be exactly ${expectedLength} bytes, got ${value.length}`,
    );
  }
}

function assertSupportedVersion(version: number, supportedVersions: Set<number>, label: string) {
  assertUnsignedInt(version, MAX_UINT8, `${label} version`);
  if (!supportedVersions.has(version)) {
    throw new BackhaulCodecError("unsupported_version", `unsupported ${label} version ${version}`);
  }
}

function assertFiniteCoordinate(value: number, label: string, min: number, max: number) {
  if (!Number.isFinite(value) || value < min || value > max) {
    throw new BackhaulCodecError(
      "invalid_field_value",
      `${label} must be a finite value between ${min} and ${max}, got ${value}`,
    );
  }
}

function assertUnsignedInt(value: number, maxValue: number, label: string) {
  if (!Number.isInteger(value) || value < 0 || value > maxValue) {
    throw new BackhaulCodecError(
      "invalid_field_value",
      `${label} must be an unsigned integer between 0 and ${maxValue}, got ${value}`,
    );
  }
}

function assertUint64(value: bigint, label: string) {
  if (value < 0n || value > MAX_UINT64) {
    throw new BackhaulCodecError(
      "invalid_field_value",
      `${label} must be between 0 and ${MAX_UINT64.toString()}, got ${value.toString()}`,
    );
  }
}

function assertGlobalUnicastIpv6(value: Uint8Array, label: string) {
  if (value.length !== 16) {
    throw new BackhaulCodecError("invalid_length", `${label} must be 16 bytes`);
  }

  if (isZeroIpv6(value)) {
    throw new BackhaulCodecError("invalid_field_value", `${label} must not be all zeroes`);
  }

  const firstByte = value[0];
  const secondByte = value[1];

  if (firstByte === 0xff) {
    throw new BackhaulCodecError("invalid_field_value", `${label} must not be multicast`);
  }

  if (firstByte === 0xfe && (secondByte & 0xc0) === 0x80) {
    throw new BackhaulCodecError("invalid_field_value", `${label} must not be link-local`);
  }

  const isLoopback = value.subarray(0, 15).every((byte) => byte === 0) && value[15] === 1;
  if (isLoopback) {
    throw new BackhaulCodecError("invalid_field_value", `${label} must not be loopback`);
  }
}

function isZeroIpv6(value: Uint8Array) {
  return value.every((byte) => byte === 0);
}

function formatIpv6(value: Uint8Array): string {
  const groups: string[] = [];
  for (let index = 0; index < 16; index += 2) {
    groups.push(((value[index] << 8) | value[index + 1]).toString(16));
  }

  let bestStart = -1;
  let bestLength = 0;
  let currentStart = -1;
  let currentLength = 0;

  for (let index = 0; index <= groups.length; index += 1) {
    const isZeroGroup = index < groups.length && groups[index] === "0";
    if (isZeroGroup) {
      if (currentStart === -1) {
        currentStart = index;
      }
      currentLength += 1;
      continue;
    }

    if (currentLength > bestLength && currentLength >= 2) {
      bestStart = currentStart;
      bestLength = currentLength;
    }

    currentStart = -1;
    currentLength = 0;
  }

  if (bestStart === -1) {
    return groups.join(":");
  }

  const left = groups.slice(0, bestStart).join(":");
  const right = groups.slice(bestStart + bestLength).join(":");

  if (left.length === 0 && right.length === 0) {
    return "::";
  }
  if (left.length === 0) {
    return `::${right}`;
  }
  if (right.length === 0) {
    return `${left}::`;
  }
  return `${left}::${right}`;
}

function formatByte(value: number) {
  return `0x${value.toString(16).padStart(2, "0")}`;
}
