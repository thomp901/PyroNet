import cors from "cors";
import express from "express";
import { Pool, type QueryResultRow } from "pg";
import type {
  AlertIncident,
  AlertTimelineEntry,
  ConfigRevisionDraft,
  ConfigurationResponse,
  ConnectivityStatus,
  DashboardResponse,
  DownlinkRequest,
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
import { isNotificationEventType, notificationEventTypes, packetDirections, packetEventTypes, packetLogCodes } from "../src/api/types";
import { parseNodeId } from "../src/lib/nodeId";
import {
  createMockConfigRevision,
  createMockNeighborDistribution,
  createMockThresholdPush,
  createMockTimeSync,
  getMockConfiguration,
  getMockDashboard,
  getMockHistory,
  getMockPacketHistory,
  getMockNodeDetail,
  getMockNotificationSettings,
  listMockAlerts,
  listMockNodes,
  updateMockNeighborRevision,
  updateMockNotificationRecipient,
} from "../src/mocks/mockBackend";
<<<<<<< HEAD
=======
import { decodePacket, PacketIngestError, resolvePacketReceivedAt } from "./packetIngest";
import { sendEmail } from "./email";
>>>>>>> 8ed2bba (notifications)

const app = express();
const port = Number(process.env.API_PORT ?? "4000");
const databaseUrl = process.env.DATABASE_URL?.trim();
const pool = databaseUrl ? new Pool({ connectionString: databaseUrl }) : null;
<<<<<<< HEAD
const notificationEventTypes: NotificationEventType[] = [
  "critical_risk",
  "connectivity_loss",
  "battery_degradation",
  "system",
  "time_sync_failure",
  "nn_update_failure",
  "config_update_failure",
];
=======
const twilioAccountSid = process.env.TWILIO_ACCOUNT_SID?.trim() || null;
const twilioAuthToken = process.env.TWILIO_AUTH_TOKEN?.trim() || null;
const twilioFromNumber = process.env.TWILIO_FROM_NUMBER?.trim() || null;
const twilioMessagingServiceSid = process.env.TWILIO_MESSAGING_SERVICE_SID?.trim() || null;
>>>>>>> 8ed2bba (notifications)

app.use(cors());
app.use(express.json());

function getNowMs() {
  return Date.now();
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

const DEGRADED_THRESHOLD_MS = 30 * 60 * 1000;
const OFFLINE_THRESHOLD_MS = 24 * 60 * 60 * 1000;
const BATTERY_DEGRADATION_OPEN_THRESHOLD_PCT = 20;
const BATTERY_DEGRADATION_CLEAR_THRESHOLD_PCT = 25;
const DOWNLINK_FAILURE_TIMEOUT_MS = 15 * 60 * 1000;
const DOWNLINK_FAILURE_TIMEOUT_MINUTES = DOWNLINK_FAILURE_TIMEOUT_MS / (60 * 1000);
const DOWNLINK_FAILURE_SYNC_LOCK_KEY = 42_422;

function connectivityFromLastSeen(lastSeenAt: string | null): ConnectivityStatus {
  if (!lastSeenAt) {
    return "offline";
  }

  const ageMs = getNowMs() - timestampMs(lastSeenAt);

  if (ageMs >= OFFLINE_THRESHOLD_MS) {
    return "offline";
  }

  if (ageMs >= DEGRADED_THRESHOLD_MS) {
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

async function query<T extends QueryResultRow>(text: string, values: unknown[] = []) {
  if (!pool) {
    throw new Error("Database is not configured.");
  }
  return pool.query<T>(text, values);
}

<<<<<<< HEAD
=======
interface NotificationTrigger {
  eventType: NotificationEventType;
  occurredAt: string;
  subject: string;
  bodyText: string;
  nodeId: number | null;
  alertId: number | null;
}

interface DeliverableRecipient {
  id: number;
  displayName: string | null;
  emailAddress: string;
  phoneNumber: string | null;
  emailEnabled: boolean;
  smsEnabled: boolean;
}

function connectivityLossOccurredAt(lastSeenAt: string) {
  return new Date(timestampMs(lastSeenAt) + OFFLINE_THRESHOLD_MS).toISOString();
}

function connectivityLossTitle(nodeId: number) {
  return `Node ${nodeId} silent for 24 hours`;
}

function connectivityLossOpenedNote(node: NodeSummary) {
  return `PyroNet derived a connectivity loss incident because node ${node.nodeId} has not been seen since ${node.lastSeenAt}.`;
}

function connectivityLossClearedNote(nodeId: number) {
  return `Node ${nodeId} was observed again and connectivity recovered.`;
}

function isBatteryDegradationOpenValue(batteryPct: number | null | undefined) {
  return typeof batteryPct === "number" && batteryPct <= BATTERY_DEGRADATION_OPEN_THRESHOLD_PCT;
}

function isBatteryDegradationRecoveredValue(batteryPct: number | null | undefined) {
  return typeof batteryPct === "number" && batteryPct >= BATTERY_DEGRADATION_CLEAR_THRESHOLD_PCT;
}

function batteryDegradationTitle(nodeId: number) {
  return `Battery degradation detected at node ${nodeId}`;
}

function batteryDegradationOpenedNote(nodeId: number, batteryPct: number, occurredAt: string) {
  return `PyroNet opened a battery degradation incident because node ${nodeId} reported ${batteryPct}% battery at ${occurredAt}.`;
}

function batteryDegradationClearedNote(nodeId: number, batteryPct: number, recoveredAt: string) {
  return `Node ${nodeId} reported ${batteryPct}% battery at ${recoveredAt}, clearing the battery degradation incident.`;
}

type DownlinkFailureAlertType = Extract<
  NotificationEventType,
  "time_sync_failure" | "nn_update_failure" | "config_update_failure"
>;

type DownlinkFailureSourceKind = "time_sync" | "neighbor_distribution" | "config_deployment";

interface DownlinkFailureObservation {
  eventId: number;
  eventKind: DownlinkFailureSourceKind;
  alertType: DownlinkFailureAlertType;
  eventCode: "0x04" | "0x05" | "0x06";
  deviceId: number;
  nodeId: number;
  configRevisionId: number | null;
  status: "failed" | "timed_out" | "acknowledged";
  sentAt: string;
  acknowledgedAt: string | null;
  attemptNo: number;
  revisionNo: number | null;
  targetTime: string | null;
  configId: number | null;
  resultMessage: string | null;
  currentIpv6: string | null;
  currentLatitude: string;
  currentLongitude: string;
  latestReportedAt: string | null;
  latestRiskLevel: number | null;
  latestTemperatureC: string | null;
  latestHumidityPct: string | null;
  latestVocIaq: number | null;
  latestPm25UgM3: string | null;
  latestBatteryPct: number | null;
}

function downlinkFailureTypeLabel(alertType: DownlinkFailureAlertType) {
  switch (alertType) {
    case "time_sync_failure":
      return "time sync";
    case "nn_update_failure":
      return "neighbor update";
    case "config_update_failure":
      return "config update";
  }
}

function downlinkFailureTitle(alertType: DownlinkFailureAlertType, nodeId: number) {
  switch (alertType) {
    case "time_sync_failure":
      return `Time sync failure detected at node ${nodeId}`;
    case "nn_update_failure":
      return `Neighbor update failure detected at node ${nodeId}`;
    case "config_update_failure":
      return `Config update failure detected at node ${nodeId}`;
  }
}

function downlinkFailureSummary(alertType: DownlinkFailureAlertType) {
  switch (alertType) {
    case "time_sync_failure":
      return "Observed time sync command failure or timeout.";
    case "nn_update_failure":
      return "Observed neighbor table distribution failure or timeout.";
    case "config_update_failure":
      return "Observed config deployment failure or timeout.";
  }
}

function downlinkFailureOccurredAt(observation: DownlinkFailureObservation, detectedAt: string) {
  if (observation.status === "timed_out") {
    return new Date(timestampMs(observation.sentAt) + DOWNLINK_FAILURE_TIMEOUT_MS).toISOString();
  }
  return detectedAt;
}

function downlinkFailureReasonText(observation: DownlinkFailureObservation) {
  if (observation.status === "failed") {
    return observation.resultMessage?.trim() || "The command status was reported as failed.";
  }
  return `No acknowledgement was observed within ${DOWNLINK_FAILURE_TIMEOUT_MINUTES} minutes of the command being sent.`;
}

function downlinkFailureOpenedNote(observation: DownlinkFailureObservation, detectedAt: string) {
  return [
    `PyroNet opened a ${downlinkFailureTypeLabel(observation.alertType)} incident for node ${observation.nodeId}.`,
    `Command status ${observation.status} detected at ${detectedAt}.`,
    downlinkFailureReasonText(observation),
  ].join(" ");
}

function downlinkFailureClearedNote(observation: DownlinkFailureObservation) {
  return `PyroNet observed an acknowledgement for the ${downlinkFailureTypeLabel(observation.alertType)} command on node ${observation.nodeId} at ${observation.acknowledgedAt}.`;
}

function downlinkFailureBodyText(observation: DownlinkFailureObservation, occurredAt: string) {
  const details = [
    `PyroNet observed a ${observation.status} ${downlinkFailureTypeLabel(observation.alertType)} command for node ${observation.nodeId}.`,
    `Command sent at ${observation.sentAt}.`,
    `Incident occurred at ${occurredAt}.`,
    `Attempt ${observation.attemptNo}.`,
    downlinkFailureReasonText(observation),
  ];

  if (observation.revisionNo !== null && observation.alertType === "nn_update_failure") {
    details.push(`Neighbor revision ${observation.revisionNo}.`);
  }
  if (observation.targetTime && observation.alertType === "time_sync_failure") {
    details.push(`Target time ${observation.targetTime}.`);
  }
  if (observation.configId !== null && observation.alertType === "config_update_failure") {
    details.push(`Config revision ${observation.configId}.`);
  }
  details.push(`Last known location ${formatCoordinatePair(Number(observation.currentLatitude), Number(observation.currentLongitude))}.`);
  if (observation.currentIpv6) {
    details.push(`Current IPv6 ${observation.currentIpv6}.`);
  }

  return details.join(" ");
}

function downlinkFailureAlertDetails(observation: DownlinkFailureObservation, occurredAt: string) {
  return {
    derivedFrom: "downlink_status",
    sourceEventKind: observation.eventKind,
    sourceEventId: observation.eventId,
    sourcePacketCode: observation.eventCode,
    sourceStatus: observation.status,
    sentAt: observation.sentAt,
    acknowledgedAt: observation.acknowledgedAt,
    attemptNo: observation.attemptNo,
    timeoutThresholdMinutes: DOWNLINK_FAILURE_TIMEOUT_MINUTES,
    occurredAt,
    resultMessage: observation.resultMessage,
    revisionNo: observation.revisionNo,
    targetTime: observation.targetTime,
    configId: observation.configId,
    currentIpv6: observation.currentIpv6,
  };
}

interface BatteryTelemetryObservation {
  deviceId: number;
  nodeId: number;
  configRevisionId: number | null;
  sensorReadingId: number | null;
  occurredAt: string;
  detectedAt: string;
  batteryPct: number | null;
  riskLevel: number;
  temperatureC: number;
  humidityPct: number;
  vocIaq: number;
  pm25UgM3: number;
  locationLabel: string;
  ipv6Address: string | null;
}

async function reconcileBatteryDegradationAlertsInDb(
  client: PoolClient,
  observation: BatteryTelemetryObservation,
): Promise<NotificationTrigger[]> {
  const activeAlerts = await client.query<{ id: number; status: "open" | "acknowledged" }>(
    `
      SELECT id, status
      FROM alerts
      WHERE device_id = $1
        AND alert_type = 'battery_degradation'
        AND status <> 'cleared'
    `,
    [observation.deviceId],
  );

  if (observation.batteryPct === null) {
    return [];
  }

  if (isBatteryDegradationOpenValue(observation.batteryPct)) {
    if (activeAlerts.rows.length > 0) {
      return [];
    }

    const details = {
      derivedFrom: "battery_pct",
      batteryPct: observation.batteryPct,
      openThresholdPct: BATTERY_DEGRADATION_OPEN_THRESHOLD_PCT,
      clearThresholdPct: BATTERY_DEGRADATION_CLEAR_THRESHOLD_PCT,
      currentIpv6: observation.ipv6Address,
    };
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
        VALUES (
          $1,
          $2,
          $3,
          'battery_degradation',
          'open',
          'warning',
          $4,
          $5,
          $6,
          $7,
          $7,
          $6,
          $8,
          $9,
          $10,
          $11,
          $12,
          $13
        )
        RETURNING id
      `,
      [
        observation.deviceId,
        observation.sensorReadingId,
        observation.configRevisionId,
        batteryDegradationTitle(observation.nodeId),
        details,
        observation.occurredAt,
        observation.detectedAt,
        observation.riskLevel,
        observation.temperatureC,
        observation.humidityPct,
        observation.vocIaq,
        observation.pm25UgM3,
        observation.batteryPct,
      ],
    );
    const alertId = insertedAlert.rows[0]?.id;
    if (!alertId) {
      throw new Error(`Unable to create battery degradation alert for node ${observation.nodeId}.`);
    }

    await client.query(
      `
        INSERT INTO alert_events (alert_id, event_type, new_status, event_at, actor, note, details)
        VALUES ($1, 'opened', 'open', $2, 'system', $3, $4)
      `,
      [
        alertId,
        observation.detectedAt,
        batteryDegradationOpenedNote(observation.nodeId, observation.batteryPct, observation.occurredAt),
        details,
      ],
    );

    return [
      {
        eventType: "battery_degradation",
        occurredAt: observation.occurredAt,
        subject: batteryDegradationTitle(observation.nodeId),
        bodyText: [
          `PyroNet observed ${observation.batteryPct}% battery from node ${observation.nodeId} at ${observation.occurredAt}.`,
          `The battery degradation threshold is ${BATTERY_DEGRADATION_OPEN_THRESHOLD_PCT}% and the clear threshold is ${BATTERY_DEGRADATION_CLEAR_THRESHOLD_PCT}%.`,
          `Last known location ${observation.locationLabel}.`,
          observation.ipv6Address ? `Current IPv6 ${observation.ipv6Address}.` : null,
        ]
          .filter(Boolean)
          .join(" "),
        nodeId: observation.nodeId,
        alertId,
      },
    ];
  }

  if (!isBatteryDegradationRecoveredValue(observation.batteryPct) || activeAlerts.rows.length === 0) {
    return [];
  }

  for (const alert of activeAlerts.rows) {
    await client.query(
      `
        UPDATE alerts
        SET
          status = 'cleared',
          cleared_at = $2,
          cleared_by = 'system',
          latest_event_at = $2
        WHERE id = $1
      `,
      [alert.id, observation.detectedAt],
    );

    await client.query(
      `
        INSERT INTO alert_events (alert_id, event_type, previous_status, new_status, event_at, actor, note, details)
        VALUES ($1, 'cleared', $2, 'cleared', $3, 'system', $4, $5)
      `,
      [
        alert.id,
        alert.status,
        observation.detectedAt,
        batteryDegradationClearedNote(observation.nodeId, observation.batteryPct, observation.occurredAt),
        {
          recoveredAt: observation.occurredAt,
          recoveredBy: "sensor_reading",
          batteryPct: observation.batteryPct,
          clearThresholdPct: BATTERY_DEGRADATION_CLEAR_THRESHOLD_PCT,
        },
      ],
    );
  }

  return [];
}

async function clearRecoveredConnectivityLossAlertsInDb(
  client: PoolClient,
  deviceId: number,
  nodeId: number,
  recoveredAt: string,
) {
  if (connectivityFromLastSeen(recoveredAt) === "offline") {
    return;
  }

  const activeAlerts = await client.query<{ id: number; status: "open" | "acknowledged" }>(
    `
      SELECT id, status
      FROM alerts
      WHERE device_id = $1
        AND alert_type = 'connectivity_loss'
        AND status <> 'cleared'
    `,
    [deviceId],
  );

  for (const alert of activeAlerts.rows) {
    await client.query(
      `
        UPDATE alerts
        SET
          status = 'cleared',
          cleared_at = $2,
          cleared_by = 'system',
          latest_event_at = $2
        WHERE id = $1
      `,
      [alert.id, recoveredAt],
    );

    await client.query(
      `
        INSERT INTO alert_events (alert_id, event_type, previous_status, new_status, event_at, actor, note, details)
        VALUES ($1, 'cleared', $2, 'cleared', $3, 'system', $4, $5)
      `,
      [
        alert.id,
        alert.status,
        recoveredAt,
        connectivityLossClearedNote(nodeId),
        {
          recoveredAt,
          recoveredBy: "device_observation",
        },
      ],
    );
  }
}

async function syncConnectivityLossAlertsInDb(nodes?: NodeSummary[]) {
  if (!pool) {
    return;
  }

  const fleet = nodes ?? (await queryNodesFromDb());
  const offlineNodes = fleet.filter((node) => node.lastSeenAt && connectivityFromLastSeen(node.lastSeenAt) === "offline");
  const offlineNodesById = new Map(offlineNodes.map((node) => [node.nodeId, node]));
  const detectedAt = new Date().toISOString();
  const client = await pool.connect();
  const triggers: NotificationTrigger[] = [];

  try {
    await client.query("BEGIN");

    const activeAlerts = await client.query<{
      id: number;
      device_id: number;
      node_id: number;
      status: "open" | "acknowledged";
    }>(
      `
        SELECT
          a.id,
          a.device_id,
          d.node_id,
          a.status
        FROM alerts a
        JOIN devices d ON d.id = a.device_id
        WHERE a.alert_type = 'connectivity_loss'
          AND a.status <> 'cleared'
      `,
    );

    const activeAlertsByNodeId = new Map<number, (typeof activeAlerts.rows)[number]>();
    for (const alert of activeAlerts.rows) {
      if (!activeAlertsByNodeId.has(alert.node_id)) {
        activeAlertsByNodeId.set(alert.node_id, alert);
      }
    }

    for (const node of offlineNodes) {
      if (!node.lastSeenAt || activeAlertsByNodeId.has(node.nodeId)) {
        continue;
      }

      const occurredAt = connectivityLossOccurredAt(node.lastSeenAt);
      const details = {
        derivedFrom: "last_seen_at",
        lastSeenAt: node.lastSeenAt,
        offlineThresholdMs: OFFLINE_THRESHOLD_MS,
        currentIpv6: node.ipv6Address,
      };
      const insertedAlert = await client.query<{ id: number }>(
        `
          INSERT INTO alerts (
            device_id,
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
          VALUES (
            $1,
            $2,
            'connectivity_loss',
            'open',
            'warning',
            $3,
            $4,
            $5,
            $6,
            $6,
            $7,
            $8,
            $9,
            $10,
            $11,
            $12,
            $13
          )
          RETURNING id
        `,
        [
          Number(node.id),
          node.activeConfigRevisionId,
          connectivityLossTitle(node.nodeId),
          details,
          occurredAt,
          detectedAt,
          node.latestTelemetry?.reportedAt ?? null,
          node.latestTelemetry?.riskLevel ?? null,
          node.latestTelemetry?.temperatureC ?? null,
          node.latestTelemetry?.humidityPct ?? null,
          node.latestTelemetry?.vocIaq ?? null,
          node.latestTelemetry?.pm25UgM3 ?? null,
          node.latestTelemetry?.batteryPct ?? null,
        ],
      );
      const alertId = insertedAlert.rows[0]?.id;
      if (!alertId) {
        throw new Error(`Unable to create connectivity-loss alert for node ${node.nodeId}.`);
      }

      await client.query(
        `
          INSERT INTO alert_events (alert_id, event_type, new_status, event_at, actor, note, details)
          VALUES ($1, 'opened', 'open', $2, 'system', $3, $4)
        `,
        [alertId, detectedAt, connectivityLossOpenedNote(node), details],
      );

      triggers.push({
        eventType: "connectivity_loss",
        occurredAt,
        subject: `Connectivity loss detected at node ${node.nodeId}`,
        bodyText: [
          `PyroNet has not observed node ${node.nodeId} since ${node.lastSeenAt}.`,
          `The node crossed the 24-hour offline threshold at ${occurredAt}.`,
          `Last known location ${node.location.label}.`,
          node.ipv6Address ? `Last known IPv6 ${node.ipv6Address}.` : null,
        ]
          .filter(Boolean)
          .join(" "),
        nodeId: node.nodeId,
        alertId,
      });
    }

    for (const alert of activeAlerts.rows) {
      if (offlineNodesById.has(alert.node_id)) {
        continue;
      }

      await client.query(
        `
          UPDATE alerts
          SET
            status = 'cleared',
            cleared_at = $2,
            cleared_by = 'system',
            latest_event_at = $2
          WHERE id = $1
        `,
        [alert.id, detectedAt],
      );

      await client.query(
        `
          INSERT INTO alert_events (alert_id, event_type, previous_status, new_status, event_at, actor, note, details)
          VALUES ($1, 'cleared', $2, 'cleared', $3, 'system', $4, $5)
        `,
        [
          alert.id,
          alert.status,
          detectedAt,
          connectivityLossClearedNote(alert.node_id),
          {
            clearedAt: detectedAt,
            clearedBy: "connectivity_evaluation",
          },
        ],
      );
    }

    await client.query("COMMIT");
  } catch (error) {
    await client.query("ROLLBACK");
    throw error;
  } finally {
    client.release();
  }

  for (const trigger of triggers) {
    await dispatchNotifications(trigger);
  }
}

async function syncDownlinkFailureAlertsInDb() {
  if (!pool) {
    return;
  }

  const detectedAt = new Date().toISOString();
  const timeoutCutoff = new Date(timestampMs(detectedAt) - DOWNLINK_FAILURE_TIMEOUT_MS).toISOString();
  const client = await pool.connect();
  const triggers: NotificationTrigger[] = [];

  try {
    await client.query("BEGIN");
    await client.query("SELECT pg_advisory_xact_lock($1)", [DOWNLINK_FAILURE_SYNC_LOCK_KEY]);

    await client.query(
      `
        UPDATE nn_distribution_events
        SET status = 'timed_out'
        WHERE status IN ('pending', 'sent')
          AND acknowledged_at IS NULL
          AND sent_at <= $1
      `,
      [timeoutCutoff],
    );

    await client.query(
      `
        UPDATE time_sync_events
        SET
          status = 'timed_out',
          result_message = COALESCE(
            NULLIF(result_message, ''),
            $2
          )
        WHERE status IN ('pending', 'sent')
          AND acknowledged_at IS NULL
          AND sent_at <= $1
      `,
      [timeoutCutoff, `No acknowledgement observed within ${DOWNLINK_FAILURE_TIMEOUT_MINUTES} minutes.`],
    );

    await client.query(
      `
        UPDATE device_config_deployments
        SET status = 'timed_out'
        WHERE status IN ('pending', 'sent')
          AND acknowledged_at IS NULL
          AND sent_at <= $1
      `,
      [timeoutCutoff],
    );

    const activeAlerts = await client.query<{
      id: number;
      alert_type: DownlinkFailureAlertType;
      status: "open" | "acknowledged";
      sourceEventId: string | null;
    }>(
      `
        SELECT
          id,
          alert_type,
          status,
          details->>'sourceEventId' AS "sourceEventId"
        FROM alerts
        WHERE alert_type IN ('time_sync_failure', 'nn_update_failure', 'config_update_failure')
          AND status <> 'cleared'
      `,
    );

    const activeAlertsByEvent = new Map<string, (typeof activeAlerts.rows)[number]>();
    for (const alert of activeAlerts.rows) {
      if (!alert.sourceEventId) {
        continue;
      }
      activeAlertsByEvent.set(`${alert.alert_type}:${alert.sourceEventId}`, alert);
    }

    const observations = await client.query<DownlinkFailureObservation>(
      `
        SELECT
          observation."eventId",
          observation."eventKind",
          observation."alertType",
          observation."eventCode",
          observation."deviceId",
          observation."nodeId",
          observation."configRevisionId",
          observation.status,
          observation."sentAt",
          observation."acknowledgedAt",
          observation."attemptNo",
          observation."revisionNo",
          observation."targetTime",
          observation."configId",
          observation."resultMessage",
          observation."currentIpv6",
          observation."currentLatitude",
          observation."currentLongitude",
          observation."latestReportedAt",
          observation."latestRiskLevel",
          observation."latestTemperatureC",
          observation."latestHumidityPct",
          observation."latestVocIaq",
          observation."latestPm25UgM3",
          observation."latestBatteryPct"
        FROM (
          SELECT
            nde.id AS "eventId",
            'neighbor_distribution'::text AS "eventKind",
            'nn_update_failure'::text AS "alertType",
            '0x04'::text AS "eventCode",
            d.id AS "deviceId",
            d.node_id AS "nodeId",
            d.current_config_revision_id AS "configRevisionId",
            nde.status::text AS status,
            nde.sent_at::text AS "sentAt",
            nde.acknowledged_at::text AS "acknowledgedAt",
            nde.attempt_no AS "attemptNo",
            nn.revision_no AS "revisionNo",
            NULL::text AS "targetTime",
            NULL::bigint AS "configId",
            NULL::text AS "resultMessage",
            host(d.current_ipv6) AS "currentIpv6",
            d.current_latitude::text AS "currentLatitude",
            d.current_longitude::text AS "currentLongitude",
            d.latest_reported_at::text AS "latestReportedAt",
            d.latest_risk_level AS "latestRiskLevel",
            d.latest_temperature_c::text AS "latestTemperatureC",
            d.latest_humidity_pct::text AS "latestHumidityPct",
            d.latest_voc_iaq AS "latestVocIaq",
            d.latest_pm25_ug_m3::text AS "latestPm25UgM3",
            d.latest_battery_pct AS "latestBatteryPct"
          FROM nn_distribution_events nde
          JOIN devices d ON d.id = nde.device_id
          JOIN nn_revisions nn ON nn.id = nde.nn_revision_id

          UNION ALL

          SELECT
            tse.id AS "eventId",
            'time_sync'::text AS "eventKind",
            'time_sync_failure'::text AS "alertType",
            '0x05'::text AS "eventCode",
            d.id AS "deviceId",
            d.node_id AS "nodeId",
            d.current_config_revision_id AS "configRevisionId",
            tse.status::text AS status,
            tse.sent_at::text AS "sentAt",
            tse.acknowledged_at::text AS "acknowledgedAt",
            1 AS "attemptNo",
            NULL::integer AS "revisionNo",
            tse.target_time::text AS "targetTime",
            NULL::bigint AS "configId",
            tse.result_message AS "resultMessage",
            host(d.current_ipv6) AS "currentIpv6",
            d.current_latitude::text AS "currentLatitude",
            d.current_longitude::text AS "currentLongitude",
            d.latest_reported_at::text AS "latestReportedAt",
            d.latest_risk_level AS "latestRiskLevel",
            d.latest_temperature_c::text AS "latestTemperatureC",
            d.latest_humidity_pct::text AS "latestHumidityPct",
            d.latest_voc_iaq AS "latestVocIaq",
            d.latest_pm25_ug_m3::text AS "latestPm25UgM3",
            d.latest_battery_pct AS "latestBatteryPct"
          FROM time_sync_events tse
          JOIN devices d ON d.id = tse.device_id

          UNION ALL

          SELECT
            dcd.id AS "eventId",
            'config_deployment'::text AS "eventKind",
            'config_update_failure'::text AS "alertType",
            '0x06'::text AS "eventCode",
            d.id AS "deviceId",
            d.node_id AS "nodeId",
            dcd.config_revision_id AS "configRevisionId",
            dcd.status::text AS status,
            dcd.sent_at::text AS "sentAt",
            dcd.acknowledged_at::text AS "acknowledgedAt",
            dcd.attempt_no AS "attemptNo",
            NULL::integer AS "revisionNo",
            NULL::text AS "targetTime",
            cr.config_id AS "configId",
            NULL::text AS "resultMessage",
            host(d.current_ipv6) AS "currentIpv6",
            d.current_latitude::text AS "currentLatitude",
            d.current_longitude::text AS "currentLongitude",
            d.latest_reported_at::text AS "latestReportedAt",
            d.latest_risk_level AS "latestRiskLevel",
            d.latest_temperature_c::text AS "latestTemperatureC",
            d.latest_humidity_pct::text AS "latestHumidityPct",
            d.latest_voc_iaq AS "latestVocIaq",
            d.latest_pm25_ug_m3::text AS "latestPm25UgM3",
            d.latest_battery_pct AS "latestBatteryPct"
          FROM device_config_deployments dcd
          JOIN devices d ON d.id = dcd.device_id
          JOIN config_revisions cr ON cr.id = dcd.config_revision_id
        ) AS observation
        WHERE observation.status IN ('failed', 'timed_out', 'acknowledged')
        ORDER BY observation."sentAt" ASC, observation."eventId" ASC
      `,
    );

    for (const observation of observations.rows) {
      const key = `${observation.alertType}:${observation.eventId}`;
      const activeAlert = activeAlertsByEvent.get(key);

      if (observation.status === "acknowledged") {
        if (!activeAlert) {
          continue;
        }

        await client.query(
          `
            UPDATE alerts
            SET
              status = 'cleared',
              cleared_at = $2,
              cleared_by = 'system',
              latest_event_at = $2
            WHERE id = $1
          `,
          [activeAlert.id, observation.acknowledgedAt ?? detectedAt],
        );

        await client.query(
          `
            INSERT INTO alert_events (alert_id, event_type, previous_status, new_status, event_at, actor, note, details)
            VALUES ($1, 'cleared', $2, 'cleared', $3, 'system', $4, $5)
          `,
          [
            activeAlert.id,
            activeAlert.status,
            observation.acknowledgedAt ?? detectedAt,
            downlinkFailureClearedNote(observation),
            {
              acknowledgedAt: observation.acknowledgedAt ?? detectedAt,
              sourceEventId: observation.eventId,
              sourceEventKind: observation.eventKind,
            },
          ],
        );

        activeAlertsByEvent.delete(key);
        continue;
      }

      if (activeAlert) {
        continue;
      }

      const occurredAt = downlinkFailureOccurredAt(observation, detectedAt);
      const details = downlinkFailureAlertDetails(observation, occurredAt);
      const insertedAlert = await client.query<{ id: number }>(
        `
          INSERT INTO alerts (
            device_id,
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
          VALUES (
            $1,
            $2,
            $3,
            'open',
            'warning',
            $4,
            $5,
            $6,
            $7,
            $7,
            $8,
            $9,
            $10,
            $11,
            $12,
            $13,
            $14
          )
          RETURNING id
        `,
        [
          observation.deviceId,
          observation.configRevisionId,
          observation.alertType,
          downlinkFailureTitle(observation.alertType, observation.nodeId),
          details,
          occurredAt,
          detectedAt,
          observation.latestReportedAt,
          observation.latestRiskLevel,
          observation.latestTemperatureC,
          observation.latestHumidityPct,
          observation.latestVocIaq,
          observation.latestPm25UgM3,
          observation.latestBatteryPct,
        ],
      );

      const alertId = insertedAlert.rows[0]?.id;
      if (!alertId) {
        throw new Error(`Unable to create ${observation.alertType} alert for node ${observation.nodeId}.`);
      }

      await client.query(
        `
          INSERT INTO alert_events (alert_id, event_type, new_status, event_at, actor, note, details)
          VALUES ($1, 'opened', 'open', $2, 'system', $3, $4)
        `,
        [alertId, detectedAt, downlinkFailureOpenedNote(observation, detectedAt), details],
      );

      activeAlertsByEvent.set(key, {
        id: alertId,
        alert_type: observation.alertType,
        status: "open",
        sourceEventId: String(observation.eventId),
      });
      triggers.push({
        eventType: observation.alertType,
        occurredAt,
        subject: downlinkFailureTitle(observation.alertType, observation.nodeId),
        bodyText: downlinkFailureBodyText(observation, occurredAt),
        nodeId: observation.nodeId,
        alertId,
      });
    }

    await client.query("COMMIT");
  } catch (error) {
    await client.query("ROLLBACK");
    throw error;
  } finally {
    client.release();
  }

  for (const trigger of triggers) {
    await dispatchNotifications(trigger);
  }
}

function normalizePhoneNumber(value: string | null | undefined) {
  if (!value) {
    return null;
  }

  const normalized = value.trim();
  return /^\+[1-9][0-9]{7,14}$/.test(normalized) ? normalized : null;
}

async function queueDeliveryRecord(
  recipient: DeliverableRecipient,
  trigger: NotificationTrigger,
  channel: NotificationChannel,
  destination: string,
  status: NotificationSettingsResponse["deliveries"][number]["status"],
  failureReason: string | null = null,
) {
  return query<{ id: number }>(
    `
      INSERT INTO notification_deliveries (
        recipient_id,
        event_type,
        channel,
        destination,
        alert_id,
        device_id,
        event_occurred_at,
        subject,
        status,
        attempted_at,
        delivered_at,
        failure_reason,
        payload_snapshot
      )
      VALUES (
        $1,
        $2,
        $3,
        $4,
        $5,
        (SELECT id FROM devices WHERE node_id = $6),
        $7,
        $8,
        $9::notification_delivery_status,
        CASE WHEN $9::notification_delivery_status IN ('accepted', 'sent', 'failed') THEN NOW() ELSE NULL END,
        NULL,
        $10,
        $11
      )
      RETURNING id
    `,
    [
      recipient.id,
      trigger.eventType,
      channel,
      destination,
      trigger.alertId,
      trigger.nodeId,
      trigger.occurredAt,
      trigger.subject,
      status,
      failureReason,
      {
        eventType: trigger.eventType,
        bodyText: trigger.bodyText,
        recipientName: recipient.displayName,
      },
    ],
  );
}

async function sendSmsWithTwilio(to: string, bodyText: string) {
  if (!twilioAccountSid || !twilioAuthToken || (!twilioMessagingServiceSid && !twilioFromNumber)) {
    throw new Error("Twilio is not configured.");
  }

  const params = new URLSearchParams({
    To: to,
    Body: bodyText,
  });

  if (twilioMessagingServiceSid) {
    params.set("MessagingServiceSid", twilioMessagingServiceSid);
  } else if (twilioFromNumber) {
    params.set("From", twilioFromNumber);
  }

  const response = await fetch(`https://api.twilio.com/2010-04-01/Accounts/${twilioAccountSid}/Messages.json`, {
    method: "POST",
    headers: {
      Authorization: `Basic ${Buffer.from(`${twilioAccountSid}:${twilioAuthToken}`).toString("base64")}`,
      "Content-Type": "application/x-www-form-urlencoded;charset=UTF-8",
    },
    body: params.toString(),
  });

  const payload = (await response.json()) as { sid?: string; message?: string };
  if (!response.ok) {
    throw new Error(payload.message ?? `Twilio request failed with ${response.status}`);
  }

  return payload.sid ?? null;
}

async function deliverNotificationChannel(
  recipient: DeliverableRecipient,
  trigger: NotificationTrigger,
  channel: NotificationChannel,
) {
  const destination = channel === "email" ? recipient.emailAddress : normalizePhoneNumber(recipient.phoneNumber);
  if (!destination) {
    await queueDeliveryRecord(recipient, trigger, channel, channel === "email" ? recipient.emailAddress : "sms-unconfigured", "skipped", `Recipient has no ${channel} destination configured.`);
    return;
  }

  const queuedDelivery = await queueDeliveryRecord(recipient, trigger, channel, destination, "queued");
  const deliveryId = queuedDelivery.rows[0]?.id;
  if (!deliveryId) {
    throw new Error("Unable to create notification delivery record.");
  }

  try {
    const providerMessageId =
      channel === "email"
        ? (
            await sendEmail({
              to: destination,
              subject: trigger.subject,
              text: trigger.bodyText,
            })
          ).providerMessageId
        : await sendSmsWithTwilio(destination, trigger.bodyText);

    await query(
      `
        UPDATE notification_deliveries
        SET
          status = 'accepted',
          attempted_at = NOW(),
          delivered_at = NULL,
          provider_message_id = $2,
          failure_reason = NULL
        WHERE id = $1
      `,
      [deliveryId, providerMessageId],
    );
    console.info("[notifications] Provider accepted notification", {
      channel,
      deliveryId,
      destination,
      eventType: trigger.eventType,
      providerMessageId,
      recipientId: recipient.id,
    });
  } catch (error) {
    const failureReason = error instanceof Error ? error.message : "Unexpected provider error";
    await query(
      `
        UPDATE notification_deliveries
        SET
          status = 'failed',
          attempted_at = NOW(),
          failure_reason = $2
        WHERE id = $1
      `,
      [deliveryId, failureReason],
    );
    console.error("[notifications] Notification send failed", {
      channel,
      deliveryId,
      destination,
      eventType: trigger.eventType,
      failureReason,
      recipientId: recipient.id,
    });
  }
}

async function dispatchNotifications(trigger: NotificationTrigger) {
  if (!pool) {
    return;
  }

  const recipientsResult = await query<{
    id: number;
    display_name: string | null;
    email_address: string;
    phone_number: string | null;
    email_enabled: boolean | null;
    sms_enabled: boolean | null;
  }>(
    `
      SELECT
        recipient.id,
        recipient.display_name,
        recipient.email_address,
        recipient.phone_number,
        preference.email_enabled,
        preference.sms_enabled
      FROM notification_recipients recipient
      JOIN notification_preferences preference
        ON preference.recipient_id = recipient.id
       AND preference.event_type = $1
      WHERE recipient.is_enabled = TRUE
    `,
    [trigger.eventType],
  );

  for (const recipient of recipientsResult.rows) {
    const deliverableRecipient: DeliverableRecipient = {
      id: recipient.id,
      displayName: recipient.display_name,
      emailAddress: recipient.email_address,
      phoneNumber: recipient.phone_number,
      emailEnabled: recipient.email_enabled ?? false,
      smsEnabled: recipient.sms_enabled ?? false,
    };

    if (deliverableRecipient.emailEnabled) {
      await deliverNotificationChannel(deliverableRecipient, trigger, "email");
    }
    if (deliverableRecipient.smsEnabled) {
      await deliverNotificationChannel(deliverableRecipient, trigger, "sms");
    }
  }
}

function packetOccurredAt(packet: DecodedPacket, acceptedAt: string) {
  switch (packet.packetCode) {
    case "0x01":
    case "0x04":
    case "0x05":
    case "0x06":
      return acceptedAt;
    case "0x02":
    case "0x03":
    case "0x07":
    case "0x08":
      return packet.occurredAt;
  }
}

function buildPacketMetadata(packet: DecodedPacket, acceptedAt: string) {
  const base = {
    packetCode: packet.packetCode,
    eventType: packet.eventType,
    direction: packet.direction,
    version: packet.version,
    payloadHex: packet.payloadHex,
    payloadSizeBytes: packet.payloadSizeBytes,
    acceptedAt,
  };

  switch (packet.packetCode) {
    case "0x01":
      return {
        ...base,
        sourceIpv6: packet.sourceIpv6,
        parentIpv6: packet.parentIpv6,
      };
    case "0x02":
    case "0x03":
      return {
        ...base,
        reportedAt: packet.occurredAt,
      };
    case "0x04":
      return {
        ...base,
        neighborIpv6Addresses: packet.neighborIpv6Addresses,
      };
    case "0x05":
      return {
        ...base,
        targetTime: packet.occurredAt,
      };
    case "0x06":
      return {
        ...base,
        configId: packet.configId,
      };
    case "0x07":
      return {
        ...base,
        targetNodeId: packet.targetNodeId,
      };
    case "0x08":
      return {
        ...base,
        parentIpv6: packet.parentIpv6,
      };
  }
}

function buildPacketSummary(packet: DecodedPacket) {
  switch (packet.packetCode) {
    case "0x01":
      return {
        summary: `Registration received from node ${packet.nodeId}.`,
        detail: [packet.sourceIpv6, `FW ${packet.firmwareVersion}`, `Battery ${packet.batteryPct}%`].join(" · "),
      };
    case "0x02":
      return {
        summary: `Periodic report received from node ${packet.nodeId}.`,
        detail: [
          `Risk ${packet.riskLevel}`,
          `${packet.temperatureC.toFixed(1)}°C`,
          `${packet.humidityPct.toFixed(0)}% RH`,
          `VOC ${packet.vocIaq}`,
          `PM2.5 ${packet.pm25UgM3.toFixed(1)}`,
        ].join(" · "),
      };
    case "0x03":
      return {
        summary: `Critical alert uplink from node ${packet.nodeId}.`,
        detail: [
          `Risk ${packet.riskLevel}`,
          `${packet.temperatureC.toFixed(1)}°C`,
          `${packet.humidityPct.toFixed(0)}% RH`,
          `VOC ${packet.vocIaq}`,
          `PM2.5 ${packet.pm25UgM3.toFixed(1)}`,
        ].join(" · "),
      };
    case "0x04":
      return {
        summary: `Neighbor table distribution sent to node ${packet.nodeId}.`,
        detail: `${packet.neighborIpv6Addresses.length} neighbors`,
      };
    case "0x05":
      return {
        summary: `Time sync sent to node ${packet.nodeId}.`,
        detail: `Target ${packet.occurredAt}`,
      };
    case "0x06":
      return {
        summary: `Threshold revision deployment sent to node ${packet.nodeId}.`,
        detail: `Revision ${packet.configId}`,
      };
    case "0x07":
      return {
        summary: `Neighbor alert forwarded from node ${packet.nodeId}.`,
        detail:
          packet.targetNodeId === null
            ? `Risk ${packet.riskLevel}`
            : `Risk ${packet.riskLevel} · Target ${packet.targetNodeId}`,
      };
    case "0x08":
      return {
        summary: `Parent update received from node ${packet.nodeId}.`,
        detail: packet.parentIpv6 ? `Preferred parent ${packet.parentIpv6}` : "Preferred parent none",
      };
  }
}

function buildPacketIngestResponse(
  packet: DecodedPacket,
  acceptedAt: string,
  storage: PacketIngestResponse["storage"],
): PacketIngestResponse {
  const { summary, detail } = buildPacketSummary(packet);
  return {
    storage,
    packetCode: packet.packetCode,
    eventType: packet.eventType,
    direction: packet.direction,
    nodeId: packet.nodeId,
    version: packet.version,
    acceptedAt,
    occurredAt: packetOccurredAt(packet, acceptedAt),
    summary,
    detail,
  };
}

interface DeviceIdentityRow {
  id: number;
  node_id: number;
  current_ipv6: string | null;
  current_latitude: string;
  current_longitude: string;
  current_config_revision_id: number | null;
}

async function getDeviceByNodeIdOrThrow(client: PoolClient, nodeId: NodeId) {
  const result = await client.query<DeviceIdentityRow>(
    `
      SELECT
        id,
        node_id,
        host(current_ipv6) AS current_ipv6,
        current_latitude::text,
        current_longitude::text,
        current_config_revision_id
      FROM devices
      WHERE node_id = $1
    `,
    [nodeId],
  );

  const device = result.rows[0];
  if (!device) {
    throw new Error(`Unknown node ${nodeId}`);
  }

  return device;
}

async function getDeviceByIpv6(client: PoolClient, ipv6Address: string) {
  const result = await client.query<DeviceIdentityRow>(
    `
      SELECT
        id,
        node_id,
        host(current_ipv6) AS current_ipv6,
        current_latitude::text,
        current_longitude::text,
        current_config_revision_id
      FROM devices
      WHERE current_ipv6 = $1::inet
    `,
    [ipv6Address],
  );

  return result.rows[0] ?? null;
}

async function ingestRegistrationPacketInDb(
  client: PoolClient,
  packet: Extract<DecodedPacket, { packetCode: "0x01" }>,
  acceptedAt: string,
) {
  const existing = await client.query<{ id: number; current_ipv6: string | null }>(
    "SELECT id, host(current_ipv6) AS current_ipv6 FROM devices WHERE node_id = $1",
    [packet.nodeId],
  );

  let deviceId = existing.rows[0]?.id ?? null;
  const isFirstRegistration = deviceId === null;

  if (deviceId === null) {
    const inserted = await client.query<{ id: number }>(
      `
        INSERT INTO devices (
          node_id,
          current_ipv6,
          current_latitude,
          current_longitude,
          current_firmware_version,
          first_registered_at,
          last_registered_at,
          last_seen_at,
          current_config_revision_id
        )
        VALUES (
          $1,
          $2,
          $3,
          $4,
          $5,
          $6,
          $6,
          $6,
          (
            SELECT id
            FROM config_revisions
            WHERE retired_at IS NULL
            ORDER BY created_at DESC
            LIMIT 1
          )
        )
        RETURNING id
      `,
      [packet.nodeId, packet.sourceIpv6, packet.latitude, packet.longitude, packet.firmwareVersion, acceptedAt],
    );
    deviceId = inserted.rows[0]?.id ?? null;
  } else {
    await client.query(
      `
        UPDATE devices
        SET
          current_ipv6 = $2::inet,
          current_latitude = $3,
          current_longitude = $4,
          current_firmware_version = $5,
          last_registered_at = $6,
          last_seen_at = $6
        WHERE id = $1
      `,
      [deviceId, packet.sourceIpv6, packet.latitude, packet.longitude, packet.firmwareVersion, acceptedAt],
    );
  }

  if (!deviceId) {
    throw new Error("Unable to upsert device from registration packet.");
  }

  await client.query(
    `
      INSERT INTO device_registrations (
        device_id,
        observed_ipv6,
        latitude,
        longitude,
        firmware_version,
        battery_pct,
        preferred_parent_ipv6,
        observed_at,
        ingested_at,
        raw_payload_metadata
      )
      VALUES ($1, $2::inet, $3, $4, $5, $6, $7::inet, $8, $9, $10)
    `,
    [
      deviceId,
      packet.sourceIpv6,
      packet.latitude,
      packet.longitude,
      packet.firmwareVersion,
      packet.batteryPct,
      packet.parentIpv6,
      acceptedAt,
      acceptedAt,
      buildPacketMetadata(packet, acceptedAt),
    ],
  );

  const openIpv6History = await client.query<{ id: number; ipv6_address: string }>(
    `
      SELECT id, host(ipv6_address) AS ipv6_address
      FROM device_ipv6_history
      WHERE device_id = $1
        AND valid_to IS NULL
      ORDER BY valid_from DESC
      LIMIT 1
    `,
    [deviceId],
  );

  const currentIpv6 = openIpv6History.rows[0];
  if (!currentIpv6 || currentIpv6.ipv6_address !== packet.sourceIpv6) {
    if (currentIpv6) {
      await client.query("UPDATE device_ipv6_history SET valid_to = $2 WHERE id = $1", [currentIpv6.id, acceptedAt]);
    }

    await client.query(
      `
        INSERT INTO device_ipv6_history (device_id, ipv6_address, valid_from)
        VALUES ($1, $2::inet, $3)
      `,
      [deviceId, packet.sourceIpv6, acceptedAt],
    );
  }

  await clearRecoveredConnectivityLossAlertsInDb(client, deviceId, packet.nodeId, acceptedAt);

  const actionLabel = isFirstRegistration ? "joined" : "rejoined";
  const registrationKind = isFirstRegistration ? "new node registration" : "node rejoin";

  return [
    {
      eventType: "node_registration" as const,
      occurredAt: acceptedAt,
      subject: `Node ${packet.nodeId} ${actionLabel} the network`,
      bodyText: [
        `PyroNet recorded a ${registrationKind} for node ${packet.nodeId}.`,
        `IPv6 ${packet.sourceIpv6}.`,
        `Firmware ${packet.firmwareVersion}.`,
        `Battery ${packet.batteryPct}%.`,
        `Location ${packet.latitude.toFixed(5)}, ${packet.longitude.toFixed(5)}.`,
        packet.parentIpv6 ? `Preferred parent ${packet.parentIpv6}.` : "Preferred parent none.",
      ].join(" "),
      nodeId: packet.nodeId,
      alertId: null,
    },
  ];
}

async function ingestSensorPacketInDb(
  client: PoolClient,
  packet: Extract<DecodedPacket, { packetCode: "0x02" | "0x03" }>,
  acceptedAt: string,
): Promise<NotificationTrigger[]> {
  const device = await getDeviceByNodeIdOrThrow(client, packet.nodeId);
  const triggers: NotificationTrigger[] = [];
  const insertedReading = await client.query<{ id: number }>(
    `
      INSERT INTO sensor_readings (
        device_id,
        source_type,
        reported_at,
        ingested_at,
        risk_level,
        temperature_c,
        humidity_pct,
        voc_iaq,
        pm25_ug_m3,
        battery_pct,
        config_revision_id,
        raw_payload_metadata
      )
      VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12)
      RETURNING id
    `,
    [
      device.id,
      packet.eventType,
      packet.occurredAt,
      acceptedAt,
      packet.riskLevel,
      packet.temperatureC,
      packet.humidityPct,
      packet.vocIaq,
      packet.pm25UgM3,
      packet.batteryPct,
      device.current_config_revision_id,
      buildPacketMetadata(packet, acceptedAt),
    ],
  );

  await client.query(
    `
      UPDATE devices
      SET
        last_seen_at = $2,
        latest_reported_at = $2,
        latest_risk_level = $3,
        latest_temperature_c = $4,
        latest_humidity_pct = $5,
        latest_voc_iaq = $6,
        latest_pm25_ug_m3 = $7,
        latest_battery_pct = $8
      WHERE id = $1
    `,
    [
      device.id,
      packet.occurredAt,
      packet.riskLevel,
      packet.temperatureC,
      packet.humidityPct,
      packet.vocIaq,
      packet.pm25UgM3,
      packet.batteryPct,
    ],
  );

  await clearRecoveredConnectivityLossAlertsInDb(client, device.id, packet.nodeId, packet.occurredAt);
  triggers.push(
    ...(await reconcileBatteryDegradationAlertsInDb(client, {
      deviceId: device.id,
      nodeId: packet.nodeId,
      configRevisionId: device.current_config_revision_id,
      sensorReadingId: insertedReading.rows[0]?.id ?? null,
      occurredAt: packet.occurredAt,
      detectedAt: acceptedAt,
      batteryPct: packet.batteryPct,
      riskLevel: packet.riskLevel,
      temperatureC: packet.temperatureC,
      humidityPct: packet.humidityPct,
      vocIaq: packet.vocIaq,
      pm25UgM3: packet.pm25UgM3,
      locationLabel: formatCoordinatePair(Number(device.current_latitude), Number(device.current_longitude)),
      ipv6Address: device.current_ipv6,
    })),
  );

  if (packet.packetCode === "0x03") {
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
        VALUES (
          $1,
          $2,
          $3,
          'critical_risk',
          'open',
          'critical',
          $4,
          $5,
          $6,
          $7,
          $7,
          $6,
          $8,
          $9,
          $10,
          $11,
          $12,
          $13
        )
        RETURNING id
      `,
      [
        device.id,
        insertedReading.rows[0]?.id ?? null,
        device.current_config_revision_id,
        `Critical 0x03 event from node ${packet.nodeId}`,
        buildPacketMetadata(packet, acceptedAt),
        packet.occurredAt,
        acceptedAt,
        packet.riskLevel,
        packet.temperatureC,
        packet.humidityPct,
        packet.vocIaq,
        packet.pm25UgM3,
        packet.batteryPct,
      ],
    );

    const alertId = insertedAlert.rows[0]?.id;
    if (alertId) {
      await client.query(
        `
          INSERT INTO alert_events (alert_id, event_type, new_status, event_at, actor, note, details)
          VALUES ($1, 'opened', 'open', $2, 'system', $3, $4)
        `,
        [
          alertId,
          acceptedAt,
          "CSP ingested a critical 0x03 uplink and opened the incident.",
          buildPacketMetadata(packet, acceptedAt),
        ],
      );

      triggers.push({
        eventType: "critical_risk",
        occurredAt: acceptedAt,
        subject: `Critical risk detected at node ${packet.nodeId}`,
        bodyText: `PyroNet detected a critical risk alert from node ${packet.nodeId}. Risk ${packet.riskLevel}. Temp ${packet.temperatureC.toFixed(1)}C, RH ${packet.humidityPct.toFixed(1)}%, VOC ${packet.vocIaq}, PM2.5 ${packet.pm25UgM3.toFixed(1)}.`,
        nodeId: packet.nodeId,
        alertId,
      });
    }
  }

  return triggers;
}

async function ingestNeighborDistributionPacketInDb(
  client: PoolClient,
  packet: Extract<DecodedPacket, { packetCode: "0x04" }>,
  acceptedAt: string,
) {
  const device = await getDeviceByNodeIdOrThrow(client, packet.nodeId);
  const neighborRows = await Promise.all(
    packet.neighborIpv6Addresses.map(async (ipv6Address: string) => {
      const neighbor = await getDeviceByIpv6(client, ipv6Address);
      if (!neighbor) {
        throw new Error(`Unknown neighbor IPv6 ${ipv6Address}`);
      }
      return neighbor;
    }),
  );

  await client.query("UPDATE nn_revisions SET active_to = $2 WHERE device_id = $1 AND active_to IS NULL", [device.id, acceptedAt]);
  const nextRevisionResult = await client.query<{ revision_no: number }>(
    "SELECT COALESCE(MAX(revision_no), 0) + 1 AS revision_no FROM nn_revisions WHERE device_id = $1",
    [device.id],
  );
  const nextRevisionNo = nextRevisionResult.rows[0]?.revision_no ?? 1;
  const radiusMeters = neighborRows.reduce((radius, neighbor) => {
    return Math.max(
      radius,
      haversineDistanceMeters(
        { lat: Number(device.current_latitude), lng: Number(device.current_longitude) },
        { lat: Number(neighbor.current_latitude), lng: Number(neighbor.current_longitude) },
      ),
    );
  }, 1);

  const insertedRevision = await client.query<{ id: number }>(
    `
      INSERT INTO nn_revisions (device_id, revision_no, radius_meters, revision_source, active_from, details)
      VALUES ($1, $2, $3, 'automatic', $4, $5)
      RETURNING id
    `,
    [device.id, nextRevisionNo, radiusMeters, acceptedAt, buildPacketMetadata(packet, acceptedAt)],
  );
  const revisionId = insertedRevision.rows[0]?.id;
  if (!revisionId) {
    throw new Error("Unable to create neighbor revision from 0x04 packet.");
  }

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
        device.id,
        neighbor.id,
        index + 1,
        haversineDistanceMeters(
          { lat: Number(device.current_latitude), lng: Number(device.current_longitude) },
          { lat: Number(neighbor.current_latitude), lng: Number(neighbor.current_longitude) },
        ),
        neighbor.current_latitude,
        neighbor.current_longitude,
      ],
    );
  }

  await client.query("UPDATE devices SET current_nn_revision_id = $2 WHERE id = $1", [device.id, revisionId]);
  await client.query(
    `
      INSERT INTO nn_distribution_events (device_id, nn_revision_id, status, sent_at, payload_metadata)
      VALUES ($1, $2, 'sent', $3, $4)
    `,
    [device.id, revisionId, acceptedAt, buildPacketMetadata(packet, acceptedAt)],
  );
}

async function ingestTimeSyncPacketInDb(
  client: PoolClient,
  packet: Extract<DecodedPacket, { packetCode: "0x05" }>,
  acceptedAt: string,
) {
  const device = await getDeviceByNodeIdOrThrow(client, packet.nodeId);
  await client.query(
    `
      INSERT INTO time_sync_events (device_id, target_time, status, sent_at, payload_metadata)
      VALUES ($1, $2, 'sent', $3, $4)
    `,
    [device.id, packet.occurredAt, acceptedAt, buildPacketMetadata(packet, acceptedAt)],
  );
}

async function ingestConfigUpdatePacketInDb(
  client: PoolClient,
  packet: Extract<DecodedPacket, { packetCode: "0x06" }>,
  acceptedAt: string,
) {
  const device = await getDeviceByNodeIdOrThrow(client, packet.nodeId);
  const existingRevision = await client.query<{ id: number; config_id: number }>(
    "SELECT id, config_id FROM config_revisions WHERE config_id = $1",
    [packet.configId],
  );

  let configRevisionId = existingRevision.rows[0]?.id ?? null;
  if (configRevisionId === null) {
    const maxConfigIdResult = await client.query<{ max_config_id: string | null }>(
      "SELECT MAX(config_id)::text AS max_config_id FROM config_revisions",
    );
    const maxConfigId = Number(maxConfigIdResult.rows[0]?.max_config_id ?? 0);
    if (maxConfigId > 0 && packet.configId < maxConfigId) {
      throw new Error(`Config revision ${packet.configId} is older than the current config history.`);
    }

    await client.query(
      "UPDATE config_revisions SET retired_at = $1 WHERE retired_at IS NULL AND activated_at IS NOT NULL",
      [acceptedAt],
    );
    const insertedRevision = await client.query<{ id: number }>(
      `
        INSERT INTO config_revisions (
          config_id,
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
          notes,
          metadata
        )
        VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13)
        RETURNING id
      `,
      [
        packet.configId,
        packet.thresholds.l2TempThresh,
        packet.thresholds.l2HumidityThresh,
        packet.thresholds.l2VocThresh,
        packet.thresholds.l3TempThresh,
        packet.thresholds.l3HumidityThresh,
        packet.thresholds.l3VocThresh,
        packet.thresholds.l4VocThresh,
        packet.thresholds.l5VocThresh,
        packet.thresholds.l5Pm25Thresh,
        acceptedAt,
        "Ingested from 0x06 packet.",
        buildPacketMetadata(packet, acceptedAt),
      ],
    );
    configRevisionId = insertedRevision.rows[0]?.id ?? null;
  }

  if (!configRevisionId) {
    throw new Error("Unable to resolve config revision for 0x06 packet.");
  }

  await client.query(
    `
      INSERT INTO device_config_deployments (device_id, config_revision_id, status, sent_at, payload_metadata)
      VALUES ($1, $2, 'sent', $3, $4)
    `,
    [device.id, configRevisionId, acceptedAt, buildPacketMetadata(packet, acceptedAt)],
  );
  await client.query("UPDATE devices SET current_config_revision_id = $2 WHERE id = $1", [device.id, configRevisionId]);
}

async function ingestNeighborAlertPacketInDb(
  client: PoolClient,
  packet: Extract<DecodedPacket, { packetCode: "0x07" }>,
  acceptedAt: string,
) {
  const sourceDevice = await getDeviceByNodeIdOrThrow(client, packet.nodeId);
  const targetDevice =
    packet.targetNodeId === null ? null : await getDeviceByNodeIdOrThrow(client, packet.targetNodeId);

  await client.query(
    `
      INSERT INTO neighbor_alerts (
        source_device_id,
        target_device_id,
        risk_level,
        occurred_at,
        ingested_at,
        status,
        raw_payload_metadata
      )
      VALUES ($1, $2, $3, $4, $5, 'received', $6)
    `,
    [
      sourceDevice.id,
      targetDevice?.id ?? null,
      packet.riskLevel,
      packet.occurredAt,
      acceptedAt,
      buildPacketMetadata(packet, acceptedAt),
    ],
  );

  await client.query(
    `
      UPDATE devices
      SET
        last_seen_at = $2,
        latest_risk_level = GREATEST(COALESCE(latest_risk_level, 0), $3)
      WHERE id = $1
    `,
    [sourceDevice.id, packet.occurredAt, packet.riskLevel],
  );

  await clearRecoveredConnectivityLossAlertsInDb(client, sourceDevice.id, packet.nodeId, packet.occurredAt);
}

async function ingestParentUpdatePacketInDb(
  client: PoolClient,
  packet: Extract<DecodedPacket, { packetCode: "0x08" }>,
  acceptedAt: string,
) {
  const device = await getDeviceByNodeIdOrThrow(client, packet.nodeId);
  const parentDevice = packet.parentIpv6 ? await getDeviceByIpv6(client, packet.parentIpv6) : null;

  await client.query(
    `
      INSERT INTO device_parent_updates (
        device_id,
        parent_ipv6,
        parent_device_id,
        observed_at,
        ingested_at,
        raw_payload_metadata
      )
      VALUES ($1, $2::inet, $3, $4, $5, $6)
    `,
    [device.id, packet.parentIpv6, parentDevice?.id ?? null, packet.occurredAt, acceptedAt, buildPacketMetadata(packet, acceptedAt)],
  );

  await client.query("UPDATE devices SET last_seen_at = $2 WHERE id = $1", [device.id, packet.occurredAt]);
  await clearRecoveredConnectivityLossAlertsInDb(client, device.id, packet.nodeId, packet.occurredAt);
}

async function ingestPacketInDb(packet: DecodedPacket, acceptedAt: string) {
  if (!pool) {
    throw new Error("Database is not configured.");
  }

  const client = await pool.connect();
  let triggers: NotificationTrigger[] = [];
  try {
    await client.query("BEGIN");

    switch (packet.packetCode) {
      case "0x01":
        triggers = await ingestRegistrationPacketInDb(client, packet, acceptedAt);
        break;
      case "0x02":
      case "0x03":
        triggers = await ingestSensorPacketInDb(client, packet, acceptedAt);
        break;
      case "0x04":
        await ingestNeighborDistributionPacketInDb(client, packet, acceptedAt);
        break;
      case "0x05":
        await ingestTimeSyncPacketInDb(client, packet, acceptedAt);
        break;
      case "0x06":
        await ingestConfigUpdatePacketInDb(client, packet, acceptedAt);
        break;
      case "0x07":
        await ingestNeighborAlertPacketInDb(client, packet, acceptedAt);
        break;
      case "0x08":
        await ingestParentUpdatePacketInDb(client, packet, acceptedAt);
        break;
    }

    await client.query("COMMIT");
  } catch (error) {
    await client.query("ROLLBACK");
    throw error;
  } finally {
    client.release();
  }

  for (const trigger of triggers) {
    await dispatchNotifications(trigger);
  }
}

>>>>>>> 8ed2bba (notifications)
async function queryNodesFromDb(): Promise<NodeSummary[]> {
  const result = await query<{
    id: number;
    node_id: number;
    current_ipv6: string | null;
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
        nde.status,
        nde.sent_at::text,
        nde.acknowledged_at::text,
        nn.revision_no,
        CASE
          WHEN nde.status = 'acknowledged' THEN concat('NN revision ', nn.revision_no, ' delivered to ', d.node_id)
          WHEN nde.status = 'timed_out' THEN concat('NN revision ', nn.revision_no, ' timed out for ', d.node_id)
          WHEN nde.status = 'failed' THEN concat('NN revision ', nn.revision_no, ' failed for ', d.node_id)
          ELSE concat('NN revision ', nn.revision_no, ' sent to ', d.node_id)
        END AS summary
      FROM nn_distribution_events nde
      JOIN devices d ON d.id = nde.device_id
      JOIN nn_revisions nn ON nn.id = nde.nn_revision_id
      UNION ALL
      SELECT
        concat('ts-', tse.id) AS id,
        '0x05' AS command_code,
        'Daily time synchronization' AS command_name,
        d.node_id,
        d.node_id AS node_name,
        tse.status,
        tse.sent_at::text,
        tse.acknowledged_at::text,
        NULL AS revision_no,
        COALESCE(tse.result_message, concat('Time sync sent to ', d.node_id)) AS summary
      FROM time_sync_events tse
      JOIN devices d ON d.id = tse.device_id
      UNION ALL
      SELECT
        concat('cfg-', dcd.id) AS id,
        '0x06' AS command_code,
        'Threshold revision deployment' AS command_name,
        d.node_id,
        d.node_id AS node_name,
        dcd.status,
        dcd.sent_at::text,
        dcd.acknowledged_at::text,
        cr.config_id AS revision_no,
        CASE
          WHEN dcd.status = 'acknowledged' THEN concat('Config revision ', cr.config_id, ' applied for ', d.node_id)
          WHEN dcd.status = 'timed_out' THEN concat('Config revision ', cr.config_id, ' timed out for ', d.node_id)
          WHEN dcd.status = 'failed' THEN concat('Config revision ', cr.config_id, ' failed for ', d.node_id)
          ELSE concat('Config revision ', cr.config_id, ' queued for ', d.node_id)
        END AS summary
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

async function queryAlertsFromDb(nodes?: NodeSummary[]): Promise<AlertIncident[]> {
  const fleet = nodes ?? (await queryNodesFromDb());
  await syncConnectivityLossAlertsInDb(fleet);
  await syncDownlinkFailureAlertsInDb();
  const result = await query<{
    id: number;
    node_id: number;
    alert_type:
      | "critical_risk"
      | "connectivity_loss"
      | "battery_degradation"
      | "time_sync_failure"
      | "nn_update_failure"
      | "config_update_failure"
      | "system";
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
    accepted_count: string;
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
          WHERE nd.alert_id = a.id AND nd.status = 'accepted'
        ) AS accepted_count,
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
            summary: "Critical risk alert derived from a 0x03 uplink.",
          }
        : row.alert_type === "connectivity_loss"
          ? {
              incidentType: "offline" as const,
              eventCode: "derived-offline" as const,
              sourceType: "periodic_report" as const,
              severity: "warning" as const,
              summary: "Derived connectivity loss because the node has not been seen within the required window.",
          }
        : row.alert_type === "battery_degradation"
          ? {
              incidentType: "battery_health_low" as const,
              eventCode: "battery-health-low" as const,
              sourceType: "periodic_report" as const,
              severity: "warning" as const,
              summary: "Battery health degradation detected from recent telemetry.",
            }
          : row.alert_type === "time_sync_failure"
            ? {
                incidentType: "time_sync_failure" as const,
                eventCode: "0x05" as const,
                sourceType: "periodic_report" as const,
                severity: "warning" as const,
                summary: downlinkFailureSummary("time_sync_failure"),
              }
            : row.alert_type === "nn_update_failure"
              ? {
                  incidentType: "nn_update_failure" as const,
                  eventCode: "0x04" as const,
                  sourceType: "periodic_report" as const,
                  severity: "warning" as const,
                  summary: downlinkFailureSummary("nn_update_failure"),
                }
              : row.alert_type === "config_update_failure"
                ? {
                    incidentType: "config_update_failure" as const,
                    eventCode: "0x06" as const,
                    sourceType: "periodic_report" as const,
                    severity: "warning" as const,
                    summary: downlinkFailureSummary("config_update_failure"),
                  }
          : null;

    if (!incidentMeta) {
      return [];
    }

    const sentCount = Number(row.sent_count);
    const acceptedCount = Number(row.accepted_count);
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
        summary: incidentMeta.summary,
        occurredAt: row.occurred_at,
        detectedAt: row.detected_at,
        latestEventAt: row.latest_event_at,
        locationLabel: formatCoordinatePair(Number(row.current_latitude), Number(row.current_longitude)),
        notificationStatus:
          totalCount === 0
            ? "skipped"
            : sentCount === totalCount
              ? "sent"
              : sentCount > 0 || acceptedCount > 0
                ? "partial"
                : "pending",
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

  return directAlerts.sort(
    (left, right) => timestampMs(right.detectedAt) - timestampMs(left.detectedAt),
  );
}

async function getDashboardFromDb(): Promise<DashboardResponse> {
  const fleet = await queryNodesFromDb();
  const alertQueue = await queryAlertsFromDb(fleet);
  const [downlinks, recentPackets] = await Promise.all([
    queryDownlinksFromDb(),
    getPacketHistoryFromDb({ limit: 5 }).then((response) => response.entries),
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
    neighborLinks,
    alertQueue,
    downlinks,
    recentPackets,
  };
}

async function getNodeDetailFromDb(nodeId: NodeId): Promise<NodeDetail> {
  const fleet = await queryNodesFromDb();
  await syncConnectivityLossAlertsInDb(fleet);
  await syncDownlinkFailureAlertsInDb();
  const node = fleet.find((entry) => entry.nodeId === nodeId);
  if (!node) {
    throw new Error(`Unknown node ${nodeId}`);
  }

  const [neighborResult, readingsResult, timelineResult, ipv6Result, registrationResult] = await Promise.all([
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
      alert_type:
        | "critical_risk"
        | "connectivity_loss"
        | "battery_degradation"
        | "time_sync_failure"
        | "nn_update_failure"
        | "config_update_failure"
        | "system";
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
          a.alert_type,
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
    eventCode:
      row.alert_type === "critical_risk"
        ? ("0x03" as const)
        : row.alert_type === "battery_degradation"
          ? ("battery-health-low" as const)
          : row.alert_type === "time_sync_failure"
            ? ("0x05" as const)
            : row.alert_type === "nn_update_failure"
              ? ("0x04" as const)
              : row.alert_type === "config_update_failure"
                ? ("0x06" as const)
          : ("derived-offline" as const),
    title: row.title,
    summary: row.note ?? `Alert event ${row.event_type}`,
    occurredAt: row.event_at,
    status: row.new_status ?? row.event_type,
    severity: row.severity,
    actor: row.actor,
  }));

  return {
    node,
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
    `VOC ${row.voc_iaq}`,
    `PM2.5 ${Number(row.pm25_ug_m3).toFixed(1)}`,
  ].join(" · ");
}

async function getPacketHistoryFromDb(options: PacketHistoryQuery = {}): Promise<PacketHistoryResponse> {
  await syncDownlinkFailureAlertsInDb();
  const limit = Math.max(1, Math.min(100, options.limit ?? 20));
  const offset = Math.max(0, options.offset ?? 0);
  const nodes = await queryNodesFromDb();
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
          concat('VOC ', sr.voc_iaq),
          concat('PM2.5 ', sr.pm25_ug_m3::text)
        ) AS detail
      FROM sensor_readings sr
      JOIN devices d ON d.id = sr.device_id

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
        CASE
          WHEN nde.status = 'acknowledged' THEN concat('Neighbor table distribution acknowledged by node ', d.node_id, '.')
          WHEN nde.status = 'timed_out' THEN concat('Neighbor table distribution timed out for node ', d.node_id, '.')
          WHEN nde.status = 'failed' THEN concat('Neighbor table distribution failed for node ', d.node_id, '.')
          ELSE concat('Neighbor table distribution sent to node ', d.node_id, '.')
        END AS summary,
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
        CASE
          WHEN tse.status = 'acknowledged' THEN concat('Time sync acknowledged by node ', d.node_id, '.')
          WHEN tse.status = 'timed_out' THEN concat('Time sync timed out for node ', d.node_id, '.')
          WHEN tse.status = 'failed' THEN concat('Time sync failed for node ', d.node_id, '.')
          ELSE concat('Time sync sent to node ', d.node_id, '.')
        END AS summary,
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
        CASE
          WHEN dcd.status = 'acknowledged' THEN concat('Threshold revision deployment acknowledged by node ', d.node_id, '.')
          WHEN dcd.status = 'timed_out' THEN concat('Threshold revision deployment timed out for node ', d.node_id, '.')
          WHEN dcd.status = 'failed' THEN concat('Threshold revision deployment failed for node ', d.node_id, '.')
          ELSE concat('Threshold revision deployment sent to node ', d.node_id, '.')
        END AS summary,
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
    availableNodes: nodes.map((node) => ({
      id: node.id,
      nodeId: node.nodeId,
      displayName: String(node.nodeId),
    })),
    availableDirections: [...packetDirections],
    availablePacketCodes: [...packetLogCodes],
    availableEventTypes: [...packetEventTypes],
    availableStatuses: ["received", "pending", "sent", "acknowledged", "failed", "timed_out"],
  };
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
  await syncDownlinkFailureAlertsInDb();
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
<<<<<<< HEAD
      event_type: NotificationEventType | null;
      preference_enabled: boolean | null;
=======
      event_type: string | null;
      email_enabled: boolean | null;
      sms_enabled: boolean | null;
>>>>>>> 8ed2bba (notifications)
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
<<<<<<< HEAD
      event_type: NotificationEventType;
=======
      event_type: string;
      channel: NotificationChannel;
      destination: string;
>>>>>>> 8ed2bba (notifications)
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
    const eventType = row.event_type && isNotificationEventType(row.event_type) ? row.event_type : null;
    const existing = recipientsMap.get(row.id);
    if (existing) {
      if (eventType) {
        existing.preferences.push({
<<<<<<< HEAD
          eventType: row.event_type,
          isEnabled: row.preference_enabled ?? false,
=======
          eventType,
          emailEnabled: row.email_enabled ?? false,
          smsEnabled: row.sms_enabled ?? false,
>>>>>>> 8ed2bba (notifications)
        });
      }
      continue;
    }

    recipientsMap.set(row.id, {
      id: String(row.id),
      displayName: row.display_name ?? row.email_address,
      emailAddress: row.email_address,
      isEnabled: row.is_enabled,
      preferences: eventType
        ? [
            {
<<<<<<< HEAD
              eventType: row.event_type,
              isEnabled: row.preference_enabled ?? false,
=======
              eventType,
              emailEnabled: row.email_enabled ?? false,
              smsEnabled: row.sms_enabled ?? false,
>>>>>>> 8ed2bba (notifications)
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
    recipient.preferences.sort(
      (left, right) => notificationEventTypes.indexOf(left.eventType) - notificationEventTypes.indexOf(right.eventType),
    );
  }

  return {
    recipients: [...recipientsMap.values()],
<<<<<<< HEAD
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
=======
    deliveries: deliveriesResult.rows.flatMap((row: (typeof deliveriesResult.rows)[number]) => {
      if (!isNotificationEventType(row.event_type)) {
        return [];
      }

      return [
        {
          id: String(row.id),
          recipientId: String(row.recipient_id),
          recipientName: row.recipient_name ?? `Recipient ${row.recipient_id}`,
          eventType: row.event_type,
          channel: row.channel,
          destination: row.destination,
          subject: row.subject,
          status: row.status,
          occurredAt: row.event_occurred_at,
          deliveredAt: row.delivered_at,
          nodeId: row.node_id,
          alertId: row.alert_id ? String(row.alert_id) : null,
          failureReason: row.failure_reason,
        },
      ];
    }),
>>>>>>> 8ed2bba (notifications)
  };
}

async function createConfigRevisionInDb(draft: ConfigRevisionDraft): Promise<ConfigurationResponse> {
  if (!pool) {
    throw new Error("Database is not configured.");
  }

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
        draft.thresholds.l2TempThresh,
        draft.thresholds.l2HumidityThresh,
        draft.thresholds.l2VocThresh,
        draft.thresholds.l3TempThresh,
        draft.thresholds.l3HumidityThresh,
        draft.thresholds.l3VocThresh,
        draft.thresholds.l4VocThresh,
        draft.thresholds.l5VocThresh,
        draft.thresholds.l5Pm25Thresh,
        draft.notes ?? null,
      ],
    );

    const configRevisionId = inserted.rows[0]?.id;
    if (!configRevisionId) {
      throw new Error("Unable to create config revision.");
    }

    const targets = draft.targetNodeIds?.length
      ? await client.query<{ id: number }>("SELECT id FROM devices WHERE node_id = ANY($1::smallint[])", [draft.targetNodeIds])
      : await client.query<{ id: number }>("SELECT id FROM devices");

    for (const target of targets.rows) {
      await client.query(
        `
          INSERT INTO device_config_deployments (device_id, config_revision_id, status, sent_at)
          VALUES ($1, $2, 'sent', NOW())
        `,
        [target.id, configRevisionId],
      );
      await client.query("UPDATE devices SET current_config_revision_id = $2 WHERE id = $1", [target.id, configRevisionId]);
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

    const ownerResult = await client.query<{ id: number; current_latitude: string; current_longitude: string }>(
      "SELECT id, current_latitude::text, current_longitude::text FROM devices WHERE node_id = $1",
      [nodeId],
    );
    const owner = ownerResult.rows[0];
    if (!owner) {
      throw new Error(`Unknown node ${nodeId}`);
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
    await client.query(
      "INSERT INTO nn_distribution_events (device_id, nn_revision_id, status, sent_at) VALUES ($1, $2, 'sent', NOW())",
      [owner.id, revisionId],
    );

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
  const targets = request.targetNodeIds?.length
    ? await query<{ id: number }>("SELECT id FROM devices WHERE node_id = ANY($1::smallint[])", [request.targetNodeIds])
    : await query<{ id: number }>("SELECT id FROM devices");

  for (const target of targets.rows) {
    await query(
      `
        INSERT INTO time_sync_events (device_id, target_time, status, sent_at)
        VALUES ($1, NOW(), 'sent', NOW())
      `,
      [target.id],
    );
  }

  return getConfigurationFromDb();
}

async function createNeighborDistributionInDb(request: DownlinkRequest): Promise<ConfigurationResponse> {
  if (!pool) {
    throw new Error("Database is not configured.");
  }
  const targets = request.targetNodeIds?.length
    ? await query<{ id: number; current_nn_revision_id: number | null }>(
        "SELECT id, current_nn_revision_id FROM devices WHERE node_id = ANY($1::smallint[])",
        [request.targetNodeIds],
      )
    : await query<{ id: number; current_nn_revision_id: number | null }>("SELECT id, current_nn_revision_id FROM devices");

  for (const target of targets.rows) {
    if (!target.current_nn_revision_id) {
      continue;
    }
    await query(
      `
        INSERT INTO nn_distribution_events (device_id, nn_revision_id, status, sent_at)
        VALUES ($1, $2, 'sent', NOW())
      `,
      [target.id, target.current_nn_revision_id],
    );
  }

  return getConfigurationFromDb();
}

async function createThresholdPushInDb(request: DownlinkRequest): Promise<ConfigurationResponse> {
  if (!pool) {
    throw new Error("Database is not configured.");
  }

  const configRevisionId =
    request.configRevisionId ??
    (
      await query<{ id: number }>(
        "SELECT id FROM config_revisions WHERE retired_at IS NULL ORDER BY created_at DESC LIMIT 1",
      )
    ).rows[0]?.id;

  if (!configRevisionId) {
    throw new Error("No active config revision found.");
  }

  const targets = request.targetNodeIds?.length
    ? await query<{ id: number }>("SELECT id FROM devices WHERE node_id = ANY($1::smallint[])", [request.targetNodeIds])
    : await query<{ id: number }>("SELECT id FROM devices");

  for (const target of targets.rows) {
    await query(
      `
        INSERT INTO device_config_deployments (device_id, config_revision_id, status, sent_at)
        VALUES ($1, $2, 'sent', NOW())
      `,
      [target.id, configRevisionId],
    );
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
<<<<<<< HEAD
  await query("UPDATE notification_recipients SET is_enabled = $2, updated_at = NOW() WHERE id = $1", [
    numericRecipientId,
    update.isEnabled,
  ]);
=======
  await query(
    `
      UPDATE notification_recipients
      SET
        display_name = $2,
        email_address = $3,
        phone_number = $4,
        is_enabled = $5,
        updated_at = NOW()
      WHERE id = $1
    `,
    [numericRecipientId, update.displayName, update.emailAddress, normalizePhoneNumber(update.phoneNumber), update.isEnabled],
  );

  await query("DELETE FROM notification_preferences WHERE recipient_id = $1 AND event_type::text = 'system'", [numericRecipientId]);

  const preferencesByType = new Map<NotificationEventType, NotificationPreference>(
    update.preferences.map((preference) => [preference.eventType, preference]),
  );
>>>>>>> 8ed2bba (notifications)

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
  const message = error instanceof Error ? error.message : "Unexpected server error";
  response.status(500).json({ message });
});

app.listen(port, () => {
  process.stdout.write(`PyroNet API listening on http://localhost:${port}\n`);
});
