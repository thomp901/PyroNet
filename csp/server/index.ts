import "./loadEnv";
import cors from "cors";
import express from "express";
import { Pool, type PoolClient, type QueryResultRow } from "pg";
import {
  BackhaulCodecError,
  CURRENT_BACKHAUL_VERSION,
  CURRENT_NODE_PACKET_VERSION,
  durableIngestReceiptStatus,
  deliveredDownlinkResultStatus,
  encodeConfigUpdatePacket,
  encodeDownlinkRequestMessage,
  encodeNeighborTableUpdatePacket,
  encodeTimeSyncPacket,
  encodeUplinkReceipt,
  meshDeliveryFailedDownlinkResultStatus,
  neighborTableUpdatePacketType,
  parseGatewayRegistrationMessage,
  parseDownlinkResultMessage,
  parseNodeUplinkEnvelopeMessage,
  permanentRejectDownlinkResultStatus,
  registrationPacketType,
  sensorAlertPacketType,
  sensorReportPacketType,
  timeSyncPacketType,
  configUpdatePacketType,
  unknownNodeDownlinkResultStatus,
  parentUpdatePacketType,
  type ParsedNodeUplinkEnvelope,
  type ParsedParentUpdatePacket,
  type ParsedRegistrationPacket,
  type ParsedSensorPacket,
} from "./backhaulCodec";
import type {
  AlertIncident,
  AlertTimelineEntry,
  BorderRouterDetail,
  ConfigRevisionDraft,
  ConfigThresholds,
  ConfigurationResponse,
  ConnectivityStatus,
  DashboardResponse,
  DownlinkRequest,
  GatewayMarker,
  HistoryAggregateBucket,
  HistoryResponse,
  HistoryTrendSummary,
  HistoryWindow,
  MeshLink,
  NeighborMembership,
  NeighborRevision,
  NeighborRevisionDraft,
  NodeDetail,
  NodeId,
  NodeSummary,
  PacketDirection,
  PacketEventType,
  PacketHistoryQuery,
  PacketHistoryResponse,
  PacketLogCode,
  PacketLogEntry,
  PacketLogStatus,
  NotificationEventType,
  NotificationRecipientUpdate,
  NotificationSettingsResponse,
  ReadingHistoryPoint,
  TelemetrySnapshot,
} from "../src/api/types";
import { DEFAULT_CONFIG_THRESHOLDS, configThresholdKeys } from "../src/api/configThresholds";
import { packetDirections, packetEventTypes, packetLogCodes } from "../src/api/types";
import { parseGatewayId } from "../src/lib/gatewayId";
import { parseNodeId } from "../src/lib/nodeId";
import {
  createMockConfigRevision,
  createMockNeighborDistribution,
  createMockThresholdPush,
  createMockTimeSync,
  getMockBorderRouterDetail,
  getMockConfiguration,
  getMockDashboard,
  getMockHistory,
  getMockPacketHistory,
  getMockNodeDetail,
  getMockNotificationSettings,
  listMockAlerts,
  listMockGateways,
  listMockNodes,
  updateMockNeighborRevision,
  updateMockNotificationRecipient,
} from "./mocks/mockBackend";

const app = express();
const port = Number(process.env.API_PORT ?? "4000");
const databaseUrl = process.env.DATABASE_URL?.trim();
const pool = databaseUrl ? new Pool({ connectionString: databaseUrl }) : null;
const defaultGatewayDownlinkPort = 8081;
const gatewayDownlinkScheme = (process.env.GATEWAY_DOWNLINK_SCHEME ?? "http").trim() || "http";
const gatewayDownlinkPort = parseOptionalPort(process.env.GATEWAY_DOWNLINK_PORT) ?? defaultGatewayDownlinkPort;
const gatewayDownlinkRequestPath = normalizeHttpPath(process.env.GATEWAY_DOWNLINK_REQUEST_PATH ?? "/api/v1/downlinks");
const gatewayDownlinkDispatchIntervalMs = parsePositiveInteger(process.env.GATEWAY_DOWNLINK_DISPATCH_INTERVAL_MS, 5_000);
const gatewayDownlinkDispatchBatchSize = parsePositiveInteger(process.env.GATEWAY_DOWNLINK_DISPATCH_BATCH_SIZE, 10);
const gatewayDownlinkTimeoutMs = parsePositiveInteger(process.env.GATEWAY_DOWNLINK_TIMEOUT_MS, 10_000);
const gatewayDownlinkRetryDelayMs = parsePositiveInteger(process.env.GATEWAY_DOWNLINK_RETRY_DELAY_MS, 15_000);
const degradedThresholdMs = 2 * 60 * 1000;
const offlineThresholdMs = 5 * 60 * 1000;
let isGatewayDownlinkDispatchInFlight = false;
const notificationEventTypes: NotificationEventType[] = [
  "critical_risk",
  "connectivity_loss",
  "battery_degradation",
  "system",
  "time_sync_failure",
  "nn_update_failure",
  "config_update_failure",
];

app.use(cors());
app.use(express.json());
const octetStreamBody = express.raw({ type: "application/octet-stream", limit: "4kb" });

function getNowMs() {
  return Date.now();
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null;
}

function normalizeConfigThresholds(input: unknown): ConfigThresholds {
  const source = isRecord(input) ? input : {};
  const normalized = { ...DEFAULT_CONFIG_THRESHOLDS };

  for (const key of configThresholdKeys) {
    const value = source[key];
    if (value === undefined || value === null) {
      continue;
    }
    if (typeof value !== "number" || !Number.isFinite(value)) {
      throw new Error(`${key} must be a finite number.`);
    }
    normalized[key] = value;
  }

  validateConfigThresholds(normalized);
  return normalized;
}

function validateConfigThresholds(thresholds: ConfigThresholds) {
  assertThresholdRange(thresholds.l2TempThresh, -327.68, 327.67, "l2TempThresh");
  assertThresholdRange(thresholds.l3TempThresh, -327.68, 327.67, "l3TempThresh");
  assertThresholdRange(thresholds.l2HumidityThresh, 0, 100, "l2HumidityThresh");
  assertThresholdRange(thresholds.l3HumidityThresh, 0, 100, "l3HumidityThresh");
  assertThresholdInteger(thresholds.l2VocThresh, 0, 65_535, "l2VocThresh");
  assertThresholdInteger(thresholds.l3VocThresh, 0, 65_535, "l3VocThresh");
  assertThresholdInteger(thresholds.l4VocThresh, 0, 65_535, "l4VocThresh");
  assertThresholdInteger(thresholds.l5VocThresh, 0, 65_535, "l5VocThresh");
  assertThresholdRange(thresholds.l5Pm25Thresh, 0, 6_553.5, "l5Pm25Thresh");

  if (thresholds.l2TempThresh > thresholds.l3TempThresh) {
    throw new Error("l2TempThresh must be less than or equal to l3TempThresh.");
  }
  if (thresholds.l3HumidityThresh > thresholds.l2HumidityThresh) {
    throw new Error("l3HumidityThresh must be less than or equal to l2HumidityThresh.");
  }
  if (thresholds.l2VocThresh > thresholds.l3VocThresh) {
    throw new Error("l2VocThresh must be less than or equal to l3VocThresh.");
  }
  if (thresholds.l3VocThresh > thresholds.l4VocThresh) {
    throw new Error("l3VocThresh must be less than or equal to l4VocThresh.");
  }
  if (thresholds.l4VocThresh > thresholds.l5VocThresh) {
    throw new Error("l4VocThresh must be less than or equal to l5VocThresh.");
  }
}

function assertThresholdRange(
  value: number,
  minimum: number,
  maximum: number,
  fieldName: (typeof configThresholdKeys)[number],
) {
  if (value < minimum || value > maximum) {
    throw new Error(`${fieldName} must be between ${minimum} and ${maximum}.`);
  }
}

function assertThresholdInteger(
  value: number,
  minimum: number,
  maximum: number,
  fieldName: (typeof configThresholdKeys)[number],
) {
  if (!Number.isInteger(value)) {
    throw new Error(`${fieldName} must be an integer.`);
  }
  assertThresholdRange(value, minimum, maximum, fieldName);
}

function timestampMs(value: string | null) {
  return value ? new Date(value).getTime() : 0;
}

function formatCoordinateDms(value: number, positiveHemisphere: string, negativeHemisphere: string) {
  const hemisphere = value >= 0 ? positiveHemisphere : negativeHemisphere;
  const absoluteValue = Math.abs(value);
  let degrees = Math.floor(absoluteValue);
  const minutesFloat = (absoluteValue - degrees) * 60;
  let minutes = Math.floor(minutesFloat);
  let seconds = Number(((minutesFloat - minutes) * 60).toFixed(1));

  if (seconds >= 60) {
    seconds = 0;
    minutes += 1;
  }

  if (minutes >= 60) {
    minutes = 0;
    degrees += 1;
  }

  return `${degrees}°${String(minutes).padStart(2, "0")}'${seconds.toFixed(1).padStart(4, "0")}"${hemisphere}`;
}

function formatCoordinatePair(latitude: number, longitude: number) {
  return `${formatCoordinateDms(latitude, "N", "S")}, ${formatCoordinateDms(longitude, "E", "W")}`;
}

function connectivityFromLastSeen(lastSeenAt: string | null): ConnectivityStatus {
  if (!lastSeenAt) {
    return "offline";
  }

  const ageMs = getNowMs() - timestampMs(lastSeenAt);

  if (ageMs >= offlineThresholdMs) {
    return "offline";
  }

  if (ageMs >= degradedThresholdMs) {
    return "degraded";
  }

  return "online";
}

function haversineDistanceMeters(
  first: { lat: number; lng: number },
  second: { lat: number; lng: number },
) {
  const toRad = (value: number) => (value * Math.PI) / 180;
  const earthRadius = 6_371_000;
  const deltaLat = toRad(second.lat - first.lat);
  const deltaLng = toRad(second.lng - first.lng);
  const a =
    Math.sin(deltaLat / 2) * Math.sin(deltaLat / 2) +
    Math.cos(toRad(first.lat)) * Math.cos(toRad(second.lat)) * Math.sin(deltaLng / 2) * Math.sin(deltaLng / 2);
  const c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
  return Math.round(earthRadius * c);
}

interface RoutedDeviceTarget {
  deviceId: number;
  nodeId: number;
  gatewayRowId: number;
  gatewayId: number;
  lastObservedGatewayAt: string;
  currentNeighborRevisionId: number | null;
  downlinkUrl: string;
}

async function query<T extends QueryResultRow>(text: string, values: unknown[] = []) {
  if (!pool) {
    throw new Error("Database is not configured.");
  }
  return pool.query<T>(text, values);
}

function epochSecondsToDate(epochSeconds: number) {
  return new Date(epochSeconds * 1000);
}

function normalizePgError(error: unknown) {
  return error instanceof Error ? error.message : "Unexpected database error";
}

function parseOptionalPort(value: string | undefined) {
  if (!value) {
    return null;
  }

  const parsed = Number.parseInt(value, 10);
  return Number.isInteger(parsed) && parsed > 0 && parsed <= 65535 ? parsed : null;
}

function parsePositiveInteger(value: string | undefined, fallback: number) {
  if (!value) {
    return fallback;
  }

  const parsed = Number.parseInt(value, 10);
  return Number.isInteger(parsed) && parsed > 0 ? parsed : fallback;
}

function normalizeHttpPath(value: string) {
  const trimmed = value.trim();
  if (trimmed.length === 0) {
    return "/api/v1/downlinks";
  }
  return trimmed.startsWith("/") ? trimmed : `/${trimmed}`;
}

const stableRejectCodes = {
  invalidLength: "invalid_length",
  invalidType: "invalid_type",
  unsupportedVersion: "unsupported_version",
  invalidFieldValue: "invalid_field_value",
  unsupportedPayloadType: "unsupported_payload_type",
} as const;

type StableRejectCode = (typeof stableRejectCodes)[keyof typeof stableRejectCodes];

function formatPackedVersion(version: number) {
  return `${(version >> 8) & 0xff}.${version & 0xff}`;
}

function buildGatewayProjectionMetadata(
  message: ParsedNodeUplinkEnvelope,
  extra: Record<string, string | number | boolean | null> = {},
) {
  return {
    gatewayId: message.gatewayId,
    uplinkId: message.uplinkId.toString(),
    backhaulVersion: message.version,
    observedSrcIpv6: message.observedSrcIpv6,
    receivedAt: epochSecondsToDate(message.receivedAtEpochSeconds).toISOString(),
    payloadType: message.decodedPayload.type,
    payloadVersion: message.decodedPayload.version,
    ...extra,
  };
}

async function syncDeviceIpv6HistoryInDb(
  client: PoolClient,
  deviceId: number,
  observedIpv6: string,
  validFrom: Date,
  registrationId: number | null = null,
) {
  const currentIpv6Result = await client.query<{
    id: number;
    ipv6_address: string;
  }>(
    `
      SELECT id, host(ipv6_address) AS ipv6_address
      FROM device_ipv6_history
      WHERE device_id = $1
        AND valid_to IS NULL
      FOR UPDATE
    `,
    [deviceId],
  );

  const currentIpv6 = currentIpv6Result.rows[0];
  if (!currentIpv6) {
    await client.query(
      `
        INSERT INTO device_ipv6_history (device_id, ipv6_address, registration_id, valid_from)
        VALUES ($1, $2, $3, $4)
      `,
      [deviceId, observedIpv6, registrationId, validFrom],
    );
    return;
  }

  if (currentIpv6.ipv6_address === observedIpv6) {
    if (registrationId !== null) {
      await client.query(
        `
          UPDATE device_ipv6_history
          SET registration_id = $2
          WHERE id = $1
        `,
        [currentIpv6.id, registrationId],
      );
    }
    return;
  }

  await client.query(
    `
      UPDATE device_ipv6_history
      SET valid_to = $2
      WHERE id = $1
    `,
    [currentIpv6.id, validFrom],
  );

  await client.query(
    `
      INSERT INTO device_ipv6_history (device_id, ipv6_address, registration_id, valid_from)
      VALUES ($1, $2, $3, $4)
    `,
    [deviceId, observedIpv6, registrationId, validFrom],
  );
}

async function insertParentObservationInDb(
  client: PoolClient,
  options: {
    deviceId: number;
    gatewayUplinkId: number;
    observedParentIpv6: string | null;
    observedAt: Date;
    sourceType: "registration" | "parent_update";
    metadata: Record<string, string | number | boolean | null>;
  },
) {
  await client.query(
    `
      INSERT INTO device_parent_observations (
        device_id,
        gateway_uplink_id,
        source_type,
        observed_parent_ipv6,
        observed_at,
        raw_payload_metadata
      )
      VALUES ($1, $2, $3, $4, $5, $6::jsonb)
    `,
    [
      options.deviceId,
      options.gatewayUplinkId,
      options.sourceType,
      options.observedParentIpv6,
      options.observedAt,
      JSON.stringify(options.metadata),
    ],
  );
}

async function projectRegistrationUplinkInDb(
  client: PoolClient,
  gatewayRowId: number,
  gatewayUplinkId: number,
  message: ParsedNodeUplinkEnvelope,
  requestMetadata: Record<string, string | number | boolean | null>,
  packet: ParsedRegistrationPacket,
) {
  const observedAt = epochSecondsToDate(message.receivedAtEpochSeconds);
  const firmwareVersion = formatPackedVersion(packet.fwVersionPacked);

  const deviceResult = await client.query<{ id: number }>(
    `
      INSERT INTO devices (
        node_id,
        current_ipv6,
        current_parent_ipv6,
        last_observed_gateway_row_id,
        last_observed_gateway_at,
        current_latitude,
        current_longitude,
        current_firmware_version,
        first_registered_at,
        last_registered_at,
        last_seen_at,
        latest_battery_pct
      )
      VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $9, $9, $10)
      ON CONFLICT (node_id)
      DO UPDATE SET
        current_ipv6 = EXCLUDED.current_ipv6,
        current_parent_ipv6 = EXCLUDED.current_parent_ipv6,
        last_observed_gateway_row_id = EXCLUDED.last_observed_gateway_row_id,
        last_observed_gateway_at = EXCLUDED.last_observed_gateway_at,
        current_latitude = EXCLUDED.current_latitude,
        current_longitude = EXCLUDED.current_longitude,
        current_firmware_version = EXCLUDED.current_firmware_version,
        first_registered_at = LEAST(devices.first_registered_at, EXCLUDED.first_registered_at),
        last_registered_at = GREATEST(devices.last_registered_at, EXCLUDED.last_registered_at),
        last_seen_at = GREATEST(COALESCE(devices.last_seen_at, EXCLUDED.last_seen_at), EXCLUDED.last_seen_at),
        latest_battery_pct = EXCLUDED.latest_battery_pct
      RETURNING id
    `,
    [
      packet.nodeId,
      message.observedSrcIpv6,
      packet.parentIpv6,
      gatewayRowId,
      observedAt,
      packet.latitude,
      packet.longitude,
      firmwareVersion,
      observedAt,
      packet.batteryPct,
    ],
  );

  const deviceId = deviceResult.rows[0]?.id;
  if (!deviceId) {
    throw new Error(`Unable to upsert device for registration uplink ${message.uplinkId.toString()}`);
  }

  const insertedRegistration = await client.query<{ id: number }>(
    `
      INSERT INTO device_registrations (
        device_id,
        gateway_uplink_id,
        observed_ipv6,
        latitude,
        longitude,
        firmware_version,
        battery_pct,
        observed_at,
        raw_payload_metadata
      )
      VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9::jsonb)
      RETURNING id
    `,
    [
      deviceId,
      gatewayUplinkId,
      message.observedSrcIpv6,
      packet.latitude,
      packet.longitude,
      firmwareVersion,
      packet.batteryPct,
      observedAt,
      JSON.stringify(
        buildGatewayProjectionMetadata(message, {
          registrationParentIpv6: packet.parentIpv6,
          firmwareVersionPacked: packet.fwVersionPacked,
        }),
      ),
    ],
  );

  const registrationId = insertedRegistration.rows[0]?.id ?? null;
  await syncDeviceIpv6HistoryInDb(client, deviceId, message.observedSrcIpv6, observedAt, registrationId);
  await insertParentObservationInDb(client, {
    deviceId,
    gatewayUplinkId,
    observedParentIpv6: packet.parentIpv6,
    observedAt,
    sourceType: "registration",
    metadata: buildGatewayProjectionMetadata(message, {
      registrationParentIpv6: packet.parentIpv6,
    }),
  });

  const timeSyncTarget = await resolveRegistrationTimeSyncTargetInDb(client, {
    deviceId,
    nodeId: packet.nodeId,
    gatewayId: message.gatewayId,
    gatewayRowId,
    observedAt,
    requestMetadata,
  });

  if (timeSyncTarget) {
    await queueTimeSyncForTargetInDb(client, timeSyncTarget, {
      payloadMetadata: {
        trigger: "registration",
        gatewayUplinkId,
        registrationId,
      },
    });
  }
}

