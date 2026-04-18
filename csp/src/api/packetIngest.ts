import type { ConfigThresholds, NodeId, PacketDirection, PacketEventType, PacketLogCode } from "./types";

export type PacketEncoding = "hex" | "base64";

export interface PacketIngestRequest {
  payload: string;
  encoding: PacketEncoding;
  sourceIpv6?: string | null;
  targetNodeId?: NodeId | null;
  receivedAt?: string | null;
}

interface DecodedPacketBase {
  packetCode: PacketLogCode;
  eventType: PacketEventType;
  direction: PacketDirection;
  version: number;
  nodeId: NodeId;
  payloadHex: string;
  payloadSizeBytes: number;
}

export interface DecodedRegistrationPacket extends DecodedPacketBase {
  packetCode: "0x01";
  eventType: "registration";
  direction: "uplink";
  latitude: number;
  longitude: number;
  firmwareVersion: string;
  batteryPct: number;
  sourceIpv6: string;
  parentIpv6: string | null;
}

export interface DecodedSensorPacket extends DecodedPacketBase {
  packetCode: "0x02" | "0x03";
  eventType: "periodic_report" | "critical_alert";
  direction: "uplink";
  occurredAt: string;
  riskLevel: number;
  temperatureC: number;
  humidityPct: number;
  vocIaq: number;
  pm25UgM3: number;
  batteryPct: number | null;
}

export interface DecodedNeighborDistributionPacket extends DecodedPacketBase {
  packetCode: "0x04";
  eventType: "neighbor_distribution";
  direction: "downlink";
  neighborIpv6Addresses: string[];
}

export interface DecodedTimeSyncPacket extends DecodedPacketBase {
  packetCode: "0x05";
  eventType: "time_sync";
  direction: "downlink";
  occurredAt: string;
}

export interface DecodedConfigUpdatePacket extends DecodedPacketBase {
  packetCode: "0x06";
  eventType: "config_deployment";
  direction: "downlink";
  configId: number;
  thresholds: ConfigThresholds;
}

export interface DecodedNeighborAlertPacket extends DecodedPacketBase {
  packetCode: "0x07";
  eventType: "neighbor_alert";
  direction: "lateral";
  occurredAt: string;
  riskLevel: number;
  targetNodeId: NodeId | null;
}

export interface DecodedParentUpdatePacket extends DecodedPacketBase {
  packetCode: "0x08";
  eventType: "parent_update";
  direction: "uplink";
  occurredAt: string;
  parentIpv6: string | null;
}

export type DecodedPacket =
  | DecodedRegistrationPacket
  | DecodedSensorPacket
  | DecodedNeighborDistributionPacket
  | DecodedTimeSyncPacket
  | DecodedConfigUpdatePacket
  | DecodedNeighborAlertPacket
  | DecodedParentUpdatePacket;

export interface PacketIngestResponse {
  storage: "database" | "mock";
  packetCode: PacketLogCode;
  eventType: PacketEventType;
  direction: PacketDirection;
  nodeId: NodeId;
  version: number;
  acceptedAt: string;
  occurredAt: string;
  summary: string;
  detail: string | null;
}
