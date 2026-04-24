import { isIP } from "node:net";

const MAX_UINT8 = 0xff;
const MAX_UINT16 = 0xffff;
const MAX_UINT32 = 0xffffffff;
const MAX_UINT64 = 18_446_744_073_709_551_615n;
const MIN_INT16 = -0x8000;
const MAX_INT16 = 0x7fff;

export const CURRENT_BACKHAUL_VERSION = 1;
export const CURRENT_NODE_PACKET_VERSION = 1;

export const gatewayRegistrationMessageType = 0x81;
export const nodeUplinkEnvelopeMessageType = 0x82;
export const uplinkReceiptMessageType = 0x83;
export const downlinkRequestMessageType = 0x84;
export const downlinkResultMessageType = 0x85;

export const registrationPacketType = 0x01;
export const sensorReportPacketType = 0x02;
export const sensorAlertPacketType = 0x03;
export const neighborTableUpdatePacketType = 0x04;
export const timeSyncPacketType = 0x05;
export const configUpdatePacketType = 0x06;
export const parentUpdatePacketType = 0x08;

export const durableIngestReceiptStatus = 0x00;
export const permanentRejectReceiptStatus = 0x01;
export const deliveredDownlinkResultStatus = 0x00;
export const unknownNodeDownlinkResultStatus = 0x01;
export const meshDeliveryFailedDownlinkResultStatus = 0x02;
export const permanentRejectDownlinkResultStatus = 0x03;

export type BackhaulMessageType =
  | typeof gatewayRegistrationMessageType
  | typeof nodeUplinkEnvelopeMessageType
  | typeof uplinkReceiptMessageType
  | typeof downlinkRequestMessageType
  | typeof downlinkResultMessageType;

export type NodeUplinkPacketType =
  | typeof registrationPacketType
  | typeof sensorReportPacketType
  | typeof sensorAlertPacketType
  | typeof parentUpdatePacketType;

export type NodeDownlinkPacketType =
  | typeof neighborTableUpdatePacketType
  | typeof timeSyncPacketType
  | typeof configUpdatePacketType;

export type UplinkReceiptStatus =
  | typeof durableIngestReceiptStatus
  | typeof permanentRejectReceiptStatus;

export type DownlinkResultStatus =
  | typeof deliveredDownlinkResultStatus
  | typeof unknownNodeDownlinkResultStatus
  | typeof meshDeliveryFailedDownlinkResultStatus
  | typeof permanentRejectDownlinkResultStatus;

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
  wisunIpv6: string;
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

export interface DownlinkRequestMessage {
  type: typeof downlinkRequestMessageType;
  version: number;
  gatewayId: number;
  downlinkId: bigint;
  targetNodeId: number;
  createdAtEpochSeconds: number;
  payloadLength: number;
  payload: Buffer;
}

export interface DownlinkResultMessage {
  type: typeof downlinkResultMessageType;
  version: number;
  gatewayId: number;
  downlinkId: bigint;
  targetNodeId: number;
  status: DownlinkResultStatus;
  completedAtEpochSeconds: number;
}

const supportedBackhaulVersions = new Set<number>([CURRENT_BACKHAUL_VERSION]);
const supportedNodePacketVersions = new Set<number>([CURRENT_NODE_PACKET_VERSION]);
const supportedReceiptStatuses = new Set<UplinkReceiptStatus>([
  durableIngestReceiptStatus,
  permanentRejectReceiptStatus,
]);
const supportedDownlinkResultStatuses = new Set<DownlinkResultStatus>([
  deliveredDownlinkResultStatus,
  unknownNodeDownlinkResultStatus,
  meshDeliveryFailedDownlinkResultStatus,
  permanentRejectDownlinkResultStatus,
]);