async function projectSensorUplinkInDb(
  client: PoolClient,
  gatewayRowId: number,
  gatewayUplinkId: number,
  message: ParsedNodeUplinkEnvelope,
  packet: ParsedSensorPacket,
) {
  const deviceResult = await client.query<{
    id: number;
    current_config_revision_id: number | null;
  }>(
    `
      SELECT id, current_config_revision_id
      FROM devices
      WHERE node_id = $1
      FOR UPDATE
    `,
    [packet.nodeId],
  );

  const device = deviceResult.rows[0];
  if (!device) {
    throw new BackhaulCodecError(
      "invalid_field_value",
      `Cannot project sensor uplink for unknown node ${packet.nodeId}. Registration must arrive first.`,
    );
  }

  const reportedAt = epochSecondsToDate(packet.timestampEpochSeconds);
  const observedAt = epochSecondsToDate(message.receivedAtEpochSeconds);
  const sourceType = packet.type === sensorAlertPacketType ? "critical_alert" : "periodic_report";

  const insertedReading = await client.query<{ id: number }>(
    `
      INSERT INTO sensor_readings (
        device_id,
        gateway_uplink_id,
        source_type,
        reported_at,
        risk_level,
        temperature_c,
        humidity_pct,
        voc_iaq,
        pm25_ug_m3,
        battery_pct,
        config_revision_id,
        raw_payload_metadata
      )
      VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12::jsonb)
      RETURNING id
    `,
    [
      device.id,
      gatewayUplinkId,
      sourceType,
      reportedAt,
      packet.riskLevel,
      packet.temperatureC,
      packet.humidityPct,
      packet.bvocPpmRaw,
      packet.pm25UgM3,
      packet.batteryPct,
      device.current_config_revision_id,
      JSON.stringify(
        buildGatewayProjectionMetadata(message, {
          nodeReportedAt: reportedAt.toISOString(),
          bvocPpmRaw: packet.bvocPpmRaw,
          temperatureRaw: packet.temperatureRaw,
          humidityRaw: packet.humidityRaw,
          pm25Raw: packet.pm25Raw,
        }),
      ),
    ],
  );

  const sensorReadingId = insertedReading.rows[0]?.id;
  if (!sensorReadingId) {
    throw new Error(`Unable to persist sensor reading for uplink ${message.uplinkId.toString()}`);
  }

  await client.query(
    `
      UPDATE devices
      SET
        current_ipv6 = $2,
        last_observed_gateway_row_id = $3,
        last_observed_gateway_at = $4,
        last_seen_at = GREATEST(COALESCE(devices.last_seen_at, $4), $4),
        latest_reported_at = CASE
          WHEN devices.latest_reported_at IS NULL OR devices.latest_reported_at <= $5 THEN $5
          ELSE devices.latest_reported_at
        END,
        latest_risk_level = CASE
          WHEN devices.latest_reported_at IS NULL OR devices.latest_reported_at <= $5 THEN $6
          ELSE devices.latest_risk_level
        END,
        latest_temperature_c = CASE
          WHEN devices.latest_reported_at IS NULL OR devices.latest_reported_at <= $5 THEN $7
          ELSE devices.latest_temperature_c
        END,
        latest_humidity_pct = CASE
          WHEN devices.latest_reported_at IS NULL OR devices.latest_reported_at <= $5 THEN $8
          ELSE devices.latest_humidity_pct
        END,
        latest_voc_iaq = CASE
          WHEN devices.latest_reported_at IS NULL OR devices.latest_reported_at <= $5 THEN $9
          ELSE devices.latest_voc_iaq
        END,
        latest_pm25_ug_m3 = CASE
          WHEN devices.latest_reported_at IS NULL OR devices.latest_reported_at <= $5 THEN $10
          ELSE devices.latest_pm25_ug_m3
        END,
        latest_battery_pct = CASE
          WHEN devices.latest_reported_at IS NULL OR devices.latest_reported_at <= $5 THEN $11
          ELSE devices.latest_battery_pct
        END
      WHERE id = $1
    `,
    [
      device.id,
      message.observedSrcIpv6,
      gatewayRowId,
      observedAt,
      reportedAt,
      packet.riskLevel,
      packet.temperatureC,
      packet.humidityPct,
      packet.bvocPpmRaw,
      packet.pm25UgM3,
      packet.batteryPct,
    ],
  );

  await syncDeviceIpv6HistoryInDb(client, device.id, message.observedSrcIpv6, observedAt);

  if (packet.type === sensorAlertPacketType) {
    const alertDetails = buildGatewayProjectionMetadata(message, {
      alertSource: "0x03",
      bvocPpmRaw: packet.bvocPpmRaw,
    });

    const insertedAlert = await client.query<{ id: number }>(
      `
        INSERT INTO alerts (
          device_id,
          sensor_reading_id,
          config_revision_id,
          alert_type,
          status,
          severity,
          title,
          details,
          occurred_at,
          detected_at,
          latest_event_at,
          snapshot_reported_at,
          snapshot_risk_level,
          snapshot_temperature_c,
          snapshot_humidity_pct,
          snapshot_voc_iaq,
          snapshot_pm25_ug_m3,
          snapshot_battery_pct
        )
        VALUES ($1, $2, $3, 'critical_risk', 'open', 'critical', $4, $5::jsonb, $6, NOW(), NOW(), $6, $7, $8, $9, $10, $11, $12)
        RETURNING id
      `,
      [
        device.id,
        sensorReadingId,
        device.current_config_revision_id,
        `Node ${packet.nodeId} critical alert`,
        JSON.stringify(alertDetails),
        reportedAt,
        packet.riskLevel,
        packet.temperatureC,
        packet.humidityPct,
        packet.bvocPpmRaw,
        packet.pm25UgM3,
        packet.batteryPct,
      ],
    );

    const alertId = insertedAlert.rows[0]?.id;
    if (alertId) {
      await client.query(
        `
          INSERT INTO alert_events (
            alert_id,
            event_type,
            new_status,
            event_at,
            note,
            details
          )
          VALUES ($1, 'opened', 'open', NOW(), $2, $3::jsonb)
        `,
        [alertId, "Critical-risk alert projected from uplink 0x03.", JSON.stringify(alertDetails)],
      );
    }
  }
}

async function projectParentUpdateUplinkInDb(
  client: PoolClient,
  gatewayRowId: number,
  gatewayUplinkId: number,
  message: ParsedNodeUplinkEnvelope,
  packet: ParsedParentUpdatePacket,
) {
  const deviceResult = await client.query<{ id: number }>(
    `
      SELECT id
      FROM devices
      WHERE node_id = $1
      FOR UPDATE
    `,
    [packet.nodeId],
  );

  const device = deviceResult.rows[0];
  if (!device) {
    throw new BackhaulCodecError(
      "invalid_field_value",
      `Cannot project parent-update uplink for unknown node ${packet.nodeId}. Registration must arrive first.`,
    );
  }

  const observedAt = epochSecondsToDate(message.receivedAtEpochSeconds);
  const parentObservedAt = epochSecondsToDate(packet.timestampEpochSeconds);

  await client.query(
    `
      UPDATE devices
      SET
        current_ipv6 = $2,
        current_parent_ipv6 = $3,
        last_observed_gateway_row_id = $4,
        last_observed_gateway_at = $5,
        last_seen_at = GREATEST(COALESCE(devices.last_seen_at, $5), $5)
      WHERE id = $1
    `,
    [device.id, message.observedSrcIpv6, packet.parentIpv6, gatewayRowId, observedAt],
  );

  await syncDeviceIpv6HistoryInDb(client, device.id, message.observedSrcIpv6, observedAt);
  await insertParentObservationInDb(client, {
    deviceId: device.id,
    gatewayUplinkId,
    observedParentIpv6: packet.parentIpv6,
    observedAt: parentObservedAt,
    sourceType: "parent_update",
    metadata: buildGatewayProjectionMetadata(message, {
      parentObservedAt: parentObservedAt.toISOString(),
    }),
  });
}

async function projectGatewayUplinkInDb(
  client: PoolClient,
  gatewayRowId: number,
  gatewayUplinkId: number,
  message: ParsedNodeUplinkEnvelope,
  requestMetadata: Record<string, string | number | boolean | null>,
) {
  switch (message.decodedPayload.type) {
    case registrationPacketType:
      await projectRegistrationUplinkInDb(
        client,
        gatewayRowId,
        gatewayUplinkId,
        message,
        requestMetadata,
        message.decodedPayload,
      );
      return;
    case sensorReportPacketType:
    case sensorAlertPacketType:
      await projectSensorUplinkInDb(client, gatewayRowId, gatewayUplinkId, message, message.decodedPayload);
      return;
    case parentUpdatePacketType:
      await projectParentUpdateUplinkInDb(client, gatewayRowId, gatewayUplinkId, message, message.decodedPayload);
      return;
    default:
      throw new Error(`Unsupported projected packet type ${(message.decodedPayload as { type: number }).type}`);
  }
}

async function resolveRoutedDeviceTargetsInDb(
  client: PoolClient,
  targetNodeIds?: NodeId[],
): Promise<RoutedDeviceTarget[]> {
  const rows = (
    targetNodeIds?.length
      ? await client.query<{
          device_id: number;
          node_id: number;
          gateway_row_id: number | null;
          gateway_id: number | null;
          last_observed_gateway_at: string | null;
          current_nn_revision_id: number | null;
          downlink_url: string | null;
          latest_gateway_remote_address: string | null;
        }>(
          `
            SELECT
              d.id AS device_id,
              d.node_id,
              d.last_observed_gateway_row_id AS gateway_row_id,
              g.gateway_id,
              d.last_observed_gateway_at::text,
              d.current_nn_revision_id,
              registration.downlink_url,
              registration.latest_gateway_remote_address
            FROM devices d
            LEFT JOIN gateways g ON g.id = d.last_observed_gateway_row_id
            LEFT JOIN LATERAL (
              SELECT
                raw_payload_metadata->>'downlinkUrl' AS downlink_url,
                raw_payload_metadata->>'remoteAddress' AS latest_gateway_remote_address
              FROM gateway_registrations
              WHERE gateway_row_id = d.last_observed_gateway_row_id
              ORDER BY reported_at DESC, id DESC
              LIMIT 1
            ) registration ON TRUE
            WHERE d.node_id = ANY($1::smallint[])
            ORDER BY d.node_id
          `,
          [targetNodeIds],
        )
      : await client.query<{
          device_id: number;
          node_id: number;
          gateway_row_id: number | null;
          gateway_id: number | null;
          last_observed_gateway_at: string | null;
          current_nn_revision_id: number | null;
          downlink_url: string | null;
          latest_gateway_remote_address: string | null;
        }>(
          `
            SELECT
              d.id AS device_id,
              d.node_id,
              d.last_observed_gateway_row_id AS gateway_row_id,
              g.gateway_id,
              d.last_observed_gateway_at::text,
              d.current_nn_revision_id,
              registration.downlink_url,
              registration.latest_gateway_remote_address
            FROM devices d
            LEFT JOIN gateways g ON g.id = d.last_observed_gateway_row_id
            LEFT JOIN LATERAL (
              SELECT
                raw_payload_metadata->>'downlinkUrl' AS downlink_url,
                raw_payload_metadata->>'remoteAddress' AS latest_gateway_remote_address
              FROM gateway_registrations
              WHERE gateway_row_id = d.last_observed_gateway_row_id
              ORDER BY reported_at DESC, id DESC
              LIMIT 1
            ) registration ON TRUE
            ORDER BY d.node_id
          `,
        )
  ).rows;

  if (targetNodeIds?.length) {
    const requestedNodeIds = [...new Set(targetNodeIds)].sort((left, right) => left - right);
    const foundNodeIds = rows.map((row) => row.node_id);
    const missingNodeIds = requestedNodeIds.filter((nodeId) => !foundNodeIds.includes(nodeId));
    if (missingNodeIds.length > 0) {
      throw new Error(`Unknown node ${missingNodeIds.join(", ")}`);
    }
  }

  const unroutableTargets = rows.filter((row) => {
    const downlinkUrl = normalizeGatewayDownlinkUrl(row.downlink_url) ??
      deriveGatewayDownlinkUrlFromRemoteAddress(row.latest_gateway_remote_address);
    return row.gateway_row_id === null || row.gateway_id === null || row.last_observed_gateway_at === null || !downlinkUrl;
  });
  if (unroutableTargets.length > 0) {
    const unroutableNodeList = unroutableTargets.map((row) => row.node_id).join(", ");
    throw new Error(
      `Cannot queue downlink for unroutable node(s): ${unroutableNodeList}. Each target needs a previously observed gateway uplink and a BR downlink URL.`,
    );
  }

  return rows.map((row) => ({
    deviceId: row.device_id,
    nodeId: row.node_id,
    gatewayRowId: row.gateway_row_id as number,
    gatewayId: row.gateway_id as number,
    lastObservedGatewayAt: row.last_observed_gateway_at as string,
    currentNeighborRevisionId: row.current_nn_revision_id,
    downlinkUrl:
      normalizeGatewayDownlinkUrl(row.downlink_url) ??
      deriveGatewayDownlinkUrlFromRemoteAddress(row.latest_gateway_remote_address) ??
      "",
  }));
}

async function resolveRegistrationTimeSyncTargetInDb(
  client: PoolClient,
  options: {
    deviceId: number;
    nodeId: number;
    gatewayId: number;
    gatewayRowId: number;
    observedAt: Date;
    requestMetadata: Record<string, string | number | boolean | null>;
  },
): Promise<RoutedDeviceTarget | null> {
  const latestGatewayRegistration = await client.query<{
    downlink_url: string | null;
    latest_gateway_remote_address: string | null;
  }>(
    `
      SELECT
        raw_payload_metadata->>'downlinkUrl' AS downlink_url,
        raw_payload_metadata->>'remoteAddress' AS latest_gateway_remote_address
      FROM gateway_registrations
      WHERE gateway_row_id = $1
      ORDER BY reported_at DESC, id DESC
      LIMIT 1
    `,
    [options.gatewayRowId],
  );

  const requestDownlinkUrl =
    typeof options.requestMetadata.downlinkUrl === "string" ? options.requestMetadata.downlinkUrl : null;
  const requestRemoteAddress =
    typeof options.requestMetadata.remoteAddress === "string" ? options.requestMetadata.remoteAddress : null;
  const latestRegistration = latestGatewayRegistration.rows[0];
  const downlinkUrl =
    normalizeGatewayDownlinkUrl(requestDownlinkUrl) ??
    deriveGatewayDownlinkUrlFromRemoteAddress(requestRemoteAddress) ??
    normalizeGatewayDownlinkUrl(latestRegistration?.downlink_url) ??
    deriveGatewayDownlinkUrlFromRemoteAddress(latestRegistration?.latest_gateway_remote_address);

  if (!downlinkUrl) {
    return null;
  }

  return {
    deviceId: options.deviceId,
    nodeId: options.nodeId,
    gatewayRowId: options.gatewayRowId,
    gatewayId: options.gatewayId,
    lastObservedGatewayAt: options.observedAt.toISOString(),
    currentNeighborRevisionId: null,
    downlinkUrl,
  };
}

function buildDownlinkRoutingMetadata(target: RoutedDeviceTarget) {
  return {
    routingBasis: "last_observed_gateway",
    gatewayRowId: target.gatewayRowId,
    gatewayId: target.gatewayId,
    lastObservedGatewayAt: target.lastObservedGatewayAt,
  };
}

function normalizeGatewayDownlinkUrl(candidate: string | null | undefined) {
  if (!candidate) {
    return null;
  }

  try {
    const url = new URL(candidate);
    if (url.protocol !== "http:" && url.protocol !== "https:") {
      return null;
    }
    if (url.port.length === 0 && gatewayDownlinkPort) {
      url.port = String(gatewayDownlinkPort);
    }
    return url.toString();
  } catch {
    return null;
  }
}

function normalizeRemoteAddress(remoteAddress: string | null | undefined) {
  if (!remoteAddress) {
    return null;
  }

  const trimmed = remoteAddress.trim();
  if (trimmed.length === 0) {
    return null;
  }

  return trimmed.startsWith("::ffff:") ? trimmed.slice(7) : trimmed;
}

function formatHostForUrl(host: string) {
  return host.includes(":") ? `[${host}]` : host;
}

function deriveGatewayDownlinkUrlFromRemoteAddress(remoteAddress: string | null | undefined) {
  const host = normalizeRemoteAddress(remoteAddress);
  if (!host) {
    return null;
  }

  const portSuffix = gatewayDownlinkPort ? `:${gatewayDownlinkPort}` : "";
  return `${gatewayDownlinkScheme}://${formatHostForUrl(host)}${portSuffix}${gatewayDownlinkRequestPath}`;
}

function resolveGatewayDownlinkUrlFromRequest(request: express.Request) {
  return (
    normalizeGatewayDownlinkUrl(request.get("x-pyronet-downlink-url")) ??
    deriveGatewayDownlinkUrlFromRemoteAddress(request.ip || null)
  );
}

function toEpochSeconds(value: Date) {
  return Math.floor(value.getTime() / 1000);
}

async function reserveGatewayDownlinkIdInDb(client: PoolClient) {
  const result = await client.query<{ downlink_id: string }>(
    "SELECT nextval('gateway_downlink_downlink_id_seq')::text AS downlink_id",
  );
  const downlinkId = result.rows[0]?.downlink_id;
  if (!downlinkId) {
    throw new Error("Unable to reserve gateway downlink id.");
  }
  return BigInt(downlinkId);
}

