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
  NodeSummary,
  NotificationEventType,
  NotificationRecipientUpdate,
  NotificationSettingsResponse,
  ReadingHistoryPoint,
  TelemetrySnapshot,
} from "../src/api/types";
import {
  createMockConfigRevision,
  createMockNeighborDistribution,
  createMockThresholdPush,
  createMockTimeSync,
  getMockConfiguration,
  getMockDashboard,
  getMockHistory,
  getMockNodeDetail,
  getMockNotificationSettings,
  listMockAlerts,
  listMockNodes,
  updateMockNeighborRevision,
  updateMockNotificationRecipient,
} from "../src/mocks/mockBackend";

const app = express();
const port = Number(process.env.API_PORT ?? "4000");
const databaseUrl = process.env.DATABASE_URL?.trim();
const pool = databaseUrl ? new Pool({ connectionString: databaseUrl }) : null;
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

function getNowMs() {
  return Date.now();
}

function timestampMs(value: string | null) {
  return value ? new Date(value).getTime() : 0;
}

function connectivityFromLastSeen(lastSeenAt: string | null): ConnectivityStatus {
  if (!lastSeenAt) {
    return "offline";
  }

  const ageMs = getNowMs() - timestampMs(lastSeenAt);

  if (ageMs >= 24 * 60 * 60 * 1000) {
    return "offline";
  }

  if (ageMs >= 30 * 60 * 1000) {
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

async function queryNodesFromDb(): Promise<NodeSummary[]> {
  const result = await query<{
    id: number;
    node_id: string;
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
      displayName: row.node_id,
      ipv6Address: row.current_ipv6,
      connectivity: connectivityFromLastSeen(row.last_seen_at),
      location: {
        lat: Number(row.current_latitude),
        lng: Number(row.current_longitude),
        label: `${Number(row.current_latitude).toFixed(4)}, ${Number(row.current_longitude).toFixed(4)}`,
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
    node_id: string;
    node_name: string;
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
        concat('NN revision ', nn.revision_no, ' delivered to ', d.node_id) AS summary
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
        concat('Config revision ', cr.config_id, ' queued for ', d.node_id) AS summary
      FROM device_config_deployments dcd
      JOIN devices d ON d.id = dcd.device_id
      JOIN config_revisions cr ON cr.id = dcd.config_revision_id
      ORDER BY sent_at DESC
      LIMIT 20
    `,
  );

  return result.rows.map((row: (typeof result.rows)[number]) => ({
    id: row.id,
    commandCode: row.command_code,
    commandName: row.command_name,
    nodeId: row.node_id,
    nodeName: row.node_name,
    status: row.status,
    sentAt: row.sent_at,
    acknowledgedAt: row.acknowledged_at,
    revisionNo: row.revision_no,
    summary: row.summary,
  }));
}

async function queryAlertsFromDb(nodes?: NodeSummary[]): Promise<AlertIncident[]> {
  const fleet = nodes ?? (await queryNodesFromDb());
  const result = await query<{
    id: number;
    node_id: string;
    title: string;
    severity: AlertIncident["severity"];
    status: "open" | "acknowledged" | "cleared";
    occurred_at: string;
    detected_at: string;
    latest_event_at: string;
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
        a.title,
        a.severity,
        a.status,
        a.occurred_at::text,
        a.detected_at::text,
        a.latest_event_at::text,
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

  const directAlerts: AlertIncident[] = result.rows.map((row: (typeof result.rows)[number]) => {
    const sentCount = Number(row.sent_count);
    const totalCount = Number(row.total_count);
    return {
      id: `alert-${row.id}`,
      incidentType: "critical_alert",
      eventCode: "0x03",
      nodeId: row.node_id,
      nodeName: row.node_id,
      severity: row.severity,
      status: row.status,
      title: row.title,
      summary: typeof row.details === "object" ? "Database alert event" : "Database alert event",
      occurredAt: row.occurred_at,
      detectedAt: row.detected_at,
      latestEventAt: row.latest_event_at,
      locationLabel: `${Number(row.current_latitude).toFixed(4)}, ${Number(row.current_longitude).toFixed(4)}`,
      notificationStatus: totalCount === 0 ? "skipped" : sentCount === totalCount ? "sent" : sentCount > 0 ? "partial" : "pending",
      visibleWithinSla: timestampMs(row.detected_at) - timestampMs(row.occurred_at) <= 10 * 60 * 1000,
      latestSnapshot:
        row.snapshot_reported_at && row.snapshot_risk_level !== null
          ? {
              reportedAt: row.snapshot_reported_at,
              sourceType: "critical_alert",
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
    };
  });

  const offlineIncidents = fleet
    .filter((node) => node.connectivity === "offline" && node.lastSeenAt)
    .map((node) => {
      const derivedAt = new Date(timestampMs(node.lastSeenAt) + 24 * 60 * 60 * 1000).toISOString();
      return {
        id: `offline-${node.nodeId}`,
        incidentType: "offline" as const,
        eventCode: "derived-offline" as const,
        nodeId: node.nodeId,
        nodeName: node.displayName,
        severity: "warning" as const,
        status: "derived" as const,
        title: `${node.nodeId} silent for 24 hours`,
        summary: "Derived offline incident because the node has not been seen within the required window.",
        occurredAt: derivedAt,
        detectedAt: derivedAt,
        latestEventAt: derivedAt,
        locationLabel: node.location.label,
        notificationStatus: "pending" as const,
        visibleWithinSla: true,
        latestSnapshot: node.latestTelemetry,
      };
    });

  return [...directAlerts, ...offlineIncidents].sort(
    (left, right) => timestampMs(right.detectedAt) - timestampMs(left.detectedAt),
  );
}

async function getDashboardFromDb(): Promise<DashboardResponse> {
  const fleet = await queryNodesFromDb();
  const [alertQueue, downlinks] = await Promise.all([queryAlertsFromDb(fleet), queryDownlinksFromDb()]);
  const neighborLinksResult = await query<{
    owner_node_id: string;
    owner_latitude: string;
    owner_longitude: string;
    neighbor_node_id: string;
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
    ownerName: row.owner_node_id,
    neighborName: row.neighbor_node_id,
    distanceMeters: row.distance_meters,
    points: [
      {
        lat: Number(row.owner_latitude),
        lng: Number(row.owner_longitude),
        label: row.owner_node_id,
      },
      {
        lat: Number(row.neighbor_latitude),
        lng: Number(row.neighbor_longitude),
        label: row.neighbor_node_id,
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
  };
}

async function getNodeDetailFromDb(nodeId: string): Promise<NodeDetail> {
  const fleet = await queryNodesFromDb();
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
      neighbor_node_id: string;
      neighbor_name: string;
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
          neighborName: row.neighbor_name,
          rank: row.neighbor_rank,
          distanceMeters: row.distance_meters,
          location: {
            lat: Number(row.neighbor_latitude),
            lng: Number(row.neighbor_longitude),
            label: row.neighbor_name,
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
    const derivedAt = new Date(timestampMs(node.lastSeenAt) + 24 * 60 * 60 * 1000).toISOString();
    alertTimeline.unshift({
      id: `offline-${node.nodeId}`,
      eventCode: "derived-offline",
      title: "Offline incident derived",
      summary: "No telemetry received for more than 24 hours.",
      occurredAt: derivedAt,
      status: "derived",
      severity: "warning",
      actor: "system",
    });
  }

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

async function getHistoryFromDb(nodeId?: string, window: HistoryWindow = "24h"): Promise<HistoryResponse> {
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
        displayName: node.displayName,
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
      displayName: node.displayName,
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
          nodeName: node.displayName,
          revisionId: null,
          revisionNo: null,
          radiusMeters: null,
          neighbors: [],
        };
      }

      const membershipsResult = await query<{
        revision_no: number;
        radius_meters: number;
        neighbor_node_id: string;
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
        nodeName: node.displayName,
        revisionId: node.currentNeighborRevisionId,
        revisionNo: membershipsResult.rows[0]?.revision_no ?? node.currentNeighborRevisionNo,
        radiusMeters: membershipsResult.rows[0]?.radius_meters ?? null,
        neighbors: membershipsResult.rows
          .filter((row: (typeof membershipsResult.rows)[number]) => row.neighbor_node_id)
          .map((row: (typeof membershipsResult.rows)[number]) => ({
            neighborId: row.neighbor_node_id,
            neighborNodeId: row.neighbor_node_id,
            neighborName: row.neighbor_node_id,
            rank: row.neighbor_rank,
            distanceMeters: row.distance_meters,
            location: {
              lat: Number(row.neighbor_latitude),
              lng: Number(row.neighbor_longitude),
              label: row.neighbor_node_id,
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
      node_id: string | null;
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
      ? await client.query<{ id: number }>("SELECT id FROM devices WHERE node_id = ANY($1::text[])", [draft.targetNodeIds])
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

async function updateNeighborRevisionInDb(nodeId: string, draft: NeighborRevisionDraft): Promise<ConfigurationResponse> {
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
          await client.query<{ id: number; node_id: string; current_latitude: string; current_longitude: string }>(
            "SELECT id, node_id, current_latitude::text, current_longitude::text FROM devices WHERE node_id = ANY($1::text[])",
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
    ? await query<{ id: number }>("SELECT id FROM devices WHERE node_id = ANY($1::text[])", [request.targetNodeIds])
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
        "SELECT id, current_nn_revision_id FROM devices WHERE node_id = ANY($1::text[])",
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
    ? await query<{ id: number }>("SELECT id FROM devices WHERE node_id = ANY($1::text[])", [request.targetNodeIds])
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
    response.json(
      await withSource(() => getNodeDetailFromDb(request.params.nodeId), () => getMockNodeDetail(request.params.nodeId)),
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
    const nodeId = typeof request.query.nodeId === "string" ? request.query.nodeId : undefined;
    const window = (typeof request.query.window === "string" ? request.query.window : "24h") as HistoryWindow;
    response.json(await withSource(() => getHistoryFromDb(nodeId, window), () => getMockHistory(nodeId, window)));
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
    const body = request.body as NeighborRevisionDraft;
    response.json(
      await withSource(
        () => updateNeighborRevisionInDb(request.params.nodeId, body),
        () => updateMockNeighborRevision(request.params.nodeId, body),
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