export function parseGatewayRegistrationMessage(input: Buffer | Uint8Array): ParsedGatewayRegistration {
  const rawMessage = asBuffer(input);
  assertExactLength(rawMessage, 34, "gateway registration");

  const type = rawMessage.readUInt8(0);
  if (type !== gatewayRegistrationMessageType) {
    throw new BackhaulCodecError("invalid_type", `expected message type 0x81, got ${formatByte(type)}`);
  }

  const version = rawMessage.readUInt8(1);
  assertSupportedVersion(version, supportedBackhaulVersions, "backhaul");

  const gatewayId = rawMessage.readUInt16LE(2);
  const timestampEpochSeconds = rawMessage.readUInt32LE(4);
  const wisunIpv6Bytes = rawMessage.subarray(8, 24);
  const latitude = rawMessage.readFloatLE(24);
  const longitude = rawMessage.readFloatLE(28);
  const swVersionPacked = rawMessage.readUInt16LE(32);

  assertIngressIpv6(wisunIpv6Bytes, "wisun_ipv6");
  assertFiniteCoordinate(latitude, "latitude", -90, 90);
  assertFiniteCoordinate(longitude, "longitude", -180, 180);

  return {
    type: gatewayRegistrationMessageType,
    version,
    gatewayId,
    timestampEpochSeconds,
    wisunIpv6: formatIpv6(wisunIpv6Bytes),
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

  assertIngressIpv6(observedSrcIpv6Bytes, "observed_src_ipv6");

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

export function encodeDownlinkRequestMessage(request: {
  version?: number;
  gatewayId: number;
  downlinkId: bigint;
  targetNodeId: number;
  createdAtEpochSeconds: number;
  payload: Buffer | Uint8Array;
}): Buffer {
  const version = request.version ?? CURRENT_BACKHAUL_VERSION;
  assertSupportedVersion(version, supportedBackhaulVersions, "backhaul");
  assertUnsignedInt(request.gatewayId, MAX_UINT16, "gatewayId");
  assertUint64(request.downlinkId, "downlinkId");
  assertUnsignedInt(request.targetNodeId, MAX_UINT16, "targetNodeId");
  assertUnsignedInt(request.createdAtEpochSeconds, MAX_UINT32, "createdAtEpochSeconds");

  const payload = asBuffer(request.payload);
  if (payload.length === 0) {
    throw new BackhaulCodecError("invalid_field_value", "payload must not be empty");
  }
  assertUnsignedInt(payload.length, MAX_UINT16, "payload_len");
  assertNodeDownlinkPayload(payload, request.targetNodeId);

  const output = Buffer.alloc(20 + payload.length);
  output.writeUInt8(downlinkRequestMessageType, 0);
  output.writeUInt8(version, 1);
  output.writeUInt16LE(request.gatewayId, 2);
  output.writeBigUInt64LE(request.downlinkId, 4);
  output.writeUInt16LE(request.targetNodeId, 12);
  output.writeUInt32LE(request.createdAtEpochSeconds, 14);
  output.writeUInt16LE(payload.length, 18);
  payload.copy(output, 20);
  return output;
}

export function parseDownlinkResultMessage(input: Buffer | Uint8Array): DownlinkResultMessage {
  const rawMessage = asBuffer(input);
  assertExactLength(rawMessage, 19, "downlink result");

  const type = rawMessage.readUInt8(0);
  if (type !== downlinkResultMessageType) {
    throw new BackhaulCodecError("invalid_type", `expected message type 0x85, got ${formatByte(type)}`);
  }

  const version = rawMessage.readUInt8(1);
  assertSupportedVersion(version, supportedBackhaulVersions, "backhaul");

  const gatewayId = rawMessage.readUInt16LE(2);
  const downlinkId = rawMessage.readBigUInt64LE(4);
  const targetNodeId = rawMessage.readUInt16LE(12);
  const status = rawMessage.readUInt8(14) as DownlinkResultStatus;
  const completedAtEpochSeconds = rawMessage.readUInt32LE(15);

  if (!supportedDownlinkResultStatuses.has(status)) {
    throw new BackhaulCodecError("invalid_field_value", `unsupported downlink result status ${formatByte(status)}`);
  }

  return {
    type: downlinkResultMessageType,
    version,
    gatewayId,
    downlinkId,
    targetNodeId,
    status,
    completedAtEpochSeconds,
  };
}

export function encodeNeighborTableUpdatePacket(packet: {
  version?: number;
  targetNodeId: number;
  neighborIpv6Addresses: string[];
}): Buffer {
  const version = packet.version ?? CURRENT_NODE_PACKET_VERSION;
  assertSupportedVersion(version, supportedNodePacketVersions, "node packet");
  assertUnsignedInt(packet.targetNodeId, MAX_UINT16, "targetNodeId");
  assertUnsignedInt(packet.neighborIpv6Addresses.length, MAX_UINT8, "nn_count");

  const output = Buffer.alloc(5 + packet.neighborIpv6Addresses.length * 16);
  output.writeUInt8(neighborTableUpdatePacketType, 0);
  output.writeUInt8(version, 1);
  output.writeUInt16LE(packet.targetNodeId, 2);
  output.writeUInt8(packet.neighborIpv6Addresses.length, 4);

  packet.neighborIpv6Addresses.forEach((address, index) => {
    parseIpv6Address(address).copy(output, 5 + index * 16);
  });

  return output;
}

export function encodeTimeSyncPacket(packet: {
  version?: number;
  epochSeconds: number;
}): Buffer {
  const version = packet.version ?? CURRENT_NODE_PACKET_VERSION;
  assertSupportedVersion(version, supportedNodePacketVersions, "node packet");
  assertUnsignedInt(packet.epochSeconds, MAX_UINT32, "epochSeconds");

  const output = Buffer.alloc(6);
  output.writeUInt8(timeSyncPacketType, 0);
  output.writeUInt8(version, 1);
  output.writeUInt32LE(packet.epochSeconds, 2);
  return output;
}

export function encodeConfigUpdatePacket(packet: {
  version?: number;
  configId: number;
  l2TempThresh: number;
  l2HumidityThresh: number;
  l2VocThresh: number;
  l3TempThresh: number;
  l3HumidityThresh: number;
  l3VocThresh: number;
  l4VocThresh: number;
  l5VocThresh: number;
  l5Pm25Thresh: number;
}): Buffer {
  const version = packet.version ?? CURRENT_NODE_PACKET_VERSION;
  assertSupportedVersion(version, supportedNodePacketVersions, "node packet");

  const output = Buffer.alloc(24);
  output.writeUInt8(configUpdatePacketType, 0);
  output.writeUInt8(version, 1);
  output.writeUInt32LE(assertUnsignedInt(packet.configId, MAX_UINT32, "configId"), 2);
  output.writeInt16LE(scaleSignedFixedPoint(packet.l2TempThresh, 100, "l2TempThresh"), 6);
  output.writeUInt16LE(scaleUnsignedFixedPoint(packet.l2HumidityThresh, 100, "l2HumidityThresh"), 8);
  output.writeUInt16LE(assertUnsignedInt(packet.l2VocThresh, MAX_UINT16, "l2VocThresh"), 10);
  output.writeInt16LE(scaleSignedFixedPoint(packet.l3TempThresh, 100, "l3TempThresh"), 12);
  output.writeUInt16LE(scaleUnsignedFixedPoint(packet.l3HumidityThresh, 100, "l3HumidityThresh"), 14);
  output.writeUInt16LE(assertUnsignedInt(packet.l3VocThresh, MAX_UINT16, "l3VocThresh"), 16);
  output.writeUInt16LE(assertUnsignedInt(packet.l4VocThresh, MAX_UINT16, "l4VocThresh"), 18);
  output.writeUInt16LE(assertUnsignedInt(packet.l5VocThresh, MAX_UINT16, "l5VocThresh"), 20);
  output.writeUInt16LE(scaleUnsignedFixedPoint(packet.l5Pm25Thresh, 10, "l5Pm25Thresh"), 22);
  return output;
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
    assertIngressIpv6(parentIpv6Bytes, "parent_ipv6");
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
    assertIngressIpv6(parentIpv6Bytes, "parent_ipv6");
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
  return value;
}

function assertSignedInt(value: number, minValue: number, maxValue: number, label: string) {
  if (!Number.isInteger(value) || value < minValue || value > maxValue) {
    throw new BackhaulCodecError(
      "invalid_field_value",
      `${label} must be an integer between ${minValue} and ${maxValue}, got ${value}`,
    );
  }
  return value;
}

function assertUint64(value: bigint, label: string) {
  if (value < 0n || value > MAX_UINT64) {
    throw new BackhaulCodecError(
      "invalid_field_value",
      `${label} must be between 0 and ${MAX_UINT64.toString()}, got ${value.toString()}`,
    );
  }
}

function assertIngressIpv6(value: Uint8Array, label: string) {
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

function assertNodeDownlinkPayload(payload: Buffer, targetNodeId: number) {
  if (payload.length < 2) {
    throw new BackhaulCodecError("invalid_length", "downlink payload must include type and version");
  }

  const type = payload.readUInt8(0);
  switch (type as NodeDownlinkPacketType) {
    case neighborTableUpdatePacketType: {
      if (payload.length < 5) {
        throw new BackhaulCodecError("invalid_length", "neighbor table update payload must be at least 5 bytes");
      }

      const packetTargetNodeId = payload.readUInt16LE(2);
      const neighborCount = payload.readUInt8(4);
      const expectedLength = 5 + neighborCount * 16;
      if (payload.length !== expectedLength) {
        throw new BackhaulCodecError(
          "invalid_length",
          `neighbor table update payload must be ${expectedLength} bytes, got ${payload.length}`,
        );
      }
      if (packetTargetNodeId !== targetNodeId) {
        throw new BackhaulCodecError(
          "invalid_field_value",
          `neighbor table update target ${packetTargetNodeId} does not match wrapper target ${targetNodeId}`,
        );
      }
      break;
    }
    case timeSyncPacketType:
      assertExactLength(payload, 6, "time sync payload");
      break;
    case configUpdatePacketType:
      assertExactLength(payload, 24, "config update payload");
      break;
    default:
      throw new BackhaulCodecError(
        "unsupported_payload_type",
        `unsupported node downlink packet type ${formatByte(type)}`,
      );
  }
}

function scaleSignedFixedPoint(value: number, multiplier: number, label: string) {
  if (!Number.isFinite(value)) {
    throw new BackhaulCodecError("invalid_field_value", `${label} must be finite, got ${value}`);
  }
  return assertSignedInt(Math.round(value * multiplier), MIN_INT16, MAX_INT16, label);
}

function scaleUnsignedFixedPoint(value: number, multiplier: number, label: string) {
  if (!Number.isFinite(value)) {
    throw new BackhaulCodecError("invalid_field_value", `${label} must be finite, got ${value}`);
  }
  return assertUnsignedInt(Math.round(value * multiplier), MAX_UINT16, label);
}

function parseIpv6Address(value: string): Buffer {
  if (isIP(value) !== 6) {
    throw new BackhaulCodecError("invalid_field_value", `invalid IPv6 address ${value}`);
  }

  const [head, tail = ""] = value.toLowerCase().split("::");
  if (value.split("::").length > 2) {
    throw new BackhaulCodecError("invalid_field_value", `invalid IPv6 address ${value}`);
  }

  const headGroups = head.length > 0 ? head.split(":") : [];
  const tailGroups = tail.length > 0 ? tail.split(":") : [];
  const zeroFillCount = 8 - (headGroups.length + tailGroups.length);
  if (zeroFillCount < 0 || (value.includes("::") ? false : zeroFillCount !== 0)) {
    throw new BackhaulCodecError("invalid_field_value", `invalid IPv6 address ${value}`);
  }

  const groups = value.includes("::")
    ? [...headGroups, ...Array.from({ length: zeroFillCount }, () => "0"), ...tailGroups]
    : headGroups;

  if (groups.length !== 8) {
    throw new BackhaulCodecError("invalid_field_value", `invalid IPv6 address ${value}`);
  }

  const bytes = Buffer.alloc(16);
  groups.forEach((group, index) => {
    if (!/^[0-9a-f]{1,4}$/.test(group)) {
      throw new BackhaulCodecError("invalid_field_value", `invalid IPv6 address ${value}`);
    }
    const parsed = Number.parseInt(group, 16);
    bytes.writeUInt16BE(parsed, index * 2);
  });

  assertIngressIpv6(bytes, "ipv6_address");
  return bytes;
}