async function buildNeighborDistributionPayloadInDb(
  client: PoolClient,
  options: {
    targetNodeId: number;
    nnRevisionId: number;
  },
) {
  const result = await client.query<{
    neighbor_node_id: number;
    neighbor_ipv6: string | null;
  }>(
    `
      SELECT
        neighbor.node_id AS neighbor_node_id,
        CASE
          WHEN neighbor.current_ipv6 IS NULL THEN NULL
          ELSE host(neighbor.current_ipv6)
        END AS neighbor_ipv6
      FROM nn_revision_memberships membership
      JOIN devices neighbor ON neighbor.id = membership.neighbor_device_id
      WHERE membership.nn_revision_id = $1
      ORDER BY membership.neighbor_rank
    `,
    [options.nnRevisionId],
  );

  const missingIpv6Neighbors = result.rows.filter((row) => row.neighbor_ipv6 === null).map((row) => row.neighbor_node_id);
  if (missingIpv6Neighbors.length > 0) {
    throw new Error(
      `Cannot encode NN table update for node ${options.targetNodeId}. Missing IPv6 for neighbor node(s): ${missingIpv6Neighbors.join(", ")}`,
    );
  }

  return encodeNeighborTableUpdatePacket({
    targetNodeId: options.targetNodeId,
    neighborIpv6Addresses: result.rows.map((row) => row.neighbor_ipv6 as string),
  });
}

async function queueTimeSyncForTargetInDb(
  client: PoolClient,
  target: RoutedDeviceTarget,
  options: {
    targetTime?: Date;
    payloadMetadata?: Record<string, string | number | boolean | null>;
  } = {},
) {
  const targetTime = options.targetTime ?? new Date();
  const timeSyncPayload = encodeTimeSyncPacket({
    epochSeconds: toEpochSeconds(targetTime),
  });

  const eventInsert = await client.query<{ id: number }>(
    `
      INSERT INTO time_sync_events (device_id, target_time, status, sent_at, payload_metadata)
      VALUES ($1, $2, 'pending', NOW(), $3::jsonb)
      RETURNING id
    `,
    [
      target.deviceId,
      targetTime,
      JSON.stringify({
        ...buildDownlinkRoutingMetadata(target),
        ...(options.payloadMetadata ?? {}),
      }),
    ],
  );
  const timeSyncEventId = eventInsert.rows[0]?.id;
  if (!timeSyncEventId) {
    throw new Error(`Unable to create time sync audit row for node ${target.nodeId}`);
  }

  await insertGatewayDownlinkInDb(client, {
    target,
    commandCode: timeSyncPacketType,
    innerPayload: timeSyncPayload,
    timeSyncEventId,
  });
}

async function buildConfigUpdatePayloadInDb(client: PoolClient, configRevisionId: number) {
  const result = await client.query<{
    config_id: string;
    l2_temp_thresh: string;
    l2_humidity_thresh: string;
    l2_voc_thresh: number;
    l3_temp_thresh: string;
    l3_humidity_thresh: string;
    l3_voc_thresh: number;
    l4_voc_thresh: number;
    l5_voc_thresh: number;
    l5_pm25_thresh: string;
  }>(
    `
      SELECT
        config_id::text,
        l2_temp_thresh::text,
        l2_humidity_thresh::text,
        l2_voc_thresh,
        l3_temp_thresh::text,
        l3_humidity_thresh::text,
        l3_voc_thresh,
        l4_voc_thresh,
        l5_voc_thresh,
        l5_pm25_thresh::text
      FROM config_revisions
      WHERE id = $1
    `,
    [configRevisionId],
  );

  const revision = result.rows[0];
  if (!revision) {
    throw new Error(`Unknown config revision ${configRevisionId}`);
  }

  return encodeConfigUpdatePacket({
    configId: Number(revision.config_id),
    l2TempThresh: Number(revision.l2_temp_thresh),
    l2HumidityThresh: Number(revision.l2_humidity_thresh),
    l2VocThresh: revision.l2_voc_thresh,
    l3TempThresh: Number(revision.l3_temp_thresh),
    l3HumidityThresh: Number(revision.l3_humidity_thresh),
    l3VocThresh: revision.l3_voc_thresh,
    l4VocThresh: revision.l4_voc_thresh,
    l5VocThresh: revision.l5_voc_thresh,
    l5Pm25Thresh: Number(revision.l5_pm25_thresh),
  });
}

async function insertGatewayDownlinkInDb(
  client: PoolClient,
  options: {
    target: RoutedDeviceTarget;
    commandCode: typeof neighborTableUpdatePacketType | typeof timeSyncPacketType | typeof configUpdatePacketType;
    innerPayload: Buffer;
    nnDistributionEventId?: number;
    timeSyncEventId?: number;
    deviceConfigDeploymentId?: number;
  },
) {
  const downlinkId = await reserveGatewayDownlinkIdInDb(client);
  const createdAt = new Date();
  const requestPayload = encodeDownlinkRequestMessage({
    gatewayId: options.target.gatewayId,
    downlinkId,
    targetNodeId: options.target.nodeId,
    createdAtEpochSeconds: toEpochSeconds(createdAt),
    payload: options.innerPayload,
  });

  await client.query(
    `
      INSERT INTO gateway_downlinks (
        gateway_row_id,
        device_id,
        downlink_id,
        target_node_id,
        command_code,
        backhaul_version,
        node_packet_version,
        request_payload,
        inner_payload,
        metadata,
        nn_distribution_event_id,
        time_sync_event_id,
        device_config_deployment_id
      )
      VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10::jsonb, $11, $12, $13)
    `,
    [
      options.target.gatewayRowId,
      options.target.deviceId,
      downlinkId.toString(),
      options.target.nodeId,
      options.commandCode,
      CURRENT_BACKHAUL_VERSION,
      CURRENT_NODE_PACKET_VERSION,
      requestPayload,
      options.innerPayload,
      JSON.stringify({
        ...buildDownlinkRoutingMetadata(options.target),
        downlinkUrl: options.target.downlinkUrl,
        downlinkId: downlinkId.toString(),
      }),
      options.nnDistributionEventId ?? null,
      options.timeSyncEventId ?? null,
      options.deviceConfigDeploymentId ?? null,
    ],
  );
}

async function persistGatewayRegistrationInDb(
  message: ReturnType<typeof parseGatewayRegistrationMessage>,
  requestMetadata: Record<string, string | number | boolean | null>,
) {
  if (!pool) {
    throw new Error("Database is not configured.");
  }

  const reportedAt = epochSecondsToDate(message.timestampEpochSeconds);
  const client = await pool.connect();

  try {
    await client.query("BEGIN");

    const gatewayResult = await client.query<{ id: number }>(
      `
        INSERT INTO gateways (
          gateway_id,
          current_ipv6,
          current_latitude,
          current_longitude,
          current_sw_version_packed,
          first_registered_at,
          last_registered_at,
          last_gateway_timestamp_at
        )
        VALUES ($1, $2, $3, $4, $5, $6, $6, $6)
        ON CONFLICT (gateway_id)
        DO UPDATE SET
          current_ipv6 = CASE
            WHEN gateways.last_gateway_timestamp_at IS NULL OR EXCLUDED.last_gateway_timestamp_at >= gateways.last_gateway_timestamp_at
              THEN EXCLUDED.current_ipv6
            ELSE gateways.current_ipv6
          END,
          current_latitude = CASE
            WHEN gateways.last_gateway_timestamp_at IS NULL OR EXCLUDED.last_gateway_timestamp_at >= gateways.last_gateway_timestamp_at
              THEN EXCLUDED.current_latitude
            ELSE gateways.current_latitude
          END,
          current_longitude = CASE
            WHEN gateways.last_gateway_timestamp_at IS NULL OR EXCLUDED.last_gateway_timestamp_at >= gateways.last_gateway_timestamp_at
              THEN EXCLUDED.current_longitude
            ELSE gateways.current_longitude
          END,
          current_sw_version_packed = CASE
            WHEN gateways.last_gateway_timestamp_at IS NULL OR EXCLUDED.last_gateway_timestamp_at >= gateways.last_gateway_timestamp_at
              THEN EXCLUDED.current_sw_version_packed
            ELSE gateways.current_sw_version_packed
          END,
          first_registered_at = CASE
            WHEN gateways.first_registered_at IS NULL THEN EXCLUDED.first_registered_at
            ELSE LEAST(gateways.first_registered_at, EXCLUDED.first_registered_at)
          END,
          last_registered_at = CASE
            WHEN gateways.last_registered_at IS NULL THEN EXCLUDED.last_registered_at
            ELSE GREATEST(gateways.last_registered_at, EXCLUDED.last_registered_at)
          END,
          last_gateway_timestamp_at = GREATEST(
            COALESCE(gateways.last_gateway_timestamp_at, EXCLUDED.last_gateway_timestamp_at),
            EXCLUDED.last_gateway_timestamp_at
          )
        RETURNING id
      `,
      [
        message.gatewayId,
        message.wisunIpv6,
        message.latitude,
        message.longitude,
        message.swVersionPacked,
        reportedAt,
      ],
    );

    const gatewayRowId = gatewayResult.rows[0]?.id;
    if (!gatewayRowId) {
      throw new Error(`Unable to persist gateway ${message.gatewayId}`);
    }

    await client.query(
      `
        INSERT INTO gateway_registrations (
          gateway_row_id,
          reported_at,
          wisun_ipv6,
          latitude,
          longitude,
          sw_version_packed,
          backhaul_version,
          raw_message,
          raw_payload_metadata
        )
        VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9::jsonb)
      `,
      [
        gatewayRowId,
        reportedAt,
        message.wisunIpv6,
        message.latitude,
        message.longitude,
        message.swVersionPacked,
        message.version,
        message.rawMessage,
        JSON.stringify(requestMetadata),
      ],
    );

    await client.query("COMMIT");
  } catch (error) {
    await client.query("ROLLBACK");
    throw error;
  } finally {
    client.release();
  }
}

function describeDownlinkResultStatus(status: number) {
  switch (status) {
    case deliveredDownlinkResultStatus:
      return "Delivered";
    case unknownNodeDownlinkResultStatus:
      return "Unknown Node";
    case meshDeliveryFailedDownlinkResultStatus:
      return "Mesh Delivery Failed";
    case permanentRejectDownlinkResultStatus:
      return "Permanent Reject";
    default:
      return `Status ${status}`;
  }
}

function mapGatewayResultToTransportStatus(
  status: number,
): "delivered" | "unknown_node" | "mesh_delivery_failed" | "permanent_reject" {
  switch (status) {
    case deliveredDownlinkResultStatus:
      return "delivered";
    case unknownNodeDownlinkResultStatus:
      return "unknown_node";
    case meshDeliveryFailedDownlinkResultStatus:
      return "mesh_delivery_failed";
    case permanentRejectDownlinkResultStatus:
      return "permanent_reject";
    default:
      throw new Error(`Unsupported gateway downlink result status ${status}`);
  }
}

function mapGatewayResultToDomainStatus(status: number): "sent" | "failed" {
  return status === deliveredDownlinkResultStatus ? "sent" : "failed";
}

async function claimPendingGatewayDownlinksInDb(limit: number) {
  if (!pool) {
    return [];
  }

  const client = await pool.connect();
  try {
    await client.query("BEGIN");
    const claimed = await client.query<{
      id: number;
      downlink_id: string;
      target_node_id: number;
      request_payload: Buffer;
      metadata: Record<string, unknown>;
      nn_distribution_event_id: number | null;
      time_sync_event_id: number | null;
      device_config_deployment_id: number | null;
    }>(
      `
        WITH claimed AS (
          SELECT id
          FROM gateway_downlinks
          WHERE status IN ('pending', 'transport_failed')
            AND next_attempt_at <= NOW()
          ORDER BY next_attempt_at, queued_at
          LIMIT $1
          FOR UPDATE SKIP LOCKED
        )
        UPDATE gateway_downlinks downlinks
        SET
          status = 'dispatched',
          attempt_count = downlinks.attempt_count + 1,
          last_attempt_at = NOW(),
          dispatched_at = COALESCE(downlinks.dispatched_at, NOW()),
          last_error = NULL
        FROM claimed
        WHERE downlinks.id = claimed.id
        RETURNING
          downlinks.id,
          downlinks.downlink_id::text,
          downlinks.target_node_id,
          downlinks.request_payload,
          downlinks.metadata,
          downlinks.nn_distribution_event_id,
          downlinks.time_sync_event_id,
          downlinks.device_config_deployment_id
      `,
      [limit],
    );
    await client.query("COMMIT");
    return claimed.rows;
  } catch (error) {
    await client.query("ROLLBACK");
    throw error;
  } finally {
    client.release();
  }
}

async function markGatewayDownlinkTransportFailureInDb(downlinkRowId: number, reason: string) {
  await query(
    `
      UPDATE gateway_downlinks
      SET
        status = 'transport_failed',
        next_attempt_at = NOW() + ($2 * interval '1 millisecond'),
        last_error = $3
      WHERE id = $1
    `,
    [downlinkRowId, gatewayDownlinkRetryDelayMs, reason],
  );
}

async function applyGatewayDownlinkTerminalResultInDb(
  downlink: {
    id: number;
    target_node_id: number;
    nn_distribution_event_id: number | null;
    time_sync_event_id: number | null;
    device_config_deployment_id: number | null;
  },
  gatewayId: number,
  result: ReturnType<typeof parseDownlinkResultMessage>,
  resultPayload: Buffer,
) {
  if (!pool) {
    throw new Error("Database is not configured.");
  }

  const client = await pool.connect();
  const completedAt = epochSecondsToDate(result.completedAtEpochSeconds);
  const transportStatus = mapGatewayResultToTransportStatus(result.status);
  const domainStatus = mapGatewayResultToDomainStatus(result.status);
  const transportMetadata = JSON.stringify({
    gatewayResultStatus: result.status,
    gatewayResultLabel: describeDownlinkResultStatus(result.status),
    transportStatus,
    transportCompletedAt: completedAt.toISOString(),
    gatewayId,
  });
  const resultMessage =
    result.status === deliveredDownlinkResultStatus
      ? `BR ${gatewayId} delivered downlink to node ${downlink.target_node_id}.`
      : `BR ${gatewayId} returned ${describeDownlinkResultStatus(result.status)} for node ${downlink.target_node_id}.`;

  try {
    await client.query("BEGIN");
    await client.query(
      `
        UPDATE gateway_downlinks
        SET
          status = $2,
          completed_at = $3,
          result_status = $4,
          result_payload = $5,
          next_attempt_at = $3,
          last_error = NULL
        WHERE id = $1
      `,
      [downlink.id, transportStatus, completedAt, result.status, resultPayload],
    );

    if (downlink.nn_distribution_event_id !== null) {
      await client.query(
        `
          UPDATE nn_distribution_events
          SET
            status = $2,
            payload_metadata = payload_metadata || $3::jsonb
          WHERE id = $1
        `,
        [downlink.nn_distribution_event_id, domainStatus, transportMetadata],
      );
    }

    if (downlink.time_sync_event_id !== null) {
      await client.query(
        `
          UPDATE time_sync_events
          SET
            status = $2,
            result_message = $3,
            payload_metadata = payload_metadata || $4::jsonb
          WHERE id = $1
        `,
        [downlink.time_sync_event_id, domainStatus, resultMessage, transportMetadata],
      );
    }

    if (downlink.device_config_deployment_id !== null) {
      await client.query(
        `
          UPDATE device_config_deployments
          SET
            status = $2,
            payload_metadata = payload_metadata || $3::jsonb
          WHERE id = $1
        `,
        [downlink.device_config_deployment_id, domainStatus, transportMetadata],
      );
    }

    await client.query("COMMIT");
  } catch (error) {
    await client.query("ROLLBACK");
    throw error;
  } finally {
    client.release();
  }
}

async function dispatchGatewayDownlink(
  downlink: Awaited<ReturnType<typeof claimPendingGatewayDownlinksInDb>>[number],
) {
  const metadata = downlink.metadata ?? {};
  const downlinkUrl = typeof metadata.downlinkUrl === "string" ? normalizeGatewayDownlinkUrl(metadata.downlinkUrl) : null;
  const gatewayId = typeof metadata.gatewayId === "number" ? metadata.gatewayId : Number(metadata.gatewayId);

  if (!downlinkUrl || !Number.isInteger(gatewayId)) {
    await markGatewayDownlinkTransportFailureInDb(
      downlink.id,
      "Missing BR downlink URL or gateway identifier on queued downlink.",
    );
    return;
  }

  const abortController = new AbortController();
  const timeout = setTimeout(() => abortController.abort(), gatewayDownlinkTimeoutMs);

  try {
    const response = await fetch(downlinkUrl, {
      method: "POST",
      headers: {
        "Content-Type": "application/octet-stream",
      },
      body: downlink.request_payload,
      signal: abortController.signal,
    });

    if (response.status < 200 || response.status >= 300) {
      await markGatewayDownlinkTransportFailureInDb(
        downlink.id,
        `BR returned HTTP ${response.status} for gateway downlink ${downlink.downlink_id}.`,
      );
      return;
    }

    const responseBody = Buffer.from(await response.arrayBuffer());
    const result = parseDownlinkResultMessage(responseBody);
    if (result.gatewayId !== gatewayId) {
      throw new Error(`Gateway result mismatch. Expected gateway ${gatewayId}, got ${result.gatewayId}.`);
    }
    if (result.downlinkId.toString() !== downlink.downlink_id) {
      throw new Error(`Downlink result mismatch. Expected ${downlink.downlink_id}, got ${result.downlinkId.toString()}.`);
    }
    if (result.targetNodeId !== downlink.target_node_id) {
      throw new Error(`Target node mismatch. Expected ${downlink.target_node_id}, got ${result.targetNodeId}.`);
    }

    await applyGatewayDownlinkTerminalResultInDb(downlink, gatewayId, result, responseBody);
  } catch (error) {
    const message = error instanceof Error ? error.message : "Unknown BR downlink transport error";
    await markGatewayDownlinkTransportFailureInDb(downlink.id, message);
  } finally {
    clearTimeout(timeout);
  }
}

async function dispatchPendingGatewayDownlinks() {
  while (true) {
    const claimed = await claimPendingGatewayDownlinksInDb(gatewayDownlinkDispatchBatchSize);
    if (claimed.length === 0) {
      return;
    }

    for (const downlink of claimed) {
      await dispatchGatewayDownlink(downlink);
    }

    if (claimed.length < gatewayDownlinkDispatchBatchSize) {
      return;
    }
  }
}

function startGatewayDownlinkDispatcher() {
  if (!pool) {
    return;
  }

  const runDispatch = () => {
    if (isGatewayDownlinkDispatchInFlight) {
      return;
    }

    isGatewayDownlinkDispatchInFlight = true;
    void dispatchPendingGatewayDownlinks().finally(() => {
      isGatewayDownlinkDispatchInFlight = false;
    });
  };

  runDispatch();
  const timer = setInterval(runDispatch, gatewayDownlinkDispatchIntervalMs);
  timer.unref?.();
}

async function ensureGatewayRowForUplink(
  client: PoolClient,
  gatewayId: number,
) {
  const gatewayResult = await client.query<{ id: number }>(
    `
      INSERT INTO gateways (gateway_id)
      VALUES ($1)
      ON CONFLICT (gateway_id)
      DO UPDATE SET gateway_id = EXCLUDED.gateway_id
      RETURNING id
    `,
    [gatewayId],
  );

  const gatewayRowId = gatewayResult.rows[0]?.id;
  if (!gatewayRowId) {
    throw new Error(`Unable to ensure gateway row for gateway ${gatewayId}`);
  }

  return gatewayRowId;
}

type PersistedUplinkReceipt = {
  receiptBuffer: Buffer;
  isDuplicate: boolean;
  receiptStatus: "durable_ingest";
  rejectCode: null;
};

function createDurableIngestReceiptBuffer(gatewayId: number, uplinkId: bigint, version: number) {
  return encodeUplinkReceipt({
    gatewayId,
    uplinkId,
    status: durableIngestReceiptStatus,
    version,
  });
}

function normalizeAcceptedErrorCode(error: BackhaulCodecError): StableRejectCode {
  return error.code;
}

async function createStoredAcceptedUplinkReceiptInDb(
  client: PoolClient,
  options: {
    gatewayUplinkId: number;
    gatewayId: number;
    uplinkId: bigint;
    version: number;
    acceptedErrorCode: StableRejectCode;
    acceptedErrorDetail: string;
    reason: "accepted_despite_parse_or_validation_failure";
  },
) {
  const receiptBuffer = createDurableIngestReceiptBuffer(options.gatewayId, options.uplinkId, options.version);

  await client.query(
    `
      UPDATE gateway_uplinks
      SET
        metadata = gateway_uplinks.metadata || $2::jsonb
      WHERE id = $1
    `,
    [
      options.gatewayUplinkId,
      JSON.stringify({
        acceptedDespiteErrorCode: options.acceptedErrorCode,
        acceptedDespiteErrorReason: options.reason,
      }),
    ],
  );

  await client.query(
    `
      INSERT INTO gateway_uplink_receipts (
        gateway_uplink_id,
        status,
        receipt_version,
        receipt_payload,
        metadata
      )
      VALUES ($1, 'durable_ingest', $2, $3, $4::jsonb)
    `,
    [
      options.gatewayUplinkId,
      options.version,
      receiptBuffer,
      JSON.stringify({
        reason: options.reason,
        acceptedErrorCode: options.acceptedErrorCode,
        acceptedErrorDetail: options.acceptedErrorDetail,
      }),
    ],
  );

  return receiptBuffer;
}

async function persistGatewayUplinkInDb(
  message: ReturnType<typeof parseNodeUplinkEnvelopeMessage>,
  requestMetadata: Record<string, string | number | boolean | null>,
): Promise<PersistedUplinkReceipt> {
  if (!pool) {
    throw new Error("Database is not configured.");
  }

  const client = await pool.connect();

  try {
    await client.query("BEGIN");

    const gatewayRowId = await ensureGatewayRowForUplink(client, message.gatewayId);
    const uplinkIdText = message.uplinkId.toString();

    const existingReceiptResult = await client.query<{
      receipt_payload: Buffer | null;
      status: "durable_ingest" | "permanent_reject";
      reject_code: StableRejectCode | null;
    }>(
      `
        SELECT receipt.receipt_payload, receipt.status, receipt.reject_code
        FROM gateway_uplinks uplink
        JOIN gateway_uplink_receipts receipt ON receipt.gateway_uplink_id = uplink.id
        WHERE uplink.gateway_row_id = $1
          AND uplink.uplink_id = $2::numeric(20,0)
        FOR UPDATE
      `,
      [gatewayRowId, uplinkIdText],
    );

    const existingReceipt = existingReceiptResult.rows[0];
    if (existingReceipt?.receipt_payload) {
      const receiptBuffer =
        existingReceipt.status === "durable_ingest"
          ? existingReceipt.receipt_payload
          : createDurableIngestReceiptBuffer(message.gatewayId, message.uplinkId, message.version);

      if (existingReceipt.status !== "durable_ingest") {
        await client.query(
          `
            UPDATE gateway_uplink_receipts
            SET
              status = 'durable_ingest',
              receipt_version = $2,
              receipt_payload = $3,
              reject_code = NULL,
              reject_detail = NULL,
              metadata = gateway_uplink_receipts.metadata || $4::jsonb
            WHERE gateway_uplink_id = (
              SELECT id
              FROM gateway_uplinks
              WHERE gateway_row_id = $1
                AND uplink_id = $5::numeric(20,0)
            )
          `,
          [
            gatewayRowId,
            message.version,
            receiptBuffer,
            JSON.stringify({
              reason: "accepted_duplicate_of_previous_reject",
            }),
            uplinkIdText,
          ],
        );
      }

      await client.query(
        `
          UPDATE gateway_uplinks
          SET
            last_csp_received_at = NOW(),
            delivery_attempt_count = delivery_attempt_count + 1,
            metadata = gateway_uplinks.metadata || $3::jsonb
          WHERE gateway_row_id = $1
            AND uplink_id = $2::numeric(20,0)
        `,
        [
          gatewayRowId,
          uplinkIdText,
          JSON.stringify({
            lastDuplicateHttpContentLength: requestMetadata.httpContentLength ?? null,
            lastDuplicateRemoteAddress: requestMetadata.remoteAddress ?? null,
          }),
        ],
      );

      await client.query("COMMIT");
      return {
        receiptBuffer,
        isDuplicate: true,
        receiptStatus: "durable_ingest",
        rejectCode: null,
      };
    }

    const insertedUplink = await client.query<{ id: number }>(
      `
        INSERT INTO gateway_uplinks (
          gateway_row_id,
          uplink_id,
          received_at,
          observed_src_ipv6,
          backhaul_version,
          payload_len,
          payload_type,
          payload_version,
          payload,
          raw_envelope,
          storage_status,
          metadata
        )
        VALUES ($1, $2::numeric(20,0), $3, $4, $5, $6, $7, $8, $9, $10, 'received', $11::jsonb)
        RETURNING id
      `,
      [
        gatewayRowId,
        uplinkIdText,
        epochSecondsToDate(message.receivedAtEpochSeconds),
        message.observedSrcIpv6,
        message.version,
        message.payloadLength,
        message.decodedPayload.type,
        message.decodedPayload.version,
        message.payload,
        message.rawEnvelope,
        JSON.stringify(requestMetadata),
      ],
    );

    const gatewayUplinkId = insertedUplink.rows[0]?.id;
    if (!gatewayUplinkId) {
      throw new Error(`Unable to persist uplink ${uplinkIdText} for gateway ${message.gatewayId}`);
    }

    await projectGatewayUplinkInDb(client, gatewayRowId, gatewayUplinkId, message, requestMetadata);
    await client.query(
      `
        UPDATE gateway_uplinks
        SET
          storage_status = 'projected',
          metadata = gateway_uplinks.metadata || $2::jsonb
        WHERE id = $1
      `,
      [
        gatewayUplinkId,
        JSON.stringify({
          projectedAt: new Date().toISOString(),
        }),
      ],
    );

    const receiptBuffer = encodeUplinkReceipt({
      gatewayId: message.gatewayId,
      uplinkId: message.uplinkId,
      status: durableIngestReceiptStatus,
    });

    await client.query(
      `
        INSERT INTO gateway_uplink_receipts (
          gateway_uplink_id,
          status,
          receipt_version,
          receipt_payload,
          metadata
        )
        VALUES ($1, 'durable_ingest', $2, $3, $4::jsonb)
      `,
      [
        gatewayUplinkId,
        message.version,
        receiptBuffer,
        JSON.stringify({
          reason: "raw_envelope_projected",
        }),
      ],
    );

    await client.query("COMMIT");
    return {
      receiptBuffer,
      isDuplicate: false,
      receiptStatus: "durable_ingest",
      rejectCode: null,
    };
  } catch (error) {
    await client.query("ROLLBACK");
    throw error;
  } finally {
    client.release();
  }
}

async function persistAcceptedUplinkDespiteErrorInDb(
  rawEnvelope: Buffer,
  codecError: BackhaulCodecError,
  requestMetadata: Record<string, string | number | boolean | null>,
): Promise<PersistedUplinkReceipt> {
  if (!pool) {
    throw new Error("Database is not configured.");
  }

  const type = rawEnvelope.length >= 1 ? rawEnvelope.readUInt8(0) : null;
  const version = rawEnvelope.length >= 2 ? rawEnvelope.readUInt8(1) : null;
  const gatewayId = rawEnvelope.length >= 4 ? rawEnvelope.readUInt16LE(2) : null;
  const uplinkId = rawEnvelope.length >= 12 ? rawEnvelope.readBigUInt64LE(4) : null;
  const acceptedErrorCode = normalizeAcceptedErrorCode(codecError);
  const receiptGatewayId = gatewayId ?? 0;
  const receiptUplinkId = uplinkId ?? 0n;
  const receiptBuffer = createDurableIngestReceiptBuffer(receiptGatewayId, receiptUplinkId, version ?? 1);

  if (gatewayId === null || uplinkId === null) {
    return {
      receiptBuffer,
      isDuplicate: false,
      receiptStatus: "durable_ingest" as const,
      rejectCode: null,
    };
  }

  const client = await pool.connect();
  try {
    await client.query("BEGIN");

    const gatewayRowId = await ensureGatewayRowForUplink(client, gatewayId);
    const uplinkIdText = uplinkId.toString();

    const existingReceiptResult = await client.query<{
      receipt_payload: Buffer | null;
      status: "durable_ingest" | "permanent_reject";
      reject_code: StableRejectCode | null;
    }>(
      `
        SELECT receipt.receipt_payload, receipt.status, receipt.reject_code
        FROM gateway_uplinks uplink
        JOIN gateway_uplink_receipts receipt ON receipt.gateway_uplink_id = uplink.id
        WHERE uplink.gateway_row_id = $1
          AND uplink.uplink_id = $2::numeric(20,0)
        FOR UPDATE
      `,
      [gatewayRowId, uplinkIdText],
    );

    const existingReceipt = existingReceiptResult.rows[0];
    if (existingReceipt?.receipt_payload) {
      const duplicateReceiptBuffer =
        existingReceipt.status === "durable_ingest"
          ? existingReceipt.receipt_payload
          : createDurableIngestReceiptBuffer(gatewayId, uplinkId, version ?? 1);

      if (existingReceipt.status !== "durable_ingest") {
        await client.query(
          `
            UPDATE gateway_uplink_receipts
            SET
              status = 'durable_ingest',
              receipt_version = $2,
              receipt_payload = $3,
              reject_code = NULL,
              reject_detail = NULL,
              metadata = gateway_uplink_receipts.metadata || $4::jsonb
            WHERE gateway_uplink_id = (
              SELECT id
              FROM gateway_uplinks
              WHERE gateway_row_id = $1
                AND uplink_id = $5::numeric(20,0)
            )
          `,
          [
            gatewayRowId,
            version ?? 1,
            duplicateReceiptBuffer,
            JSON.stringify({
              reason: "accepted_duplicate_of_previous_reject",
            }),
            uplinkIdText,
          ],
        );
      }

      await client.query(
        `
          UPDATE gateway_uplinks
          SET
            last_csp_received_at = NOW(),
            delivery_attempt_count = delivery_attempt_count + 1
          WHERE gateway_row_id = $1
            AND uplink_id = $2::numeric(20,0)
        `,
        [gatewayRowId, uplinkIdText],
      );

      await client.query("COMMIT");
      return {
        receiptBuffer: duplicateReceiptBuffer,
        isDuplicate: true,
        receiptStatus: "durable_ingest",
        rejectCode: null,
      };
    }

    const insertedUplink = await client.query<{ id: number }>(
      `
        INSERT INTO gateway_uplinks (
          gateway_row_id,
          uplink_id,
          backhaul_version,
          payload_len,
          raw_envelope,
          storage_status,
          metadata
        )
        VALUES ($1, $2::numeric(20,0), $3, $4, $5, 'rejected', $6::jsonb)
        RETURNING id
      `,
      [
        gatewayRowId,
        uplinkIdText,
        version ?? 1,
        Math.max(rawEnvelope.length - 34, 0),
        rawEnvelope,
        JSON.stringify({
          ...requestMetadata,
          acceptedDespiteErrorCode: acceptedErrorCode,
          acceptedDespiteErrorMessage: codecError.message,
        }),
      ],
    );

    const gatewayUplinkId = insertedUplink.rows[0]?.id;
    if (!gatewayUplinkId) {
      throw new Error(`Unable to persist rejected uplink ${uplinkIdText} for gateway ${gatewayId}`);
    }

    await createStoredAcceptedUplinkReceiptInDb(client, {
      gatewayUplinkId,
      gatewayId,
      uplinkId,
      version: version ?? 1,
      acceptedErrorCode,
      acceptedErrorDetail: codecError.message,
      reason: "accepted_despite_parse_or_validation_failure",
    });

    await client.query("COMMIT");
    return {
      receiptBuffer,
      isDuplicate: false,
      receiptStatus: "durable_ingest" as const,
      rejectCode: null,
    };
  } catch (error) {
    await client.query("ROLLBACK");
    throw error;
  } finally {
    client.release();
  }
}

async function queryNodesFromDb(): Promise<NodeSummary[]> {
  const result = await query<{
    id: number;
    node_id: number;
    current_ipv6: string | null;
    current_parent_ipv6: string | null;
    current_latitude: string;
    current_longitude: string;
    current_firmware_version: string | null;
    first_registered_at: string;
    last_registered_at: string;
    last_seen_at: string | null;
    latest_reported_at: string | null;
    latest_risk_level: number | null;
    latest_temperature_c: string | null;
    latest_humidity_pct: string | null;
    latest_voc_iaq: number | null;
    latest_pm25_ug_m3: string | null;
    latest_battery_pct: number | null;
    latest_pressure_hpa: string | null;
    latest_battery_health_score: number | null;
    current_config_revision_id: number | null;
    current_config_revision_no: number | null;
    current_nn_revision_id: number | null;
    current_nn_revision_no: number | null;
  }>(
    `
      SELECT
        d.id,
        d.node_id,
        host(d.current_ipv6) AS current_ipv6,
        CASE
          WHEN d.current_parent_ipv6 IS NULL THEN NULL
          ELSE host(d.current_parent_ipv6)
        END AS current_parent_ipv6,
        d.current_latitude::text,
        d.current_longitude::text,
        d.current_firmware_version,
        d.first_registered_at::text,
        d.last_registered_at::text,
        d.last_seen_at::text,
        d.latest_reported_at::text,
        d.latest_risk_level,
        d.latest_temperature_c::text,
        d.latest_humidity_pct::text,
        d.latest_voc_iaq,
        d.latest_pm25_ug_m3::text,
        d.latest_battery_pct,
        d.latest_pressure_hpa::text,
        d.latest_battery_health_score,
        d.current_config_revision_id,
        cr.config_id AS current_config_revision_no,
        d.current_nn_revision_id,
        nn.revision_no AS current_nn_revision_no
      FROM devices d
      LEFT JOIN config_revisions cr ON cr.id = d.current_config_revision_id
      LEFT JOIN nn_revisions nn ON nn.id = d.current_nn_revision_id
      ORDER BY d.node_id ASC
    `,
  );

  return result.rows.map((row: (typeof result.rows)[number]) => {
    const latestTelemetry: TelemetrySnapshot | null =
      row.latest_reported_at && row.latest_risk_level !== null
        ? {
            reportedAt: row.latest_reported_at,
            sourceType: "periodic_report",
            riskLevel: row.latest_risk_level,
            temperatureC: Number(row.latest_temperature_c ?? 0),
            humidityPct: Number(row.latest_humidity_pct ?? 0),
            vocIaq: row.latest_voc_iaq ?? 0,
            pm25UgM3: Number(row.latest_pm25_ug_m3 ?? 0),
            batteryPct: row.latest_battery_pct,
            pressureHpa: row.latest_pressure_hpa ? Number(row.latest_pressure_hpa) : null,
            batteryHealthScore: row.latest_battery_health_score,
          }
        : null;

    return {
      id: String(row.id),
      nodeId: row.node_id,
      ipv6Address: row.current_ipv6,
      currentParentIpv6: row.current_parent_ipv6,
      connectivity: connectivityFromLastSeen(row.last_seen_at),
      location: {
        lat: Number(row.current_latitude),
        lng: Number(row.current_longitude),
        label: formatCoordinatePair(Number(row.current_latitude), Number(row.current_longitude)),
      },
      firmwareVersion: row.current_firmware_version,
      firstRegisteredAt: row.first_registered_at,
      lastRegisteredAt: row.last_registered_at,
      lastSeenAt: row.last_seen_at,
      lastReportedAt: row.latest_reported_at,
      currentRiskLevel: row.latest_risk_level,
      activeConfigRevisionId: row.current_config_revision_id,
      activeConfigRevisionNo: row.current_config_revision_no,
      currentNeighborRevisionId: row.current_nn_revision_id,
      currentNeighborRevisionNo: row.current_nn_revision_no,
      latestTelemetry,
    };
  });
}

async function queryDownlinksFromDb(): Promise<DashboardResponse["downlinks"]> {
  const result = await query<{
    id: string;
    command_code: "0x04" | "0x05" | "0x06";
    command_name: string;
    node_id: number;
    node_name: number;
    status: DashboardResponse["downlinks"][number]["status"];
    sent_at: string;
    acknowledged_at: string | null;
    revision_no: number | null;
    summary: string;
  }>(
    `
      SELECT
        concat('nn-', nde.id) AS id,
        '0x04' AS command_code,
        'Neighbor table distribution' AS command_name,
        d.node_id,
        d.node_id AS node_name,
        nde.status::text AS status,
        nde.sent_at::text,
        nde.acknowledged_at::text,
        nn.revision_no,
        concat('NN revision ', nn.revision_no, ' delivered to ', d.node_id) AS summary
      FROM nn_distribution_events nde
      JOIN devices d ON d.id = nde.device_id
      JOIN nn_revisions nn ON nn.id = nde.nn_revision_id
      UNION ALL
      SELECT
        concat('ts-', tse.id) AS id,
        '0x05' AS command_code,
        CASE
          WHEN tse.payload_metadata->>'trigger' = 'registration' THEN 'Registration time synchronization'
          ELSE 'Daily time synchronization'
        END AS command_name,
        d.node_id,
        d.node_id AS node_name,
        tse.status::text AS status,
        tse.sent_at::text,
        tse.acknowledged_at::text,
        NULL AS revision_no,
        COALESCE(
          tse.result_message,
          CASE
            WHEN tse.payload_metadata->>'trigger' = 'registration'
              THEN concat('Time sync queued after registration for ', d.node_id)
            ELSE concat('Time sync sent to ', d.node_id)
          END
        ) AS summary
      FROM time_sync_events tse
      JOIN devices d ON d.id = tse.device_id
      UNION ALL
      SELECT
        concat('cfg-', dcd.id) AS id,
        '0x06' AS command_code,
        'Threshold revision deployment' AS command_name,
        d.node_id,
        d.node_id AS node_name,
        dcd.status::text AS status,
        dcd.sent_at::text,
        dcd.acknowledged_at::text,
        cr.config_id AS revision_no,
        concat('Config revision ', cr.config_id, ' queued for ', d.node_id) AS summary
      FROM device_config_deployments dcd
      JOIN devices d ON d.id = dcd.device_id
      JOIN config_revisions cr ON cr.id = dcd.config_revision_id
      ORDER BY sent_at DESC
      LIMIT 5
    `,
  );

  return result.rows.map((row: (typeof result.rows)[number]) => ({
    id: row.id,
    commandCode: row.command_code,
    commandName: row.command_name,
    nodeId: row.node_id,
    nodeName: String(row.node_name),
    status: row.status,
    sentAt: row.sent_at,
    acknowledgedAt: row.acknowledged_at,
    revisionNo: row.revision_no,
    summary: row.summary,
  }));
}

async function queryGatewaysFromDb(): Promise<GatewayMarker[]> {
  const result = await query<{
    gateway_id: number;
    current_ipv6: string | null;
    current_latitude: string;
    current_longitude: string;
    current_sw_version_packed: number | null;
    last_registered_at: string | null;
  }>(
    `
      SELECT
        gateway_id,
        CASE
          WHEN current_ipv6 IS NULL THEN NULL
          ELSE host(current_ipv6)
        END AS current_ipv6,
        current_latitude::text,
        current_longitude::text,
        current_sw_version_packed,
        last_registered_at::text
      FROM gateways
      WHERE current_latitude IS NOT NULL
        AND current_longitude IS NOT NULL
      ORDER BY gateway_id
    `,
  );

  return result.rows.map((row: (typeof result.rows)[number]) => ({
    id: `gateway-${row.gateway_id}`,
    gatewayId: row.gateway_id,
    ipv6Address: row.current_ipv6,
    location: {
      lat: Number(row.current_latitude),
      lng: Number(row.current_longitude),
      label: formatCoordinatePair(Number(row.current_latitude), Number(row.current_longitude)),
    },
    lastRegisteredAt: row.last_registered_at,
    softwareVersion:
      row.current_sw_version_packed === null ? null : formatPackedVersion(row.current_sw_version_packed),
  }));
}

async function getBorderRouterDetailFromDb(gatewayId: number): Promise<BorderRouterDetail> {
  const gatewayResult = await query<{
    gateway_id: number;
    current_ipv6: string | null;
    current_latitude: string;
    current_longitude: string;
    current_sw_version_packed: number | null;
    first_registered_at: string | null;
    last_registered_at: string | null;
    registration_count: string;
  }>(
    `
      SELECT
        gateway_id,
        CASE
          WHEN current_ipv6 IS NULL THEN NULL
          ELSE host(current_ipv6)
        END AS current_ipv6,
        current_latitude::text,
        current_longitude::text,
        current_sw_version_packed,
        first_registered_at::text,
        last_registered_at::text,
        (
          SELECT count(*)::text
          FROM gateway_registrations gr
          WHERE gr.gateway_row_id = gateways.id
        ) AS registration_count
      FROM gateways
      WHERE gateway_id = $1
        AND current_latitude IS NOT NULL
        AND current_longitude IS NOT NULL
    `,
    [gatewayId],
  );

  const gatewayRow = gatewayResult.rows[0];
  if (!gatewayRow) {
    throw new Error(`Unknown gateway ${gatewayId}`);
  }

  const registrationResult = await query<{
    reported_at: string;
    latitude: string;
    longitude: string;
    sw_version_packed: number;
    backhaul_version: number;
  }>(
    `
      SELECT
        gr.reported_at::text,
        gr.latitude::text,
        gr.longitude::text,
        gr.sw_version_packed,
        gr.backhaul_version
      FROM gateway_registrations gr
      JOIN gateways g ON g.id = gr.gateway_row_id
      WHERE g.gateway_id = $1
      ORDER BY gr.reported_at DESC
      LIMIT 50
    `,
    [gatewayId],
  );

  return {
    gateway: {
      id: `gateway-${gatewayRow.gateway_id}`,
      gatewayId: gatewayRow.gateway_id,
      ipv6Address: gatewayRow.current_ipv6,
      location: {
        lat: Number(gatewayRow.current_latitude),
        lng: Number(gatewayRow.current_longitude),
        label: formatCoordinatePair(Number(gatewayRow.current_latitude), Number(gatewayRow.current_longitude)),
      },
      lastRegisteredAt: gatewayRow.last_registered_at,
      softwareVersion:
        gatewayRow.current_sw_version_packed === null ? null : formatPackedVersion(gatewayRow.current_sw_version_packed),
    },
    firstRegisteredAt: gatewayRow.first_registered_at,
    registrationCount: Number(gatewayRow.registration_count),
    recentRegistrations: registrationResult.rows.map((row: (typeof registrationResult.rows)[number]) => ({
      reportedAt: row.reported_at,
      latitude: Number(row.latitude),
      longitude: Number(row.longitude),
      softwareVersion: formatPackedVersion(row.sw_version_packed),
      backhaulVersion: row.backhaul_version,
    })),
  };
}

async function queryAlertsFromDb(nodes?: NodeSummary[]): Promise<AlertIncident[]> {
  const fleet = nodes ?? (await queryNodesFromDb());
  const result = await query<{
    id: number;
    node_id: number;
    alert_type: "critical_risk" | "connectivity_loss" | "battery_degradation" | "system";
    title: string;
    severity: AlertIncident["severity"];
    status: "open" | "acknowledged" | "cleared";
    occurred_at: string;
    detected_at: string;
    latest_event_at: string;
    last_seen_at: string | null;
    current_latitude: string;
    current_longitude: string;
    snapshot_reported_at: string | null;
    snapshot_risk_level: number | null;
    snapshot_temperature_c: string | null;
    snapshot_humidity_pct: string | null;
    snapshot_voc_iaq: number | null;
    snapshot_pm25_ug_m3: string | null;
    snapshot_battery_pct: number | null;
    sent_count: string;
    total_count: string;
    details: unknown;
  }>(
    `
      SELECT
        a.id,
        d.node_id,
        a.alert_type,
        a.title,
        a.severity,
        a.status,
        a.occurred_at::text,
        a.detected_at::text,
        a.latest_event_at::text,
        d.last_seen_at::text,
        d.current_latitude::text,
        d.current_longitude::text,
        a.snapshot_reported_at::text,
        a.snapshot_risk_level,
        a.snapshot_temperature_c::text,
        a.snapshot_humidity_pct::text,
        a.snapshot_voc_iaq,
        a.snapshot_pm25_ug_m3::text,
        a.snapshot_battery_pct,
        (
          SELECT count(*)::text
          FROM notification_deliveries nd
          WHERE nd.alert_id = a.id AND nd.status = 'sent'
        ) AS sent_count,
        (
          SELECT count(*)::text
          FROM notification_deliveries nd
          WHERE nd.alert_id = a.id
        ) AS total_count,
        a.details
      FROM alerts a
      JOIN devices d ON d.id = a.device_id
      ORDER BY a.detected_at DESC
      LIMIT 50
    `,
  );

  const directAlerts: AlertIncident[] = result.rows.flatMap((row: (typeof result.rows)[number]) => {
    const incidentMeta =
      row.alert_type === "critical_risk"
        ? {
            incidentType: "critical_alert" as const,
            eventCode: "0x03" as const,
            sourceType: "critical_alert" as const,
            severity: "critical" as const,
          }
        : row.alert_type === "battery_degradation"
          ? {
              incidentType: "battery_health_low" as const,
              eventCode: "battery-health-low" as const,
              sourceType: "periodic_report" as const,
              severity: "warning" as const,
            }
          : null;

    if (!incidentMeta) {
      return [];
    }

    const sentCount = Number(row.sent_count);
    const totalCount = Number(row.total_count);

    return [
      {
        id: `alert-${row.id}`,
        incidentType: incidentMeta.incidentType,
        eventCode: incidentMeta.eventCode,
        nodeId: row.node_id,
        nodeName: String(row.node_id),
        severity: incidentMeta.severity,
        status: row.status,
        title: row.title,
        summary: typeof row.details === "object" ? "Database alert event" : "Database alert event",
        occurredAt: row.occurred_at,
        detectedAt: row.detected_at,
        latestEventAt: row.latest_event_at,
        locationLabel: formatCoordinatePair(Number(row.current_latitude), Number(row.current_longitude)),
        notificationStatus: totalCount === 0 ? "skipped" : sentCount === totalCount ? "sent" : sentCount > 0 ? "partial" : "pending",
        lastSeenAt: row.last_seen_at,
        latestSnapshot:
          row.snapshot_reported_at && row.snapshot_risk_level !== null
            ? {
                reportedAt: row.snapshot_reported_at,
                sourceType: incidentMeta.sourceType,
                riskLevel: row.snapshot_risk_level,
                temperatureC: Number(row.snapshot_temperature_c ?? 0),
                humidityPct: Number(row.snapshot_humidity_pct ?? 0),
                vocIaq: row.snapshot_voc_iaq ?? 0,
                pm25UgM3: Number(row.snapshot_pm25_ug_m3 ?? 0),
                batteryPct: row.snapshot_battery_pct,
                pressureHpa: null,
                batteryHealthScore: null,
              }
            : null,
      },
    ];
  });

  const offlineIncidents = fleet
    .filter((node) => node.connectivity === "offline" && node.lastSeenAt)
    .map((node) => {
      const derivedAt = new Date(timestampMs(node.lastSeenAt) + offlineThresholdMs).toISOString();
      return {
        id: `offline-${node.nodeId}`,
        incidentType: "offline" as const,
        eventCode: "derived-offline" as const,
        nodeId: node.nodeId,
        nodeName: String(node.nodeId),
        severity: "warning" as const,
        status: "derived" as const,
        title: `${node.nodeId} silent for 5 minutes`,
        summary: "Derived offline incident because the node has not been seen within the required window.",
        occurredAt: derivedAt,
        detectedAt: derivedAt,
        latestEventAt: derivedAt,
        locationLabel: node.location.label,
        notificationStatus: "pending" as const,
        lastSeenAt: node.lastSeenAt,
        latestSnapshot: node.latestTelemetry,
      };
    });

  return [...directAlerts, ...offlineIncidents].sort(
    (left, right) => timestampMs(right.detectedAt) - timestampMs(left.detectedAt),
  );
}

async function getDashboardFromDb(): Promise<DashboardResponse> {
  const fleet = await queryNodesFromDb();
  const [alertQueue, downlinks, recentPackets, gateways] = await Promise.all([
    queryAlertsFromDb(fleet),
    queryDownlinksFromDb(),
    getPacketHistoryFromDb({ limit: 5 }).then((response) => response.entries),
    queryGatewaysFromDb(),
  ]);
  const neighborLinksResult = await query<{
    owner_node_id: number;
    owner_latitude: string;
    owner_longitude: string;
    neighbor_node_id: number;
    neighbor_latitude: string;
    neighbor_longitude: string;
    distance_meters: number;
  }>(
    `
      SELECT
        owner.node_id AS owner_node_id,
        owner.current_latitude::text AS owner_latitude,
        owner.current_longitude::text AS owner_longitude,
        neighbor.node_id AS neighbor_node_id,
        membership.neighbor_latitude::text AS neighbor_latitude,
        membership.neighbor_longitude::text AS neighbor_longitude,
        membership.distance_meters
      FROM devices owner
      JOIN nn_revisions revision ON revision.id = owner.current_nn_revision_id
      JOIN nn_revision_memberships membership ON membership.nn_revision_id = revision.id
      JOIN devices neighbor ON neighbor.id = membership.neighbor_device_id
      ORDER BY owner.node_id, membership.neighbor_rank
    `,
  );

  const neighborLinks = neighborLinksResult.rows.map((row: (typeof neighborLinksResult.rows)[number]) => ({
    ownerNodeId: row.owner_node_id,
    neighborNodeId: row.neighbor_node_id,
    ownerName: String(row.owner_node_id),
    neighborName: String(row.neighbor_node_id),
    distanceMeters: row.distance_meters,
    points: [
      {
        lat: Number(row.owner_latitude),
        lng: Number(row.owner_longitude),
        label: String(row.owner_node_id),
      },
      {
        lat: Number(row.neighbor_latitude),
        lng: Number(row.neighbor_longitude),
        label: String(row.neighbor_node_id),
      },
    ] as MeshLink["points"],
  }));

  return {
    summary: {
      totalNodes: fleet.length,
      onlineNodes: fleet.filter((node) => node.connectivity === "online").length,
      degradedNodes: fleet.filter((node) => node.connectivity === "degraded").length,
      offlineNodes: fleet.filter((node) => node.connectivity === "offline").length,
      criticalAlerts: alertQueue.filter((alert) => alert.incidentType === "critical_alert" && alert.status !== "cleared").length,
      offlineIncidents: alertQueue.filter((alert) => alert.incidentType === "offline").length,
      pendingDownlinks: downlinks.filter((downlink) => downlink.status === "pending" || downlink.status === "sent").length,
    },
    fleet,
    gateways,
    neighborLinks,
    alertQueue,
    downlinks,
    recentPackets,
  };
}

async function getNodeDetailFromDb(nodeId: NodeId): Promise<NodeDetail> {
  const fleet = await queryNodesFromDb();
  const node = fleet.find((entry) => entry.nodeId === nodeId);
  if (!node) {
    throw new Error(`Unknown node ${nodeId}`);
  }

  const [neighborResult, readingsResult, timelineResult, ipv6Result, registrationResult, parentResult, currentParentResult] = await Promise.all([
    query<{
      revision_id: number;
      revision_no: number;
      radius_meters: number;
      revision_source: NeighborRevision["revisionSource"];
      active_from: string;
      neighbor_node_id: number;
      neighbor_name: number;
      neighbor_rank: number;
      distance_meters: number;
      neighbor_latitude: string;
      neighbor_longitude: string;
      latest_risk_level: number | null;
      last_seen_at: string | null;
    }>(
      `
        SELECT
          revision.id AS revision_id,
          revision.revision_no,
          revision.radius_meters,
          revision.revision_source,
          revision.active_from::text,
          neighbor.node_id AS neighbor_node_id,
          neighbor.node_id AS neighbor_name,
          membership.neighbor_rank,
          membership.distance_meters,
          membership.neighbor_latitude::text,
          membership.neighbor_longitude::text,
          neighbor.latest_risk_level,
          neighbor.last_seen_at::text
        FROM devices owner
        JOIN nn_revisions revision ON revision.id = owner.current_nn_revision_id
        LEFT JOIN nn_revision_memberships membership ON membership.nn_revision_id = revision.id
        LEFT JOIN devices neighbor ON neighbor.id = membership.neighbor_device_id
        WHERE owner.node_id = $1
        ORDER BY membership.neighbor_rank
      `,
      [nodeId],
    ),
    query<{
      id: number;
      reported_at: string;
      source_type: TelemetrySnapshot["sourceType"];
      risk_level: number;
      temperature_c: string;
      humidity_pct: string;
      voc_iaq: number;
      pm25_ug_m3: string;
      battery_pct: number | null;
      pressure_hpa: string | null;
      battery_health_score: number | null;
    }>(
      `
        SELECT
          sr.id,
          sr.reported_at::text,
          sr.source_type,
          sr.risk_level,
          sr.temperature_c::text,
          sr.humidity_pct::text,
          sr.voc_iaq,
          sr.pm25_ug_m3::text,
          sr.battery_pct,
          sr.pressure_hpa::text,
          sr.battery_health_score
        FROM sensor_readings sr
        JOIN devices d ON d.id = sr.device_id
        WHERE d.node_id = $1
          AND sr.reported_at >= NOW() - interval '24 hours'
        ORDER BY sr.reported_at DESC
        LIMIT 48
      `,
      [nodeId],
    ),
    query<{
      id: number;
      event_type: string;
      event_at: string;
      actor: string | null;
      note: string | null;
      new_status: string | null;
      severity: AlertTimelineEntry["severity"];
      title: string;
    }>(
      `
        SELECT
          ae.id,
          ae.event_type,
          ae.event_at::text,
          ae.actor,
          ae.note,
          ae.new_status,
          a.severity,
          a.title
        FROM alert_events ae
        JOIN alerts a ON a.id = ae.alert_id
        JOIN devices d ON d.id = a.device_id
        WHERE d.node_id = $1
        ORDER BY ae.event_at DESC
        LIMIT 30
      `,
      [nodeId],
    ),
    query<{ ipv6_address: string; valid_from: string; valid_to: string | null }>(
      `
        SELECT
          host(ipv6_address) AS ipv6_address,
          valid_from::text,
          valid_to::text
        FROM device_ipv6_history history
        JOIN devices d ON d.id = history.device_id
        WHERE d.node_id = $1
        ORDER BY valid_from DESC
      `,
      [nodeId],
    ),
    query<{
      observed_at: string;
      observed_ipv6: string;
      latitude: string;
      longitude: string;
      firmware_version: string | null;
      battery_pct: number | null;
    }>(
      `
        SELECT
          observed_at::text,
          host(observed_ipv6) AS observed_ipv6,
          latitude::text,
          longitude::text,
          firmware_version,
          battery_pct
        FROM device_registrations registrations
        JOIN devices d ON d.id = registrations.device_id
        WHERE d.node_id = $1
        ORDER BY observed_at DESC
        LIMIT 10
      `,
      [nodeId],
    ),
    query<{
      observed_at: string;
      observed_parent_ipv6: string | null;
      source_type: NodeDetail["parentObservations"][number]["sourceType"];
    }>(
      `
        SELECT
          observed_at::text,
          CASE
            WHEN observed_parent_ipv6 IS NULL THEN NULL
            ELSE host(observed_parent_ipv6)
          END AS observed_parent_ipv6,
          source_type
        FROM device_parent_observations observations
        JOIN devices d ON d.id = observations.device_id
        WHERE d.node_id = $1
        ORDER BY observed_at DESC
        LIMIT 20
      `,
      [nodeId],
    ),
    query<{ current_parent_ipv6: string | null }>(
      `
        SELECT
          CASE
            WHEN current_parent_ipv6 IS NULL THEN NULL
            ELSE host(current_parent_ipv6)
          END AS current_parent_ipv6
        FROM devices
        WHERE node_id = $1
      `,
      [nodeId],
    ),
  ]);

  const neighborRows = neighborResult.rows.filter((row: (typeof neighborResult.rows)[number]) => row.neighbor_node_id);
  const currentNeighborRevision: NeighborRevision | null = neighborRows.length
    ? {
        id: neighborRows[0].revision_id,
        revisionNo: neighborRows[0].revision_no,
        radiusMeters: neighborRows[0].radius_meters,
        revisionSource: neighborRows[0].revision_source,
        activeFrom: neighborRows[0].active_from,
        neighbors: neighborRows.map<NeighborMembership>((row: (typeof neighborRows)[number]) => ({
          neighborId: row.neighbor_node_id,
          neighborNodeId: row.neighbor_node_id,
          neighborName: String(row.neighbor_name),
          rank: row.neighbor_rank,
          distanceMeters: row.distance_meters,
          location: {
            lat: Number(row.neighbor_latitude),
            lng: Number(row.neighbor_longitude),
            label: String(row.neighbor_name),
          },
          riskLevel: row.latest_risk_level,
          connectivity: connectivityFromLastSeen(row.last_seen_at),
        })),
      }
    : null;

  const recentReadings: ReadingHistoryPoint[] = readingsResult.rows.map((row: (typeof readingsResult.rows)[number]) => ({
    id: `reading-${row.id}`,
    nodeId,
    reportedAt: row.reported_at,
    sourceType: row.source_type,
    riskLevel: row.risk_level,
    temperatureC: Number(row.temperature_c),
    humidityPct: Number(row.humidity_pct),
    vocIaq: row.voc_iaq,
    pm25UgM3: Number(row.pm25_ug_m3),
    batteryPct: row.battery_pct,
    pressureHpa: row.pressure_hpa ? Number(row.pressure_hpa) : null,
    batteryHealthScore: row.battery_health_score,
  }));

  const alertTimeline: AlertTimelineEntry[] = timelineResult.rows.map((row: (typeof timelineResult.rows)[number]) => ({
    id: `timeline-${row.id}`,
    eventCode: "0x03" as const,
    title: row.title,
    summary: row.note ?? `Alert event ${row.event_type}`,
    occurredAt: row.event_at,
    status: row.new_status ?? row.event_type,
    severity: row.severity,
    actor: row.actor,
  }));

  if (node.connectivity === "offline" && node.lastSeenAt) {
    const derivedAt = new Date(timestampMs(node.lastSeenAt) + offlineThresholdMs).toISOString();
    alertTimeline.unshift({
      id: `offline-${node.nodeId}`,
      eventCode: "derived-offline",
      title: "Offline incident derived",
      summary: "No telemetry received for more than 5 minutes.",
      occurredAt: derivedAt,
      status: "derived",
      severity: "warning",
      actor: "system",
    });
  }

  return {
    node,
    currentParentIpv6: currentParentResult.rows[0]?.current_parent_ipv6 ?? null,
    currentNeighborRevision,
    recentReadings,
    alertTimeline,
    ipv6History: ipv6Result.rows.map((row: (typeof ipv6Result.rows)[number]) => ({
      address: row.ipv6_address,
      validFrom: row.valid_from,
      validTo: row.valid_to,
    })),
    recentRegistrations: registrationResult.rows.map((row: (typeof registrationResult.rows)[number]) => ({
      observedAt: row.observed_at,
      ipv6Address: row.observed_ipv6,
      latitude: Number(row.latitude),
      longitude: Number(row.longitude),
      firmwareVersion: row.firmware_version,
      batteryPct: row.battery_pct,
    })),
    parentObservations: parentResult.rows.map((row: (typeof parentResult.rows)[number]) => ({
      observedAt: row.observed_at,
      parentIpv6: row.observed_parent_ipv6,
      sourceType: row.source_type,
    })),
  };
}

async function getHistoryFromDb(nodeId?: NodeId, window: HistoryWindow = "24h"): Promise<HistoryResponse> {
  const nodes = await queryNodesFromDb();
  const selectedNode = nodes.find((node) => node.nodeId === nodeId) ?? nodes[0];

  if (!selectedNode) {
    throw new Error("No nodes available.");
  }

  if (window === "24h") {
    const rawResult = await query<{
      id: number;
      reported_at: string;
      source_type: TelemetrySnapshot["sourceType"];
      risk_level: number;
      temperature_c: string;
      humidity_pct: string;
      voc_iaq: number;
      pm25_ug_m3: string;
      battery_pct: number | null;
      pressure_hpa: string | null;
      battery_health_score: number | null;
    }>(
      `
        SELECT
          sr.id,
          sr.reported_at::text,
          sr.source_type,
          sr.risk_level,
          sr.temperature_c::text,
          sr.humidity_pct::text,
          sr.voc_iaq,
          sr.pm25_ug_m3::text,
          sr.battery_pct,
          sr.pressure_hpa::text,
          sr.battery_health_score
        FROM sensor_readings sr
        JOIN devices d ON d.id = sr.device_id
        WHERE d.node_id = $1
          AND sr.reported_at >= NOW() - interval '24 hours'
        ORDER BY sr.reported_at DESC
      `,
      [selectedNode.nodeId],
    );

    const rawReadings = rawResult.rows.map<ReadingHistoryPoint>((row: (typeof rawResult.rows)[number]) => ({
      id: `reading-${row.id}`,
      nodeId: selectedNode.nodeId,
      reportedAt: row.reported_at,
      sourceType: row.source_type,
      riskLevel: row.risk_level,
      temperatureC: Number(row.temperature_c),
      humidityPct: Number(row.humidity_pct),
      vocIaq: row.voc_iaq,
      pm25UgM3: Number(row.pm25_ug_m3),
      batteryPct: row.battery_pct,
      pressureHpa: row.pressure_hpa ? Number(row.pressure_hpa) : null,
      batteryHealthScore: row.battery_health_score,
    }));

    const trendSummary = buildTrendSummary(rawReadings);

    return {
      selectedNodeId: selectedNode.nodeId,
      selectedWindow: window,
      mode: "raw",
      availableNodes: nodes.map((node) => ({
        id: node.id,
        nodeId: node.nodeId,
        displayName: String(node.nodeId),
      })),
      rawReadings,
      aggregateBuckets: rawReadings.map((reading: ReadingHistoryPoint) => ({
        bucketStart: reading.reportedAt,
        bucketEnd: new Date(timestampMs(reading.reportedAt) + 60 * 60 * 1000).toISOString(),
        sampleCount: 1,
        avgTemperatureC: reading.temperatureC,
        avgHumidityPct: reading.humidityPct,
        avgVocIaq: reading.vocIaq,
        avgPm25UgM3: reading.pm25UgM3,
        maxRiskLevel: reading.riskLevel,
      })),
      trendSummary,
    };
  }

  const interval = window === "7d" ? "7 days" : "30 days";
  const aggregateResult = await query<{
    bucket_start: string;
    bucket_end: string;
    sample_count: number;
    avg_temperature_c: string | null;
    avg_humidity_pct: string | null;
    avg_voc_iaq: string | null;
    avg_pm25_ug_m3: string | null;
    max_risk_level: number | null;
  }>(
    `
      SELECT
        bucket_start::text,
        bucket_end::text,
        sample_count,
        avg_temperature_c::text,
        avg_humidity_pct::text,
        avg_voc_iaq::text,
        avg_pm25_ug_m3::text,
        max_risk_level
      FROM sensor_reading_hourly_aggregates aggregates
      JOIN devices d ON d.id = aggregates.device_id
      WHERE d.node_id = $1
        AND bucket_start >= NOW() - interval '${interval}'
      ORDER BY bucket_start DESC
    `,
    [selectedNode.nodeId],
  );

  const aggregateBuckets: HistoryAggregateBucket[] = aggregateResult.rows.map((row: (typeof aggregateResult.rows)[number]) => ({
    bucketStart: row.bucket_start,
    bucketEnd: row.bucket_end,
    sampleCount: row.sample_count,
    avgTemperatureC: row.avg_temperature_c ? Number(row.avg_temperature_c) : null,
    avgHumidityPct: row.avg_humidity_pct ? Number(row.avg_humidity_pct) : null,
    avgVocIaq: row.avg_voc_iaq ? Number(row.avg_voc_iaq) : null,
    avgPm25UgM3: row.avg_pm25_ug_m3 ? Number(row.avg_pm25_ug_m3) : null,
    maxRiskLevel: row.max_risk_level,
  }));

  return {
    selectedNodeId: selectedNode.nodeId,
    selectedWindow: window,
    mode: "aggregate",
    availableNodes: nodes.map((node) => ({
      id: node.id,
      nodeId: node.nodeId,
      displayName: String(node.nodeId),
    })),
    rawReadings: [],
    aggregateBuckets,
    trendSummary: {
      temperatureDeltaC:
        aggregateBuckets.length >= 2 && aggregateBuckets[0].avgTemperatureC !== null && aggregateBuckets.at(-1)?.avgTemperatureC !== null
          ? Number((aggregateBuckets[0].avgTemperatureC - (aggregateBuckets.at(-1)?.avgTemperatureC ?? 0)).toFixed(1))
          : null,
      humidityDeltaPct:
        aggregateBuckets.length >= 2 && aggregateBuckets[0].avgHumidityPct !== null && aggregateBuckets.at(-1)?.avgHumidityPct !== null
          ? Number((aggregateBuckets[0].avgHumidityPct - (aggregateBuckets.at(-1)?.avgHumidityPct ?? 0)).toFixed(1))
          : null,
      vocPeak: aggregateBuckets.reduce<number | null>((peak, bucket) => {
        return bucket.avgVocIaq === null ? peak : Math.max(peak ?? bucket.avgVocIaq, bucket.avgVocIaq);
      }, null),
      pm25Peak: aggregateBuckets.reduce<number | null>((peak, bucket) => {
        return bucket.avgPm25UgM3 === null ? peak : Math.max(peak ?? bucket.avgPm25UgM3, bucket.avgPm25UgM3);
      }, null),
    },
  };
}

function buildSensorReadingDetail(row: {
  risk_level: number;
  temperature_c: string;
  humidity_pct: string;
  voc_iaq: number;
  pm25_ug_m3: string;
}) {
  return [
    `Risk ${row.risk_level}`,
    `${Number(row.temperature_c).toFixed(1)}°C`,
    `${Number(row.humidity_pct).toFixed(0)}% RH`,
    `VOC ${row.voc_iaq} ppm`,
    `PM2.5 ${Number(row.pm25_ug_m3).toFixed(1)}`,
  ].join(" · ");
}

async function getPacketHistoryFromDb(options: PacketHistoryQuery = {}): Promise<PacketHistoryResponse> {
  const limit = Math.max(1, Math.min(100, options.limit ?? 20));
  const offset = Math.max(0, options.offset ?? 0);
  const [nodes, gateways] = await Promise.all([
    queryNodesFromDb(),
    query<{ gateway_id: number }>(
      `
        SELECT gateway_id
        FROM gateways
        ORDER BY gateway_id
      `,
    ),
  ]);
  const values: unknown[] = [];
  const filters: string[] = [];

  if (options.nodeId !== undefined) {
    values.push(options.nodeId);
    filters.push(`node_id = $${values.length}`);
  }
  if (options.direction) {
    values.push(options.direction);
    filters.push(`direction = $${values.length}`);
  }
  if (options.packetCode) {
    values.push(options.packetCode);
    filters.push(`packet_code = $${values.length}`);
  }
  if (options.eventType) {
    values.push(options.eventType);
    filters.push(`event_type = $${values.length}`);
  }
  if (options.status) {
    values.push(options.status);
    filters.push(`status = $${values.length}`);
  }

  const whereClause = filters.length > 0 ? `WHERE ${filters.join(" AND ")}` : "";
  const packetLogCte = `
    WITH packet_log AS (
      SELECT
        concat('gateway-registration-', registrations.id) AS id,
        registrations.reported_at::text AS occurred_at,
        gateways.gateway_id AS node_id,
        concat('BR ', gateways.gateway_id) AS node_name,
        'uplink'::text AS direction,
        '0x81'::text AS packet_code,
        'gateway_registration'::text AS event_type,
        'received'::text AS status,
        concat('Border router ', gateways.gateway_id, ' registration received.') AS summary,
        concat_ws(
          ' · ',
          concat('Lat ', registrations.latitude::text),
          concat('Lng ', registrations.longitude::text),
          concat(
            'SW ',
            (registrations.sw_version_packed >> 8)::text,
            '.',
            (registrations.sw_version_packed & 255)::text
          ),
          CASE
            WHEN registrations.raw_payload_metadata->>'remoteAddress' IS NOT NULL
              THEN concat('Source ', registrations.raw_payload_metadata->>'remoteAddress')
          END
        ) AS detail
      FROM gateway_registrations registrations
      JOIN gateways ON gateways.id = registrations.gateway_row_id

      UNION ALL

      SELECT
        concat('registration-', registrations.id) AS id,
        registrations.observed_at::text AS occurred_at,
        d.node_id,
        d.node_id::text AS node_name,
        'uplink'::text AS direction,
        '0x01'::text AS packet_code,
        'registration'::text AS event_type,
        'received'::text AS status,
        concat('Registration received from node ', d.node_id, '.') AS summary,
        concat_ws(
          ' · ',
          registrations.observed_ipv6::text,
          CASE WHEN registrations.firmware_version IS NOT NULL THEN concat('FW ', registrations.firmware_version) END,
          CASE WHEN registrations.battery_pct IS NOT NULL THEN concat('Battery ', registrations.battery_pct, '%') END
        ) AS detail
      FROM device_registrations registrations
      JOIN devices d ON d.id = registrations.device_id

      UNION ALL

      SELECT
        concat('reading-', sr.id) AS id,
        sr.reported_at::text AS occurred_at,
        d.node_id,
        d.node_id::text AS node_name,
        'uplink'::text AS direction,
        CASE WHEN sr.source_type = 'critical_alert' THEN '0x03' ELSE '0x02' END AS packet_code,
        sr.source_type::text AS event_type,
        'received'::text AS status,
        CASE
          WHEN sr.source_type = 'critical_alert' THEN concat('Critical alert uplink from node ', d.node_id, '.')
          ELSE concat('Periodic report received from node ', d.node_id, '.')
        END AS summary,
        concat_ws(
          ' · ',
          concat('Risk ', sr.risk_level),
          concat(sr.temperature_c::text, '°C'),
          concat(sr.humidity_pct::text, '% RH'),
          concat('VOC ', sr.voc_iaq, ' ppm'),
          concat('PM2.5 ', sr.pm25_ug_m3::text)
        ) AS detail
      FROM sensor_readings sr
      JOIN devices d ON d.id = sr.device_id

      UNION ALL

      SELECT
        concat('parent-', observations.id) AS id,
        observations.observed_at::text AS occurred_at,
        d.node_id,
        d.node_id::text AS node_name,
        'uplink'::text AS direction,
        '0x08'::text AS packet_code,
        'parent_update'::text AS event_type,
        'received'::text AS status,
        concat('Parent update received from node ', d.node_id, '.') AS summary,
        concat_ws(
          ' · ',
          CASE
            WHEN observations.observed_parent_ipv6 IS NULL THEN 'Preferred parent cleared'
            ELSE concat('Parent ', host(observations.observed_parent_ipv6))
          END,
          CASE
            WHEN observations.source_type = 'registration' THEN 'Observed during registration'
            ELSE 'Observed during parent update'
          END
        ) AS detail
      FROM device_parent_observations observations
      JOIN devices d ON d.id = observations.device_id
      WHERE observations.source_type = 'parent_update'

      UNION ALL

      SELECT
        concat('nn-', nde.id) AS id,
        nde.sent_at::text AS occurred_at,
        d.node_id,
        d.node_id::text AS node_name,
        'downlink'::text AS direction,
        '0x04'::text AS packet_code,
        'neighbor_distribution'::text AS event_type,
        nde.status::text AS status,
        concat('Neighbor table distribution sent to node ', d.node_id, '.') AS summary,
        concat_ws(
          ' · ',
          concat('Revision ', nn.revision_no),
          concat('Attempt ', nde.attempt_no),
          CASE WHEN nde.acknowledged_at IS NOT NULL THEN concat('Ack ', nde.acknowledged_at::text) END
        ) AS detail
      FROM nn_distribution_events nde
      JOIN devices d ON d.id = nde.device_id
      JOIN nn_revisions nn ON nn.id = nde.nn_revision_id

      UNION ALL

      SELECT
        concat('time-sync-', tse.id) AS id,
        tse.sent_at::text AS occurred_at,
        d.node_id,
        d.node_id::text AS node_name,
        'downlink'::text AS direction,
        '0x05'::text AS packet_code,
        'time_sync'::text AS event_type,
        tse.status::text AS status,
        concat('Time sync sent to node ', d.node_id, '.') AS summary,
        concat_ws(
          ' · ',
          concat('Target ', tse.target_time::text),
          tse.result_message,
          CASE WHEN tse.acknowledged_at IS NOT NULL THEN concat('Ack ', tse.acknowledged_at::text) END
        ) AS detail
      FROM time_sync_events tse
      JOIN devices d ON d.id = tse.device_id

      UNION ALL

      SELECT
        concat('config-', dcd.id) AS id,
        dcd.sent_at::text AS occurred_at,
        d.node_id,
        d.node_id::text AS node_name,
        'downlink'::text AS direction,
        '0x06'::text AS packet_code,
        'config_deployment'::text AS event_type,
        dcd.status::text AS status,
        concat('Threshold revision deployment sent to node ', d.node_id, '.') AS summary,
        concat_ws(
          ' · ',
          concat('Revision ', cr.config_id),
          concat('Attempt ', dcd.attempt_no),
          CASE WHEN dcd.acknowledged_at IS NOT NULL THEN concat('Ack ', dcd.acknowledged_at::text) END,
          CASE WHEN dcd.applied_at IS NOT NULL THEN concat('Applied ', dcd.applied_at::text) END
        ) AS detail
      FROM device_config_deployments dcd
      JOIN devices d ON d.id = dcd.device_id
      JOIN config_revisions cr ON cr.id = dcd.config_revision_id
    )
  `;

  const [countResult, entriesResult] = await Promise.all([
    query<{ count: string }>(
      `
        ${packetLogCte}
        SELECT count(*)::text AS count
        FROM packet_log
        ${whereClause}
      `,
      values,
    ),
    query<{
      id: string;
      occurred_at: string;
      node_id: number;
      node_name: string;
      direction: PacketDirection;
      packet_code: PacketLogCode;
      event_type: PacketEventType;
      status: PacketLogStatus;
      summary: string;
      detail: string | null;
    }>(
      `
        ${packetLogCte}
        SELECT
          id,
          occurred_at,
          node_id,
          node_name,
          direction,
          packet_code,
          event_type,
          status,
          summary,
          detail
        FROM packet_log
        ${whereClause}
        ORDER BY occurred_at DESC, id DESC
        LIMIT $${values.length + 1}
        OFFSET $${values.length + 2}
      `,
      [...values, limit, offset],
    ),
  ]);

  const totalCount = Number(countResult.rows[0]?.count ?? 0);

  return {
    entries: entriesResult.rows.map<PacketLogEntry>((row: (typeof entriesResult.rows)[number]) => ({
      id: row.id,
      occurredAt: row.occurred_at,
      nodeId: row.node_id,
      nodeName: row.node_name,
      direction: row.direction,
      packetCode: row.packet_code,
      eventType: row.event_type,
      status: row.status,
      summary: row.summary,
      detail: row.detail,
    })),
    totalCount,
    limit,
    offset,
    hasMore: offset + limit < totalCount,
    availableNodes: [
      ...nodes.map((node) => ({
        id: node.id,
        nodeId: node.nodeId,
        displayName: String(node.nodeId),
      })),
      ...gateways.rows.map((gateway) => ({
        id: `gateway-${gateway.gateway_id}`,
        nodeId: gateway.gateway_id,
        displayName: `BR ${gateway.gateway_id}`,
      })),
    ],
    availableDirections: [...packetDirections],
    availablePacketCodes: [...packetLogCodes],
    availableEventTypes: [...packetEventTypes],
    availableStatuses: ["received", "pending", "sent", "acknowledged", "failed", "timed_out"],
  };
}

async function clearPacketHistoryInDb() {
  if (!pool) {
    throw new Error("Database is not configured.");
  }

  const client = await pool.connect();

  try {
    await client.query("BEGIN");
    await client.query(
      `
        TRUNCATE TABLE
          notification_deliveries,
          devices,
          gateways
        RESTART IDENTITY CASCADE
      `,
    );
    await client.query("COMMIT");
  } catch (error) {
    await client.query("ROLLBACK");
    throw error;
  } finally {
    client.release();
  }
}

function buildTrendSummary(points: ReadingHistoryPoint[]): HistoryTrendSummary {
  if (points.length < 2) {
    return {
      temperatureDeltaC: null,
      humidityDeltaPct: null,
      vocPeak: points[0]?.vocIaq ?? null,
      pm25Peak: points[0]?.pm25UgM3 ?? null,
    };
  }

  const newest = points[0];
  const oldest = points[points.length - 1];
  return {
    temperatureDeltaC: Number((newest.temperatureC - oldest.temperatureC).toFixed(1)),
    humidityDeltaPct: Number((newest.humidityPct - oldest.humidityPct).toFixed(1)),
    vocPeak: Math.max(...points.map((point) => point.vocIaq)),
    pm25Peak: Math.max(...points.map((point) => point.pm25UgM3)),
  };
}

async function getConfigurationFromDb(): Promise<ConfigurationResponse> {
  const [nodes, revisionsResult, downlinks] = await Promise.all([
    queryNodesFromDb(),
    query<{
      id: number;
      config_id: number;
      activated_at: string | null;
      retired_at: string | null;
      created_at: string;
      notes: string | null;
      l2_temp_thresh: string;
      l2_humidity_thresh: string;
      l2_voc_thresh: number;
      l3_temp_thresh: string;
      l3_humidity_thresh: string;
      l3_voc_thresh: number;
      l4_voc_thresh: number;
      l5_voc_thresh: number;
      l5_pm25_thresh: string;
    }>(
      `
        SELECT
          id,
          config_id,
          activated_at::text,
          retired_at::text,
          created_at::text,
          notes,
          l2_temp_thresh::text,
          l2_humidity_thresh::text,
          l2_voc_thresh,
          l3_temp_thresh::text,
          l3_humidity_thresh::text,
          l3_voc_thresh,
          l4_voc_thresh,
          l5_voc_thresh,
          l5_pm25_thresh::text
        FROM config_revisions
        ORDER BY created_at DESC
        LIMIT 12
      `,
    ),
    queryDownlinksFromDb(),
  ]);

  const neighborTables = await Promise.all(
    nodes.map(async (node: NodeSummary) => {
      if (!node.currentNeighborRevisionId) {
        return {
          nodeId: node.nodeId,
          nodeName: String(node.nodeId),
          revisionId: null,
          revisionNo: null,
          radiusMeters: null,
          neighbors: [],
        };
      }

      const membershipsResult = await query<{
        revision_no: number;
        radius_meters: number;
        neighbor_node_id: number;
        neighbor_rank: number;
        distance_meters: number;
        neighbor_latitude: string;
        neighbor_longitude: string;
        latest_risk_level: number | null;
        last_seen_at: string | null;
      }>(
        `
          SELECT
            revision.revision_no,
            revision.radius_meters,
            neighbor.node_id AS neighbor_node_id,
            membership.neighbor_rank,
            membership.distance_meters,
            membership.neighbor_latitude::text,
            membership.neighbor_longitude::text,
            neighbor.latest_risk_level,
            neighbor.last_seen_at::text
          FROM nn_revisions revision
          LEFT JOIN nn_revision_memberships membership ON membership.nn_revision_id = revision.id
          LEFT JOIN devices neighbor ON neighbor.id = membership.neighbor_device_id
          WHERE revision.id = $1
          ORDER BY membership.neighbor_rank
        `,
        [node.currentNeighborRevisionId],
      );

      return {
        nodeId: node.nodeId,
        nodeName: String(node.nodeId),
        revisionId: node.currentNeighborRevisionId,
        revisionNo: membershipsResult.rows[0]?.revision_no ?? node.currentNeighborRevisionNo,
        radiusMeters: membershipsResult.rows[0]?.radius_meters ?? null,
        neighbors: membershipsResult.rows
          .filter((row: (typeof membershipsResult.rows)[number]) => row.neighbor_node_id)
          .map((row: (typeof membershipsResult.rows)[number]) => ({
            neighborId: row.neighbor_node_id,
            neighborNodeId: row.neighbor_node_id,
            neighborName: String(row.neighbor_node_id),
            rank: row.neighbor_rank,
            distanceMeters: row.distance_meters,
            location: {
              lat: Number(row.neighbor_latitude),
              lng: Number(row.neighbor_longitude),
              label: String(row.neighbor_node_id),
            },
            riskLevel: row.latest_risk_level,
            connectivity: connectivityFromLastSeen(row.last_seen_at),
          })),
      };
    }),
  );

  const revisions = revisionsResult.rows.map((row: (typeof revisionsResult.rows)[number]) => ({
    id: row.id,
    configId: row.config_id,
    activatedAt: row.activated_at,
    retiredAt: row.retired_at,
    createdAt: row.created_at,
    notes: row.notes,
    thresholds: {
      l2TempThresh: Number(row.l2_temp_thresh),
      l2HumidityThresh: Number(row.l2_humidity_thresh),
      l2VocThresh: row.l2_voc_thresh,
      l3TempThresh: Number(row.l3_temp_thresh),
      l3HumidityThresh: Number(row.l3_humidity_thresh),
      l3VocThresh: row.l3_voc_thresh,
      l4VocThresh: row.l4_voc_thresh,
      l5VocThresh: row.l5_voc_thresh,
      l5Pm25Thresh: Number(row.l5_pm25_thresh),
    },
  }));

  return {
    activeRevision: revisions.find((revision: (typeof revisions)[number]) => revision.retiredAt === null) ?? null,
    revisions,
    neighborTables,
    downlinks,
  };
}

async function getNotificationsFromDb(): Promise<NotificationSettingsResponse> {
  const [recipientsResult, deliveriesResult] = await Promise.all([
    query<{
      id: number;
      display_name: string | null;
      email_address: string;
      is_enabled: boolean;
      event_type: NotificationEventType | null;
      preference_enabled: boolean | null;
    }>(
      `
        SELECT
          recipient.id,
          recipient.display_name,
          recipient.email_address,
          recipient.is_enabled,
          preference.event_type,
          preference.is_enabled AS preference_enabled
        FROM notification_recipients recipient
        LEFT JOIN notification_preferences preference ON preference.recipient_id = recipient.id
        ORDER BY recipient.id, preference.event_type
      `,
    ),
    query<{
      id: number;
      recipient_id: number;
      recipient_name: string | null;
      event_type: NotificationEventType;
      subject: string;
      status: NotificationSettingsResponse["deliveries"][number]["status"];
      event_occurred_at: string;
      delivered_at: string | null;
      node_id: number | null;
      alert_id: number | null;
      failure_reason: string | null;
    }>(
      `
        SELECT
          delivery.id,
          delivery.recipient_id,
          recipient.display_name AS recipient_name,
          delivery.event_type,
          delivery.subject,
          delivery.status,
          delivery.event_occurred_at::text,
          delivery.delivered_at::text,
          device.node_id,
          delivery.alert_id,
          delivery.failure_reason
        FROM notification_deliveries delivery
        JOIN notification_recipients recipient ON recipient.id = delivery.recipient_id
        LEFT JOIN devices device ON device.id = delivery.device_id
        ORDER BY delivery.event_occurred_at DESC
        LIMIT 50
      `,
    ),
  ]);

  const recipientsMap = new Map<number, NotificationSettingsResponse["recipients"][number]>();
  for (const row of recipientsResult.rows) {
    const existing = recipientsMap.get(row.id);
    if (existing) {
      if (row.event_type) {
        existing.preferences.push({
          eventType: row.event_type,
          isEnabled: row.preference_enabled ?? false,
        });
      }
      continue;
    }

    recipientsMap.set(row.id, {
      id: String(row.id),
      displayName: row.display_name ?? row.email_address,
      emailAddress: row.email_address,
      isEnabled: row.is_enabled,
      preferences: row.event_type
        ? [
            {
              eventType: row.event_type,
              isEnabled: row.preference_enabled ?? false,
            },
          ]
        : [],
    });
  }

  for (const recipient of recipientsMap.values()) {
    for (const eventType of notificationEventTypes) {
      if (!recipient.preferences.some((preference) => preference.eventType === eventType)) {
        recipient.preferences.push({
          eventType,
          isEnabled: false,
        });
      }
    }
  }

  return {
    recipients: [...recipientsMap.values()],
    deliveries: deliveriesResult.rows.map((row: (typeof deliveriesResult.rows)[number]) => ({
      id: String(row.id),
      recipientId: String(row.recipient_id),
      recipientName: row.recipient_name ?? `Recipient ${row.recipient_id}`,
      eventType: row.event_type,
      subject: row.subject,
      status: row.status,
      occurredAt: row.event_occurred_at,
      deliveredAt: row.delivered_at,
      nodeId: row.node_id,
      alertId: row.alert_id ? String(row.alert_id) : null,
      failureReason: row.failure_reason,
    })),
  };
}

async function createConfigRevisionInDb(draft: ConfigRevisionDraft): Promise<ConfigurationResponse> {
  if (!pool) {
    throw new Error("Database is not configured.");
  }

  const thresholds = normalizeConfigThresholds(draft.thresholds);
  const client = await pool.connect();
  try {
    await client.query("BEGIN");
    await client.query("UPDATE config_revisions SET retired_at = NOW() WHERE retired_at IS NULL AND activated_at IS NOT NULL");

    const inserted = await client.query<{ id: number }>(
      `
        INSERT INTO config_revisions (
          l2_temp_thresh,
          l2_humidity_thresh,
          l2_voc_thresh,
          l3_temp_thresh,
          l3_humidity_thresh,
          l3_voc_thresh,
          l4_voc_thresh,
          l5_voc_thresh,
          l5_pm25_thresh,
          activated_at,
          notes
        )
        VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,NOW(),$10)
        RETURNING id
      `,
      [
        thresholds.l2TempThresh,
        thresholds.l2HumidityThresh,
        thresholds.l2VocThresh,
        thresholds.l3TempThresh,
        thresholds.l3HumidityThresh,
        thresholds.l3VocThresh,
        thresholds.l4VocThresh,
        thresholds.l5VocThresh,
        thresholds.l5Pm25Thresh,
        draft.notes ?? null,
      ],
    );

    const configRevisionId = inserted.rows[0]?.id;
    if (!configRevisionId) {
      throw new Error("Unable to create config revision.");
    }

    const targets = await resolveRoutedDeviceTargetsInDb(client, draft.targetNodeIds);
    const configPayload = await buildConfigUpdatePayloadInDb(client, configRevisionId);

    for (const target of targets) {
      const deploymentInsert = await client.query<{ id: number }>(
        `
          INSERT INTO device_config_deployments (device_id, config_revision_id, status, sent_at, payload_metadata)
          VALUES ($1, $2, 'pending', NOW(), $3::jsonb)
          RETURNING id
        `,
        [target.deviceId, configRevisionId, JSON.stringify(buildDownlinkRoutingMetadata(target))],
      );
      const deploymentId = deploymentInsert.rows[0]?.id;
      if (!deploymentId) {
        throw new Error(`Unable to create config deployment audit row for node ${target.nodeId}`);
      }
      await insertGatewayDownlinkInDb(client, {
        target,
        commandCode: configUpdatePacketType,
        innerPayload: configPayload,
        deviceConfigDeploymentId: deploymentId,
      });
      await client.query("UPDATE devices SET current_config_revision_id = $2 WHERE id = $1", [target.deviceId, configRevisionId]);
    }

    await client.query("COMMIT");
  } catch (error) {
    await client.query("ROLLBACK");
    throw error;
  } finally {
    client.release();
  }

  return getConfigurationFromDb();
}

async function updateNeighborRevisionInDb(nodeId: NodeId, draft: NeighborRevisionDraft): Promise<ConfigurationResponse> {
  if (!pool) {
    throw new Error("Database is not configured.");
  }

  const client = await pool.connect();
  try {
    await client.query("BEGIN");

    const ownerResult = await client.query<{
      id: number;
      current_latitude: string;
      current_longitude: string;
      last_observed_gateway_row_id: number | null;
      last_observed_gateway_at: string | null;
      gateway_id: number | null;
    }>(
      `
        SELECT
          d.id,
          d.current_latitude::text,
          d.current_longitude::text,
          d.last_observed_gateway_row_id,
          d.last_observed_gateway_at::text,
          g.gateway_id
        FROM devices d
        LEFT JOIN gateways g ON g.id = d.last_observed_gateway_row_id
        WHERE d.node_id = $1
      `,
      [nodeId],
    );
    const owner = ownerResult.rows[0];
    if (!owner) {
      throw new Error(`Unknown node ${nodeId}`);
    }
    if (
      owner.last_observed_gateway_row_id === null ||
      owner.last_observed_gateway_at === null ||
      owner.gateway_id === null
    ) {
      throw new Error(
        `Cannot queue downlink for unroutable node ${nodeId}. The node needs a previously observed gateway uplink.`,
      );
    }

    await client.query("UPDATE nn_revisions SET active_to = NOW() WHERE device_id = $1 AND active_to IS NULL", [owner.id]);
    const nextRevisionResult = await client.query<{ revision_no: number }>(
      "SELECT COALESCE(MAX(revision_no), 0) + 1 AS revision_no FROM nn_revisions WHERE device_id = $1",
      [owner.id],
    );
    const nextRevisionNo = nextRevisionResult.rows[0]?.revision_no ?? 1;

    const revisionInsert = await client.query<{ id: number }>(
      `
        INSERT INTO nn_revisions (device_id, revision_no, radius_meters, revision_source, active_from)
        VALUES ($1, $2, $3, 'manual', NOW())
        RETURNING id
      `,
      [owner.id, nextRevisionNo, draft.radiusMeters],
    );
    const revisionId = revisionInsert.rows[0]?.id;
    if (!revisionId) {
      throw new Error("Unable to create neighbor revision.");
    }

    const neighborRows = draft.neighborNodeIds.length
      ? (
          await client.query<{ id: number; node_id: number; current_latitude: string; current_longitude: string }>(
            "SELECT id, node_id, current_latitude::text, current_longitude::text FROM devices WHERE node_id = ANY($1::smallint[])",
            [draft.neighborNodeIds],
          )
        ).rows
      : [];

    for (const [index, neighbor] of neighborRows.entries()) {
      await client.query(
        `
          INSERT INTO nn_revision_memberships (
            nn_revision_id,
            owner_device_id,
            neighbor_device_id,
            neighbor_rank,
            distance_meters,
            neighbor_latitude,
            neighbor_longitude
          )
          VALUES ($1, $2, $3, $4, $5, $6, $7)
        `,
        [
          revisionId,
          owner.id,
          neighbor.id,
          index + 1,
          haversineDistanceMeters(
            { lat: Number(owner.current_latitude), lng: Number(owner.current_longitude) },
            { lat: Number(neighbor.current_latitude), lng: Number(neighbor.current_longitude) },
          ),
          neighbor.current_latitude,
          neighbor.current_longitude,
        ],
      );
    }

    await client.query("UPDATE devices SET current_nn_revision_id = $2 WHERE id = $1", [owner.id, revisionId]);
    const [target] = await resolveRoutedDeviceTargetsInDb(client, [nodeId]);
    const nnPayload = await buildNeighborDistributionPayloadInDb(client, {
      targetNodeId: nodeId,
      nnRevisionId: revisionId,
    });
    const distributionInsert = await client.query<{ id: number }>(
      `
        INSERT INTO nn_distribution_events (device_id, nn_revision_id, status, sent_at, payload_metadata)
        VALUES ($1, $2, 'pending', NOW(), $3::jsonb)
        RETURNING id
      `,
      [
        owner.id,
        revisionId,
        JSON.stringify({
          routingBasis: "last_observed_gateway",
          gatewayRowId: owner.last_observed_gateway_row_id,
          gatewayId: owner.gateway_id,
          lastObservedGatewayAt: owner.last_observed_gateway_at,
        }),
      ],
    );
    const distributionEventId = distributionInsert.rows[0]?.id;
    if (!distributionEventId) {
      throw new Error(`Unable to create NN distribution audit row for node ${nodeId}`);
    }
    await insertGatewayDownlinkInDb(client, {
      target,
      commandCode: neighborTableUpdatePacketType,
      innerPayload: nnPayload,
      nnDistributionEventId: distributionEventId,
    });

    await client.query("COMMIT");
  } catch (error) {
    await client.query("ROLLBACK");
    throw error;
  } finally {
    client.release();
  }

  return getConfigurationFromDb();
}

async function createTimeSyncInDb(request: DownlinkRequest): Promise<ConfigurationResponse> {
  if (!pool) {
    throw new Error("Database is not configured.");
  }

  const client = await pool.connect();
  try {
    await client.query("BEGIN");
    const targets = await resolveRoutedDeviceTargetsInDb(client, request.targetNodeIds);

    for (const target of targets) {
      await queueTimeSyncForTargetInDb(client, target);
    }

    await client.query("COMMIT");
  } catch (error) {
    await client.query("ROLLBACK");
    throw error;
  } finally {
    client.release();
  }

  return getConfigurationFromDb();
}

async function createNeighborDistributionInDb(request: DownlinkRequest): Promise<ConfigurationResponse> {
  if (!pool) {
    throw new Error("Database is not configured.");
  }

  const client = await pool.connect();
  try {
    await client.query("BEGIN");
    const targets = await resolveRoutedDeviceTargetsInDb(client, request.targetNodeIds);

    for (const target of targets) {
      if (!target.currentNeighborRevisionId) {
        continue;
      }
      const nnPayload = await buildNeighborDistributionPayloadInDb(client, {
        targetNodeId: target.nodeId,
        nnRevisionId: target.currentNeighborRevisionId,
      });
      const distributionInsert = await client.query<{ id: number }>(
        `
          INSERT INTO nn_distribution_events (device_id, nn_revision_id, status, sent_at, payload_metadata)
          VALUES ($1, $2, 'pending', NOW(), $3::jsonb)
          RETURNING id
        `,
        [target.deviceId, target.currentNeighborRevisionId, JSON.stringify(buildDownlinkRoutingMetadata(target))],
      );
      const distributionEventId = distributionInsert.rows[0]?.id;
      if (!distributionEventId) {
        throw new Error(`Unable to create NN distribution audit row for node ${target.nodeId}`);
      }
      await insertGatewayDownlinkInDb(client, {
        target,
        commandCode: neighborTableUpdatePacketType,
        innerPayload: nnPayload,
        nnDistributionEventId: distributionEventId,
      });
    }

    await client.query("COMMIT");
  } catch (error) {
    await client.query("ROLLBACK");
    throw error;
  } finally {
    client.release();
  }

  return getConfigurationFromDb();
}

async function createThresholdPushInDb(request: DownlinkRequest): Promise<ConfigurationResponse> {
  if (!pool) {
    throw new Error("Database is not configured.");
  }

  const client = await pool.connect();
  try {
    await client.query("BEGIN");

    const configRevisionId =
      request.configRevisionId ??
      (
        await client.query<{ id: number }>(
          "SELECT id FROM config_revisions WHERE retired_at IS NULL ORDER BY created_at DESC LIMIT 1",
        )
      ).rows[0]?.id;

    if (!configRevisionId) {
      throw new Error("No active config revision found.");
    }

    const targets = await resolveRoutedDeviceTargetsInDb(client, request.targetNodeIds);
    const configPayload = await buildConfigUpdatePayloadInDb(client, configRevisionId);

    for (const target of targets) {
      const deploymentInsert = await client.query<{ id: number }>(
        `
          INSERT INTO device_config_deployments (device_id, config_revision_id, status, sent_at, payload_metadata)
          VALUES ($1, $2, 'pending', NOW(), $3::jsonb)
          RETURNING id
        `,
        [target.deviceId, configRevisionId, JSON.stringify(buildDownlinkRoutingMetadata(target))],
      );
      const deploymentId = deploymentInsert.rows[0]?.id;
      if (!deploymentId) {
        throw new Error(`Unable to create config deployment audit row for node ${target.nodeId}`);
      }
      await insertGatewayDownlinkInDb(client, {
        target,
        commandCode: configUpdatePacketType,
        innerPayload: configPayload,
        deviceConfigDeploymentId: deploymentId,
      });
    }

    await client.query("COMMIT");
  } catch (error) {
    await client.query("ROLLBACK");
    throw error;
  } finally {
    client.release();
  }

  return getConfigurationFromDb();
}

async function updateRecipientInDb(
  recipientId: string,
  update: NotificationRecipientUpdate,
): Promise<NotificationSettingsResponse> {
  if (!pool) {
    throw new Error("Database is not configured.");
  }

  const numericRecipientId = Number(recipientId);
  await query("UPDATE notification_recipients SET is_enabled = $2, updated_at = NOW() WHERE id = $1", [
    numericRecipientId,
    update.isEnabled,
  ]);

  for (const eventType of notificationEventTypes) {
    await query(
      `
        INSERT INTO notification_preferences (recipient_id, event_type, is_enabled, updated_at)
        VALUES ($1, $2, $3, NOW())
        ON CONFLICT (recipient_id, event_type)
        DO UPDATE SET is_enabled = EXCLUDED.is_enabled, updated_at = NOW()
      `,
      [numericRecipientId, eventType, update.enabledEventTypes.includes(eventType)],
    );
  }

  return getNotificationsFromDb();
}

async function withSource<T>(databaseLoader: () => Promise<T>, mockLoader: () => T): Promise<T> {
  if (!pool) {
    return mockLoader();
  }
  return databaseLoader();
}

app.get("/api/health", async (_request, response) => {
  response.json({
    ok: true,
    database: Boolean(pool),
  });
});

app.post("/api/v1/gateways/register", octetStreamBody, async (request, response, next) => {
  try {
    if (!pool) {
      response.status(503).json({ message: "Database is not configured." });
      return;
    }

    if (!request.is("application/octet-stream")) {
      response.status(415).json({ message: "Content-Type must be application/octet-stream." });
      return;
    }

    const body = request.body;
    if (!Buffer.isBuffer(body) || body.length === 0) {
      response.status(400).json({ message: "Request body must contain one binary 0x81 gateway registration message." });
      return;
    }

    try {
      const message = parseGatewayRegistrationMessage(body);
      await persistGatewayRegistrationInDb(message, {
        httpContentType: request.get("content-type") ?? null,
        httpContentLength: body.length,
        remoteAddress: request.ip || null,
        downlinkUrl: resolveGatewayDownlinkUrlFromRequest(request),
      });
    } catch (error) {
      if (!(error instanceof BackhaulCodecError)) {
        throw error;
      }
    }

    response.status(204).end();
  } catch (error) {
    next(error);
  }
});

app.post("/api/v1/uplinks", octetStreamBody, async (request, response, next) => {
  try {
    if (!pool) {
      response.status(503).json({ message: "Database is not configured." });
      return;
    }

    if (!request.is("application/octet-stream")) {
      response.status(415).json({ message: "Content-Type must be application/octet-stream." });
      return;
    }

    const body = request.body;
    if (!Buffer.isBuffer(body) || body.length === 0) {
      response.status(400).json({ message: "Request body must contain one binary 0x82 node uplink envelope." });
      return;
    }

    const requestMetadata = {
      httpContentType: request.get("content-type") ?? null,
      httpContentLength: body.length,
      remoteAddress: request.ip || null,
      downlinkUrl: resolveGatewayDownlinkUrlFromRequest(request),
    };

    let receipt: PersistedUplinkReceipt;

    try {
      const message = parseNodeUplinkEnvelopeMessage(body);
      receipt = await persistGatewayUplinkInDb(message, requestMetadata);
    } catch (error) {
      if (error instanceof BackhaulCodecError) {
        receipt = await persistAcceptedUplinkDespiteErrorInDb(body, error, requestMetadata);
      } else {
        throw error;
      }
    }

    response
      .status(200)
      .type("application/octet-stream")
      .set("X-PyroNet-Receipt-Status", receipt.receiptStatus)
      .set("X-PyroNet-Receipt-Duplicate", String(receipt.isDuplicate))
      .set("X-PyroNet-Reject-Code", receipt.rejectCode ?? "")
      .send(receipt.receiptBuffer);
  } catch (error) {
    next(error);
  }
});

app.get("/api/dashboard", async (_request, response, next) => {
  try {
    response.json(await withSource(getDashboardFromDb, getMockDashboard));
  } catch (error) {
    next(error);
  }
});

app.get("/api/nodes", async (_request, response, next) => {
  try {
    response.json(await withSource(queryNodesFromDb, listMockNodes));
  } catch (error) {
    next(error);
  }
});

app.get("/api/gateways", async (_request, response, next) => {
  try {
    response.json(await withSource(queryGatewaysFromDb, listMockGateways));
  } catch (error) {
    next(error);
  }
});

app.get("/api/gateways/:gatewayId", async (request, response, next) => {
  try {
    const gatewayId = parseGatewayId(request.params.gatewayId);
    if (gatewayId === null) {
      response.status(400).json({ message: "gatewayId must be a valid unsigned 16-bit integer." });
      return;
    }
    response.json(
      await withSource(() => getBorderRouterDetailFromDb(gatewayId), () => getMockBorderRouterDetail(gatewayId)),
    );
  } catch (error) {
    next(error);
  }
});

app.get("/api/nodes/:nodeId", async (request, response, next) => {
  try {
    const nodeId = parseNodeId(request.params.nodeId);
    if (nodeId === null) {
      response.status(400).json({ message: "nodeId must be a valid 2-byte integer." });
      return;
    }
    response.json(
      await withSource(() => getNodeDetailFromDb(nodeId), () => getMockNodeDetail(nodeId)),
    );
  } catch (error) {
    next(error);
  }
});

app.get("/api/alerts", async (_request, response, next) => {
  try {
    response.json(await withSource(() => queryAlertsFromDb(), listMockAlerts));
  } catch (error) {
    next(error);
  }
});

app.get("/api/history", async (request, response, next) => {
  try {
    const nodeIdInput = typeof request.query.nodeId === "string" ? request.query.nodeId : undefined;
    const parsedNodeId = nodeIdInput === undefined ? undefined : parseNodeId(nodeIdInput);
    if (nodeIdInput !== undefined && parsedNodeId === null) {
      response.status(400).json({ message: "nodeId must be a valid 2-byte integer." });
      return;
    }
    const nodeId = parsedNodeId ?? undefined;
    const window = (typeof request.query.window === "string" ? request.query.window : "24h") as HistoryWindow;
    response.json(await withSource(() => getHistoryFromDb(nodeId, window), () => getMockHistory(nodeId, window)));
  } catch (error) {
    next(error);
  }
});

app.get("/api/history/packets", async (request, response, next) => {
  try {
    const nodeIdInput = typeof request.query.nodeId === "string" ? request.query.nodeId : undefined;
    const parsedNodeId = nodeIdInput === undefined ? undefined : parseNodeId(nodeIdInput);
    if (nodeIdInput !== undefined && parsedNodeId === null) {
      response.status(400).json({ message: "nodeId must be a valid 2-byte integer." });
      return;
    }

    const limitInput = typeof request.query.limit === "string" ? Number.parseInt(request.query.limit, 10) : undefined;
    if (request.query.limit !== undefined && (!Number.isFinite(limitInput) || (limitInput ?? 0) <= 0)) {
      response.status(400).json({ message: "limit must be a positive integer." });
      return;
    }

    const offsetInput = typeof request.query.offset === "string" ? Number.parseInt(request.query.offset, 10) : undefined;
    if (request.query.offset !== undefined && (!Number.isFinite(offsetInput) || (offsetInput ?? -1) < 0)) {
      response.status(400).json({ message: "offset must be zero or a positive integer." });
      return;
    }

    const direction = typeof request.query.direction === "string" ? request.query.direction : undefined;
    if (direction !== undefined && !packetDirections.includes(direction as PacketDirection)) {
      response.status(400).json({ message: "direction must be a supported packet direction." });
      return;
    }

    const packetCode = typeof request.query.packetCode === "string" ? request.query.packetCode : undefined;
    if (packetCode !== undefined && !packetLogCodes.includes(packetCode as PacketLogCode)) {
      response.status(400).json({ message: "packetCode must be a supported packet code." });
      return;
    }

    const eventType = typeof request.query.eventType === "string" ? request.query.eventType : undefined;
    if (eventType !== undefined && !packetEventTypes.includes(eventType as PacketEventType)) {
      response.status(400).json({ message: "eventType must be a supported packet event type." });
      return;
    }

    const status = typeof request.query.status === "string" ? request.query.status : undefined;
    const validStatuses: PacketLogStatus[] = ["received", "pending", "sent", "acknowledged", "failed", "timed_out"];
    if (status !== undefined && !validStatuses.includes(status as PacketLogStatus)) {
      response.status(400).json({ message: "status must be a supported packet status." });
      return;
    }

    const query: PacketHistoryQuery = {
      nodeId: parsedNodeId ?? undefined,
      limit: limitInput,
      offset: offsetInput,
      direction: direction as PacketDirection | undefined,
      packetCode: packetCode as PacketLogCode | undefined,
      eventType: eventType as PacketEventType | undefined,
      status: status as PacketLogStatus | undefined,
    };

    response.json(await withSource(() => getPacketHistoryFromDb(query), () => getMockPacketHistory(query)));
  } catch (error) {
    next(error);
  }
});

app.delete("/api/history/packets", async (_request, response, next) => {
  try {
    if (!pool) {
      response.status(503).json({ message: "Database is not configured." });
      return;
    }

    await clearPacketHistoryInDb();
    response.json({ ok: true });
  } catch (error) {
    next(error);
  }
});

app.get("/api/configuration", async (_request, response, next) => {
  try {
    response.json(await withSource(getConfigurationFromDb, getMockConfiguration));
  } catch (error) {
    next(error);
  }
});

app.post("/api/configuration/revisions", async (request, response, next) => {
  try {
    const body = request.body as ConfigRevisionDraft;
    response.json(await withSource(() => createConfigRevisionInDb(body), () => createMockConfigRevision(body)));
  } catch (error) {
    next(error);
  }
});

app.post("/api/configuration/neighbors/:nodeId", async (request, response, next) => {
  try {
    const nodeId = parseNodeId(request.params.nodeId);
    if (nodeId === null) {
      response.status(400).json({ message: "nodeId must be a valid 2-byte integer." });
      return;
    }
    const body = request.body as NeighborRevisionDraft;
    response.json(
      await withSource(
        () => updateNeighborRevisionInDb(nodeId, body),
        () => updateMockNeighborRevision(nodeId, body),
      ),
    );
  } catch (error) {
    next(error);
  }
});

app.post("/api/configuration/downlinks/neighbor-distribution", async (request, response, next) => {
  try {
    const body = (request.body ?? {}) as DownlinkRequest;
    response.json(
      await withSource(
        () => createNeighborDistributionInDb(body),
        () => createMockNeighborDistribution(body),
      ),
    );
  } catch (error) {
    next(error);
  }
});

app.post("/api/configuration/downlinks/time-sync", async (request, response, next) => {
  try {
    const body = (request.body ?? {}) as DownlinkRequest;
    response.json(await withSource(() => createTimeSyncInDb(body), () => createMockTimeSync(body)));
  } catch (error) {
    next(error);
  }
});

app.post("/api/configuration/downlinks/threshold-push", async (request, response, next) => {
  try {
    const body = (request.body ?? {}) as DownlinkRequest;
    response.json(await withSource(() => createThresholdPushInDb(body), () => createMockThresholdPush(body)));
  } catch (error) {
    next(error);
  }
});

app.get("/api/notifications", async (_request, response, next) => {
  try {
    response.json(await withSource(getNotificationsFromDb, getMockNotificationSettings));
  } catch (error) {
    next(error);
  }
});

app.put("/api/notifications/recipients/:recipientId", async (request, response, next) => {
  try {
    const body = request.body as NotificationRecipientUpdate;
    response.json(
      await withSource(
        () => updateRecipientInDb(request.params.recipientId, body),
        () => updateMockNotificationRecipient(request.params.recipientId, body),
      ),
    );
  } catch (error) {
    next(error);
  }
});

app.use((error: unknown, _request: express.Request, response: express.Response, _next: express.NextFunction) => {
  const message = normalizePgError(error);
  response.status(500).json({ message });
});

app.listen(port, () => {
  startGatewayDownlinkDispatcher();
  process.stdout.write(`PyroNet API listening on http://localhost:${port}\n`);
});
