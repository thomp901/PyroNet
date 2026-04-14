import type {
  AlertIncident,
  AlertTimelineEntry,
  ConfigRevision,
  ConfigRevisionDraft,
  ConfigurationResponse,
  ConnectivityStatus,
  DashboardResponse,
  DownlinkActivity,
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
  NotificationDelivery,
  NotificationEventType,
  NotificationRecipient,
  NotificationRecipientUpdate,
  NotificationSettingsResponse,
  ReadingHistoryPoint,
  TelemetrySnapshot,
} from "../api/types";

interface DeviceRecord {
  id: string;
  nodeId: string;
  displayName: string;
  ipv6Address: string;
  firmwareVersion: string;
  location: { lat: number; lng: number; label: string };
  firstRegisteredAt: string;
  lastRegisteredAt: string;
  lastSeenAt: string;
  lastReportedAt: string;
  activeConfigRevisionId: number;
  activeConfigRevisionNo: number;
  currentNeighborRevisionId: number;
  currentNeighborRevisionNo: number;
}

interface ReadingRecord extends TelemetrySnapshot {
  id: string;
  deviceId: string;
}

interface AlertRecord {
  id: string;
  deviceId: string;
  severity: "info" | "warning" | "critical";
  status: "open" | "acknowledged" | "cleared";
  title: string;
  summary: string;
  occurredAt: string;
  detectedAt: string;
  latestEventAt: string;
}

interface TimelineRecord extends AlertTimelineEntry {
  deviceId: string;
}

interface NeighborRevisionRecord {
  id: number;
  deviceId: string;
  revisionNo: number;
  radiusMeters: number;
  revisionSource: "automatic" | "manual" | "imported";
  activeFrom: string;
}

interface NeighborMembershipRecord {
  revisionId: number;
  ownerDeviceId: string;
  neighborDeviceId: string;
  rank: number;
  distanceMeters: number;
}

interface ConfigRevisionRecord extends ConfigRevision {}

interface RegistrationRecord {
  deviceId: string;
  observedAt: string;
  ipv6Address: string;
  latitude: number;
  longitude: number;
  firmwareVersion: string;
  batteryPct: number;
}

interface Ipv6HistoryRecord {
  deviceId: string;
  address: string;
  validFrom: string;
  validTo: string | null;
}

interface DownlinkRecord {
  id: string;
  commandCode: "0x04" | "0x05" | "0x06";
  commandName: string;
  deviceId: string;
  status: "pending" | "sent" | "acknowledged" | "failed" | "timed_out";
  sentAt: string;
  acknowledgedAt: string | null;
  revisionNo: number | null;
  summary: string;
}

interface RecipientRecord {
  id: string;
  displayName: string;
  emailAddress: string;
  isEnabled: boolean;
}

interface DeliveryRecord extends NotificationDelivery {}

const OFFLINE_THRESHOLD_MS = 24 * 60 * 60 * 1000;
const SLA_THRESHOLD_MS = 10 * 60 * 1000;
const NOW = "2026-04-14T20:00:00Z";

const devices: DeviceRecord[] = [
  {
    id: "dev-001",
    nodeId: "node-001",
    displayName: "North Ridge Sensor",
    ipv6Address: "2001:db8:100::11",
    firmwareVersion: "2.4.1",
    location: { lat: 34.2847, lng: -118.4392, label: "North Ridge" },
    firstRegisteredAt: "2026-03-01T08:15:00Z",
    lastRegisteredAt: "2026-04-14T06:10:00Z",
    lastSeenAt: "2026-04-14T19:58:00Z",
    lastReportedAt: "2026-04-14T19:58:00Z",
    activeConfigRevisionId: 2,
    activeConfigRevisionNo: 1002,
    currentNeighborRevisionId: 11,
    currentNeighborRevisionNo: 4,
  },
  {
    id: "dev-002",
    nodeId: "node-002",
    displayName: "Valley Floor Sensor",
    ipv6Address: "2001:db8:100::12",
    firmwareVersion: "2.4.1",
    location: { lat: 34.2411, lng: -118.5123, label: "Valley Floor" },
    firstRegisteredAt: "2026-03-03T07:40:00Z",
    lastRegisteredAt: "2026-04-14T05:40:00Z",
    lastSeenAt: "2026-04-14T19:54:00Z",
    lastReportedAt: "2026-04-14T19:54:00Z",
    activeConfigRevisionId: 2,
    activeConfigRevisionNo: 1002,
    currentNeighborRevisionId: 12,
    currentNeighborRevisionNo: 3,
  },
  {
    id: "dev-003",
    nodeId: "node-003",
    displayName: "Canyon Mouth Sensor",
    ipv6Address: "2001:db8:100::13",
    firmwareVersion: "2.4.0",
    location: { lat: 34.2554, lng: -118.4688, label: "Canyon Mouth" },
    firstRegisteredAt: "2026-03-04T12:10:00Z",
    lastRegisteredAt: "2026-04-13T04:22:00Z",
    lastSeenAt: "2026-04-13T10:30:00Z",
    lastReportedAt: "2026-04-13T10:18:00Z",
    activeConfigRevisionId: 2,
    activeConfigRevisionNo: 1002,
    currentNeighborRevisionId: 13,
    currentNeighborRevisionNo: 2,
  },
  {
    id: "dev-004",
    nodeId: "node-004",
    displayName: "Operations Gateway",
    ipv6Address: "2001:db8:100::14",
    firmwareVersion: "3.0.2",
    location: { lat: 34.2605, lng: -118.472, label: "Operations Yard" },
    firstRegisteredAt: "2026-02-25T18:00:00Z",
    lastRegisteredAt: "2026-04-14T00:05:00Z",
    lastSeenAt: "2026-04-14T19:57:00Z",
    lastReportedAt: "2026-04-14T19:50:00Z",
    activeConfigRevisionId: 2,
    activeConfigRevisionNo: 1002,
    currentNeighborRevisionId: 14,
    currentNeighborRevisionNo: 5,
  },
];

const readings: ReadingRecord[] = [
  {
    id: "r-001",
    deviceId: "dev-001",
    reportedAt: "2026-04-13T22:00:00Z",
    sourceType: "periodic_report",
    riskLevel: 2,
    temperatureC: 28.7,
    humidityPct: 28,
    vocIaq: 84,
    pm25UgM3: 11.2,
    batteryPct: 90,
    pressureHpa: 1013.2,
    batteryHealthScore: 96,
  },
  {
    id: "r-002",
    deviceId: "dev-001",
    reportedAt: "2026-04-14T04:00:00Z",
    sourceType: "periodic_report",
    riskLevel: 2,
    temperatureC: 25.4,
    humidityPct: 34,
    vocIaq: 78,
    pm25UgM3: 9.3,
    batteryPct: 89,
    pressureHpa: 1014.8,
    batteryHealthScore: 96,
  },
  {
    id: "r-003",
    deviceId: "dev-001",
    reportedAt: "2026-04-14T10:00:00Z",
    sourceType: "periodic_report",
    riskLevel: 2,
    temperatureC: 31.2,
    humidityPct: 23,
    vocIaq: 92,
    pm25UgM3: 12.1,
    batteryPct: 88,
    pressureHpa: 1012.1,
    batteryHealthScore: 96,
  },
  {
    id: "r-004",
    deviceId: "dev-001",
    reportedAt: "2026-04-14T16:00:00Z",
    sourceType: "periodic_report",
    riskLevel: 3,
    temperatureC: 33.4,
    humidityPct: 19,
    vocIaq: 116,
    pm25UgM3: 18.5,
    batteryPct: 88,
    pressureHpa: 1011.4,
    batteryHealthScore: 95,
  },
  {
    id: "r-005",
    deviceId: "dev-001",
    reportedAt: "2026-04-14T19:58:00Z",
    sourceType: "periodic_report",
    riskLevel: 3,
    temperatureC: 34.1,
    humidityPct: 18,
    vocIaq: 124,
    pm25UgM3: 22.4,
    batteryPct: 87,
    pressureHpa: 1010.8,
    batteryHealthScore: 95,
  },
  {
    id: "r-006",
    deviceId: "dev-002",
    reportedAt: "2026-04-13T22:00:00Z",
    sourceType: "periodic_report",
    riskLevel: 2,
    temperatureC: 30.1,
    humidityPct: 21,
    vocIaq: 108,
    pm25UgM3: 18.3,
    batteryPct: 49,
    pressureHpa: 1012.4,
    batteryHealthScore: 82,
  },
  {
    id: "r-007",
    deviceId: "dev-002",
    reportedAt: "2026-04-14T04:00:00Z",
    sourceType: "periodic_report",
    riskLevel: 3,
    temperatureC: 32.7,
    humidityPct: 19,
    vocIaq: 140,
    pm25UgM3: 26.2,
    batteryPct: 48,
    pressureHpa: 1011.9,
    batteryHealthScore: 81,
  },
  {
    id: "r-008",
    deviceId: "dev-002",
    reportedAt: "2026-04-14T10:00:00Z",
    sourceType: "periodic_report",
    riskLevel: 4,
    temperatureC: 35.8,
    humidityPct: 17,
    vocIaq: 188,
    pm25UgM3: 39.1,
    batteryPct: 47,
    pressureHpa: 1010.6,
    batteryHealthScore: 81,
  },
  {
    id: "r-009",
    deviceId: "dev-002",
    reportedAt: "2026-04-14T19:51:00Z",
    sourceType: "critical_alert",
    riskLevel: 5,
    temperatureC: 37.6,
    humidityPct: 15,
    vocIaq: 248,
    pm25UgM3: 68.4,
    batteryPct: 46,
    pressureHpa: 1009.4,
    batteryHealthScore: 80,
  },
  {
    id: "r-010",
    deviceId: "dev-002",
    reportedAt: "2026-04-14T19:54:00Z",
    sourceType: "periodic_report",
    riskLevel: 5,
    temperatureC: 38.1,
    humidityPct: 14,
    vocIaq: 261,
    pm25UgM3: 74.6,
    batteryPct: 46,
    pressureHpa: 1009.2,
    batteryHealthScore: 80,
  },
  {
    id: "r-011",
    deviceId: "dev-003",
    reportedAt: "2026-04-12T22:00:00Z",
    sourceType: "periodic_report",
    riskLevel: 2,
    temperatureC: 26.8,
    humidityPct: 31,
    vocIaq: 73,
    pm25UgM3: 8.2,
    batteryPct: 63,
    pressureHpa: 1014.2,
    batteryHealthScore: 89,
  },
  {
    id: "r-012",
    deviceId: "dev-003",
    reportedAt: "2026-04-13T04:00:00Z",
    sourceType: "periodic_report",
    riskLevel: 2,
    temperatureC: 24.7,
    humidityPct: 37,
    vocIaq: 70,
    pm25UgM3: 7.4,
    batteryPct: 62,
    pressureHpa: 1014.9,
    batteryHealthScore: 89,
  },
  {
    id: "r-013",
    deviceId: "dev-003",
    reportedAt: "2026-04-13T10:18:00Z",
    sourceType: "periodic_report",
    riskLevel: 2,
    temperatureC: 29.2,
    humidityPct: 29,
    vocIaq: 83,
    pm25UgM3: 10.6,
    batteryPct: 61,
    pressureHpa: 1013.8,
    batteryHealthScore: 88,
  },
  {
    id: "r-014",
    deviceId: "dev-004",
    reportedAt: "2026-04-13T22:00:00Z",
    sourceType: "periodic_report",
    riskLevel: 1,
    temperatureC: 24.1,
    humidityPct: 39,
    vocIaq: 45,
    pm25UgM3: 5.8,
    batteryPct: 100,
    pressureHpa: 1015.2,
    batteryHealthScore: 100,
  },
  {
    id: "r-015",
    deviceId: "dev-004",
    reportedAt: "2026-04-14T04:00:00Z",
    sourceType: "periodic_report",
    riskLevel: 1,
    temperatureC: 22.9,
    humidityPct: 41,
    vocIaq: 40,
    pm25UgM3: 5.2,
    batteryPct: 100,
    pressureHpa: 1015.8,
    batteryHealthScore: 100,
  },
  {
    id: "r-016",
    deviceId: "dev-004",
    reportedAt: "2026-04-14T10:00:00Z",
    sourceType: "periodic_report",
    riskLevel: 1,
    temperatureC: 26.3,
    humidityPct: 35,
    vocIaq: 49,
    pm25UgM3: 6.4,
    batteryPct: 100,
    pressureHpa: 1014.4,
    batteryHealthScore: 100,
  },
  {
    id: "r-017",
    deviceId: "dev-004",
    reportedAt: "2026-04-14T19:50:00Z",
    sourceType: "periodic_report",
    riskLevel: 2,
    temperatureC: 28.2,
    humidityPct: 33,
    vocIaq: 58,
    pm25UgM3: 8.9,
    batteryPct: 100,
    pressureHpa: 1013.1,
    batteryHealthScore: 100,
  },
];

const alerts: AlertRecord[] = [
  {
    id: "alert-001",
    deviceId: "dev-002",
    severity: "critical",
    status: "open",
    title: "Critical 0x03 event from Valley Floor Sensor",
    summary: "VOC and PM2.5 crossed level-5 thresholds and triggered a critical uplink.",
    occurredAt: "2026-04-14T19:51:00Z",
    detectedAt: "2026-04-14T19:52:00Z",
    latestEventAt: "2026-04-14T19:52:00Z",
  },
  {
    id: "alert-002",
    deviceId: "dev-001",
    severity: "warning",
    status: "acknowledged",
    title: "Battery degradation trend on North Ridge Sensor",
    summary: "Battery health score dropped below the preferred operating band during the afternoon run.",
    occurredAt: "2026-04-14T16:00:00Z",
    detectedAt: "2026-04-14T16:05:00Z",
    latestEventAt: "2026-04-14T16:20:00Z",
  },
];

const timeline: TimelineRecord[] = [
  {
    id: "evt-001",
    deviceId: "dev-002",
    eventCode: "0x03",
    title: "Critical alert opened",
    summary: "CSP ingested a critical 0x03 uplink and opened the incident.",
    occurredAt: "2026-04-14T19:52:00Z",
    status: "open",
    severity: "critical",
    actor: "system",
  },
  {
    id: "evt-002",
    deviceId: "dev-001",
    eventCode: "0x03",
    title: "Battery degradation detected",
    summary: "Derived system alert captured a battery health regression.",
    occurredAt: "2026-04-14T16:05:00Z",
    status: "open",
    severity: "warning",
    actor: "system",
  },
  {
    id: "evt-003",
    deviceId: "dev-001",
    eventCode: "0x03",
    title: "Battery degradation acknowledged",
    summary: "Operations acknowledged the warning and scheduled the node for inspection.",
    occurredAt: "2026-04-14T16:20:00Z",
    status: "acknowledged",
    severity: "warning",
    actor: "ops@pyronet",
  },
  {
    id: "evt-004",
    deviceId: "dev-003",
    eventCode: "derived-offline",
    title: "Offline incident derived",
    summary: "Node has been silent for more than 24 hours based on last-seen telemetry.",
    occurredAt: "2026-04-14T10:30:00Z",
    status: "derived",
    severity: "warning",
    actor: "system",
  },
  {
    id: "evt-005",
    deviceId: "dev-004",
    eventCode: "0x05",
    title: "Time sync acknowledged",
    summary: "Daily time synchronization completed successfully.",
    occurredAt: "2026-04-14T00:02:00Z",
    status: "acknowledged",
    severity: "info",
    actor: "system",
  },
];

const neighborRevisions: NeighborRevisionRecord[] = [
  { id: 11, deviceId: "dev-001", revisionNo: 4, radiusMeters: 1850, revisionSource: "automatic", activeFrom: "2026-04-14T18:00:00Z" },
  { id: 12, deviceId: "dev-002", revisionNo: 3, radiusMeters: 2100, revisionSource: "automatic", activeFrom: "2026-04-14T18:00:00Z" },
  { id: 13, deviceId: "dev-003", revisionNo: 2, radiusMeters: 2200, revisionSource: "automatic", activeFrom: "2026-04-13T06:00:00Z" },
  { id: 14, deviceId: "dev-004", revisionNo: 5, radiusMeters: 1600, revisionSource: "manual", activeFrom: "2026-04-14T12:00:00Z" },
];

const memberships: NeighborMembershipRecord[] = [
  { revisionId: 11, ownerDeviceId: "dev-001", neighborDeviceId: "dev-004", rank: 1, distanceMeters: 1080 },
  { revisionId: 11, ownerDeviceId: "dev-001", neighborDeviceId: "dev-003", rank: 2, distanceMeters: 1540 },
  { revisionId: 12, ownerDeviceId: "dev-002", neighborDeviceId: "dev-004", rank: 1, distanceMeters: 1240 },
  { revisionId: 12, ownerDeviceId: "dev-002", neighborDeviceId: "dev-001", rank: 2, distanceMeters: 1760 },
  { revisionId: 13, ownerDeviceId: "dev-003", neighborDeviceId: "dev-004", rank: 1, distanceMeters: 760 },
  { revisionId: 13, ownerDeviceId: "dev-003", neighborDeviceId: "dev-001", rank: 2, distanceMeters: 1540 },
  { revisionId: 14, ownerDeviceId: "dev-004", neighborDeviceId: "dev-003", rank: 1, distanceMeters: 760 },
  { revisionId: 14, ownerDeviceId: "dev-004", neighborDeviceId: "dev-001", rank: 2, distanceMeters: 1080 },
  { revisionId: 14, ownerDeviceId: "dev-004", neighborDeviceId: "dev-002", rank: 3, distanceMeters: 1240 },
];

const configRevisions: ConfigRevisionRecord[] = [
  {
    id: 1,
    configId: 1001,
    activatedAt: "2026-04-10T09:00:00Z",
    retiredAt: "2026-04-14T08:00:00Z",
    createdAt: "2026-04-10T08:45:00Z",
    notes: "Raised level-4 VOC threshold after dry weather front.",
    thresholds: {
      l2TempThresh: 31,
      l2HumidityThresh: 35,
      l2VocThresh: 100,
      l3TempThresh: 35,
      l3HumidityThresh: 25,
      l3VocThresh: 150,
      l4VocThresh: 210,
      l5VocThresh: 260,
      l5Pm25Thresh: 60,
    },
  },
  {
    id: 2,
    configId: 1002,
    activatedAt: "2026-04-14T08:00:00Z",
    retiredAt: null,
    createdAt: "2026-04-14T07:45:00Z",
    notes: "Current active revision for dry and windy conditions.",
    thresholds: {
      l2TempThresh: 32,
      l2HumidityThresh: 33,
      l2VocThresh: 110,
      l3TempThresh: 36,
      l3HumidityThresh: 23,
      l3VocThresh: 165,
      l4VocThresh: 225,
      l5VocThresh: 255,
      l5Pm25Thresh: 58,
    },
  },
];

const registrations: RegistrationRecord[] = [
  {
    deviceId: "dev-001",
    observedAt: "2026-04-14T06:10:00Z",
    ipv6Address: "2001:db8:100::11",
    latitude: 34.2847,
    longitude: -118.4392,
    firmwareVersion: "2.4.1",
    batteryPct: 88,
  },
  {
    deviceId: "dev-002",
    observedAt: "2026-04-14T05:40:00Z",
    ipv6Address: "2001:db8:100::12",
    latitude: 34.2411,
    longitude: -118.5123,
    firmwareVersion: "2.4.1",
    batteryPct: 48,
  },
  {
    deviceId: "dev-003",
    observedAt: "2026-04-13T04:22:00Z",
    ipv6Address: "2001:db8:100::13",
    latitude: 34.2554,
    longitude: -118.4688,
    firmwareVersion: "2.4.0",
    batteryPct: 62,
  },
];

const ipv6History: Ipv6HistoryRecord[] = [
  { deviceId: "dev-001", address: "2001:db8:100::11", validFrom: "2026-03-01T08:15:00Z", validTo: null },
  { deviceId: "dev-002", address: "2001:db8:100::12", validFrom: "2026-03-03T07:40:00Z", validTo: null },
  { deviceId: "dev-003", address: "2001:db8:100::13", validFrom: "2026-03-04T12:10:00Z", validTo: null },
  { deviceId: "dev-004", address: "2001:db8:100::14", validFrom: "2026-02-25T18:00:00Z", validTo: null },
];

const downlinks: DownlinkRecord[] = [
  {
    id: "dl-001",
    commandCode: "0x04",
    commandName: "Neighbor table distribution",
    deviceId: "dev-004",
    status: "acknowledged",
    sentAt: "2026-04-14T12:01:00Z",
    acknowledgedAt: "2026-04-14T12:04:00Z",
    revisionNo: 5,
    summary: "Manual revision 5 distributed to Operations Gateway.",
  },
  {
    id: "dl-002",
    commandCode: "0x05",
    commandName: "Daily time synchronization",
    deviceId: "dev-001",
    status: "acknowledged",
    sentAt: "2026-04-14T00:00:00Z",
    acknowledgedAt: "2026-04-14T00:02:00Z",
    revisionNo: null,
    summary: "Daily time sync completed successfully.",
  },
  {
    id: "dl-003",
    commandCode: "0x06",
    commandName: "Threshold revision deployment",
    deviceId: "dev-003",
    status: "pending",
    sentAt: "2026-04-14T08:05:00Z",
    acknowledgedAt: null,
    revisionNo: 1002,
    summary: "Current threshold revision queued for an offline node.",
  },
];

const notificationRecipients: RecipientRecord[] = [
  {
    id: "rec-001",
    displayName: "Ops Primary",
    emailAddress: "ops@pyronet.example",
    isEnabled: true,
  },
  {
    id: "rec-002",
    displayName: "Fire Analyst",
    emailAddress: "analyst@pyronet.example",
    isEnabled: true,
  },
];

const notificationPreferences = new Map<string, NotificationEventType[]>([
  ["rec-001", ["critical_risk", "connectivity_loss", "system", "config_update_failure"]],
  ["rec-002", ["critical_risk", "connectivity_loss"]],
]);

const deliveries: DeliveryRecord[] = [
  {
    id: "nd-001",
    recipientId: "rec-001",
    recipientName: "Ops Primary",
    eventType: "critical_risk",
    subject: "Critical risk detected at node-002",
    status: "sent",
    occurredAt: "2026-04-14T19:52:00Z",
    deliveredAt: "2026-04-14T19:53:00Z",
    nodeId: "node-002",
    alertId: "alert-001",
    failureReason: null,
  },
  {
    id: "nd-002",
    recipientId: "rec-002",
    recipientName: "Fire Analyst",
    eventType: "critical_risk",
    subject: "Critical risk detected at node-002",
    status: "queued",
    occurredAt: "2026-04-14T19:52:00Z",
    deliveredAt: null,
    nodeId: "node-002",
    alertId: "alert-001",
    failureReason: null,
  },
  {
    id: "nd-003",
    recipientId: "rec-001",
    recipientName: "Ops Primary",
    eventType: "connectivity_loss",
    subject: "Connectivity loss derived for node-003",
    status: "sent",
    occurredAt: "2026-04-14T10:30:00Z",
    deliveredAt: "2026-04-14T10:31:00Z",
    nodeId: "node-003",
    alertId: null,
    failureReason: null,
  },
];

function timestampMs(value: string | null) {
  return value ? new Date(value).getTime() : 0;
}

function getNowMs() {
  return new Date(NOW).getTime();
}

function getLatestReading(deviceId: string) {
  return [...readings]
    .filter((reading) => reading.deviceId === deviceId)
    .sort((left, right) => timestampMs(right.reportedAt) - timestampMs(left.reportedAt))[0] ?? null;
}

function getConnectivity(lastSeenAt: string | null): ConnectivityStatus {
  if (!lastSeenAt) {
    return "offline";
  }

  const ageMs = getNowMs() - timestampMs(lastSeenAt);

  if (ageMs >= OFFLINE_THRESHOLD_MS) {
    return "offline";
  }

  if (ageMs >= 30 * 60 * 1000) {
    return "degraded";
  }

  return "online";
}

function toNodeSummary(device: DeviceRecord): NodeSummary {
  const latestTelemetry = getLatestReading(device.id);

  return {
    id: device.id,
    nodeId: device.nodeId,
    displayName: device.displayName,
    ipv6Address: device.ipv6Address,
    connectivity: getConnectivity(device.lastSeenAt),
    location: device.location,
    firmwareVersion: device.firmwareVersion,
    firstRegisteredAt: device.firstRegisteredAt,
    lastRegisteredAt: device.lastRegisteredAt,
    lastSeenAt: device.lastSeenAt,
    lastReportedAt: device.lastReportedAt,
    currentRiskLevel: latestTelemetry?.riskLevel ?? null,
    activeConfigRevisionId: device.activeConfigRevisionId,
    activeConfigRevisionNo: device.activeConfigRevisionNo,
    currentNeighborRevisionId: device.currentNeighborRevisionId,
    currentNeighborRevisionNo: device.currentNeighborRevisionNo,
    latestTelemetry,
  };
}

function getNeighborMembershipsForRevision(revisionId: number): NeighborMembership[] {
  return memberships
    .filter((membership) => membership.revisionId === revisionId)
    .sort((left, right) => left.rank - right.rank)
    .map((membership) => {
      const neighbor = devices.find((device) => device.id === membership.neighborDeviceId);
      if (!neighbor) {
        throw new Error(`Unknown neighbor ${membership.neighborDeviceId}`);
      }
      const latest = getLatestReading(neighbor.id);
      return {
        neighborId: neighbor.id,
        neighborNodeId: neighbor.nodeId,
        neighborName: neighbor.displayName,
        rank: membership.rank,
        distanceMeters: membership.distanceMeters,
        location: neighbor.location,
        riskLevel: latest?.riskLevel ?? null,
        connectivity: getConnectivity(neighbor.lastSeenAt),
      };
    });
}

function getNeighborRevisionForDevice(deviceId: string): NeighborRevision | null {
  const revision = neighborRevisions.find((entry) => entry.deviceId === deviceId);
  if (!revision) {
    return null;
  }
  return {
    id: revision.id,
    revisionNo: revision.revisionNo,
    radiusMeters: revision.radiusMeters,
    revisionSource: revision.revisionSource,
    activeFrom: revision.activeFrom,
    neighbors: getNeighborMembershipsForRevision(revision.id),
  };
}

function getNotificationStatusForAlert(alertId: string, nodeId: string) {
  const alertDeliveries = deliveries.filter(
    (delivery) => delivery.alertId === alertId || (delivery.alertId === null && delivery.nodeId === nodeId),
  );

  if (alertDeliveries.length === 0) {
    return "skipped" as const;
  }

  if (alertDeliveries.every((delivery) => delivery.status === "sent")) {
    return "sent" as const;
  }

  if (alertDeliveries.some((delivery) => delivery.status === "sent")) {
    return "partial" as const;
  }

  return "pending" as const;
}

function toAlertIncident(alert: AlertRecord): AlertIncident {
  const node = devices.find((device) => device.id === alert.deviceId);
  if (!node) {
    throw new Error(`Unknown device ${alert.deviceId}`);
  }
  const latestSnapshot = getLatestReading(node.id);

  return {
    id: alert.id,
    incidentType: "critical_alert",
    eventCode: "0x03",
    nodeId: node.nodeId,
    nodeName: node.displayName,
    severity: alert.severity,
    status: alert.status,
    title: alert.title,
    summary: alert.summary,
    occurredAt: alert.occurredAt,
    detectedAt: alert.detectedAt,
    latestEventAt: alert.latestEventAt,
    locationLabel: node.location.label,
    notificationStatus: getNotificationStatusForAlert(alert.id, node.nodeId),
    visibleWithinSla: timestampMs(alert.detectedAt) - timestampMs(alert.occurredAt) <= SLA_THRESHOLD_MS,
    latestSnapshot,
  };
}

function getOfflineIncidents(): AlertIncident[] {
  const incidents: AlertIncident[] = [];

  devices.forEach((device) => {
      const connectivity = getConnectivity(device.lastSeenAt);
      if (connectivity !== "offline") {
        return;
      }
      const offlineAt = new Date(timestampMs(device.lastSeenAt) + OFFLINE_THRESHOLD_MS).toISOString();
      incidents.push({
        id: `offline-${device.id}`,
        incidentType: "offline",
        eventCode: "derived-offline",
        nodeId: device.nodeId,
        nodeName: device.displayName,
        severity: "warning" as const,
        status: "derived" as const,
        title: `${device.displayName} silent for 24 hours`,
        summary: "Derived offline incident because the node has not checked in for more than 24 hours.",
        occurredAt: offlineAt,
        detectedAt: offlineAt,
        latestEventAt: offlineAt,
        locationLabel: device.location.label,
        notificationStatus: getNotificationStatusForAlert("", device.nodeId),
        visibleWithinSla: true,
        latestSnapshot: getLatestReading(device.id),
      });
    });

  return incidents;
}

function listAllAlerts() {
  return [...alerts.map(toAlertIncident), ...getOfflineIncidents()].sort(
    (left, right) => timestampMs(right.detectedAt) - timestampMs(left.detectedAt),
  );
}

function getRecentReadings(deviceId: string, hours = 24) {
  const cutoffMs = getNowMs() - hours * 60 * 60 * 1000;
  return readings
    .filter((reading) => reading.deviceId === deviceId && timestampMs(reading.reportedAt) >= cutoffMs)
    .sort((left, right) => timestampMs(right.reportedAt) - timestampMs(left.reportedAt))
    .map((reading) => ({
      ...reading,
      nodeId: devices.find((device) => device.id === reading.deviceId)?.nodeId ?? reading.deviceId,
    }));
}

function buildMeshLinks(): MeshLink[] {
  return devices.flatMap((device) => {
    const ownerRevision = getNeighborRevisionForDevice(device.id);
    if (!ownerRevision) {
      return [];
    }

    return ownerRevision.neighbors.map((neighbor) => ({
      ownerNodeId: device.nodeId,
      neighborNodeId: neighbor.neighborNodeId,
      ownerName: device.displayName,
      neighborName: neighbor.neighborName,
      distanceMeters: neighbor.distanceMeters,
      points: [device.location, neighbor.location] as [typeof device.location, typeof neighbor.location],
    }));
  });
}

function toDownlinkActivity(record: DownlinkRecord): DownlinkActivity {
  const device = devices.find((entry) => entry.id === record.deviceId);
  if (!device) {
    throw new Error(`Unknown device ${record.deviceId}`);
  }

  return {
    id: record.id,
    commandCode: record.commandCode,
    commandName: record.commandName,
    nodeId: device.nodeId,
    nodeName: device.displayName,
    status: record.status,
    sentAt: record.sentAt,
    acknowledgedAt: record.acknowledgedAt,
    revisionNo: record.revisionNo,
    summary: record.summary,
  };
}

function getAggregateBuckets(deviceId: string, window: HistoryWindow): HistoryAggregateBucket[] {
  const selectedReadings = [...readings]
    .filter((reading) => reading.deviceId === deviceId)
    .sort((left, right) => timestampMs(left.reportedAt) - timestampMs(right.reportedAt));

  if (window === "24h") {
    return selectedReadings.map((reading) => ({
      bucketStart: reading.reportedAt,
      bucketEnd: new Date(timestampMs(reading.reportedAt) + 60 * 60 * 1000).toISOString(),
      sampleCount: 1,
      avgTemperatureC: reading.temperatureC,
      avgHumidityPct: reading.humidityPct,
      avgVocIaq: reading.vocIaq,
      avgPm25UgM3: reading.pm25UgM3,
      maxRiskLevel: reading.riskLevel,
    }));
  }

  const dailyOffset = window === "7d" ? 7 : 30;
  return Array.from({ length: Math.min(dailyOffset, 6) }).map((_, index) => {
    const ageDays = Math.min(dailyOffset - index, 6);
    const bucketStartMs = getNowMs() - ageDays * 24 * 60 * 60 * 1000;
    const nodeLatest = getLatestReading(deviceId);
    const offset = dailyOffset - ageDays;
    return {
      bucketStart: new Date(bucketStartMs).toISOString(),
      bucketEnd: new Date(bucketStartMs + 24 * 60 * 60 * 1000).toISOString(),
      sampleCount: 24,
      avgTemperatureC: nodeLatest ? Number((nodeLatest.temperatureC - 2 + offset * 0.3).toFixed(1)) : null,
      avgHumidityPct: nodeLatest ? Number((nodeLatest.humidityPct + 4 - offset * 0.4).toFixed(1)) : null,
      avgVocIaq: nodeLatest ? Math.round(nodeLatest.vocIaq - 20 + offset * 4) : null,
      avgPm25UgM3: nodeLatest ? Number((nodeLatest.pm25UgM3 - 6 + offset * 0.8).toFixed(1)) : null,
      maxRiskLevel: nodeLatest?.riskLevel ?? null,
    };
  });
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

function toRecipient(record: RecipientRecord): NotificationRecipient {
  const enabled = new Set(notificationPreferences.get(record.id) ?? []);
  const eventTypes: NotificationEventType[] = [
    "critical_risk",
    "connectivity_loss",
    "battery_degradation",
    "system",
    "time_sync_failure",
    "nn_update_failure",
    "config_update_failure",
  ];

  return {
    id: record.id,
    displayName: record.displayName,
    emailAddress: record.emailAddress,
    isEnabled: record.isEnabled,
    preferences: eventTypes.map((eventType) => ({
      eventType,
      isEnabled: enabled.has(eventType),
    })),
  };
}

export function getMockDashboard(): DashboardResponse {
  const fleet = devices.map(toNodeSummary).sort((left, right) => {
    return (right.currentRiskLevel ?? 0) - (left.currentRiskLevel ?? 0);
  });
  const alertQueue = listAllAlerts();
  const summary = {
    totalNodes: fleet.length,
    onlineNodes: fleet.filter((node) => node.connectivity === "online").length,
    degradedNodes: fleet.filter((node) => node.connectivity === "degraded").length,
    offlineNodes: fleet.filter((node) => node.connectivity === "offline").length,
    criticalAlerts: alertQueue.filter((alert) => alert.incidentType === "critical_alert" && alert.status !== "cleared").length,
    offlineIncidents: alertQueue.filter((alert) => alert.incidentType === "offline").length,
    pendingDownlinks: downlinks.filter((downlink) => downlink.status === "pending" || downlink.status === "sent").length,
  };

  return {
    summary,
    fleet,
    neighborLinks: buildMeshLinks(),
    alertQueue,
    downlinks: downlinks.map(toDownlinkActivity).sort((left, right) => timestampMs(right.sentAt) - timestampMs(left.sentAt)),
  };
}

export function listMockNodes(): NodeSummary[] {
  return devices.map(toNodeSummary).sort((left, right) => left.nodeId.localeCompare(right.nodeId));
}

export function getMockNodeDetail(nodeId: string): NodeDetail {
  const device = devices.find((entry) => entry.nodeId === nodeId || entry.id === nodeId);
  if (!device) {
    throw new Error(`Unknown node ${nodeId}`);
  }

  return {
    node: toNodeSummary(device),
    currentNeighborRevision: getNeighborRevisionForDevice(device.id),
    recentReadings: getRecentReadings(device.id, 24),
    alertTimeline: timeline
      .filter((entry) => entry.deviceId === device.id)
      .sort((left, right) => timestampMs(right.occurredAt) - timestampMs(left.occurredAt)),
    ipv6History: ipv6History.filter((entry) => entry.deviceId === device.id),
    recentRegistrations: registrations
      .filter((entry) => entry.deviceId === device.id)
      .sort((left, right) => timestampMs(right.observedAt) - timestampMs(left.observedAt)),
  };
}

export function listMockAlerts(): AlertIncident[] {
  return listAllAlerts();
}

export function getMockHistory(nodeId?: string, window: HistoryWindow = "24h"): HistoryResponse {
  const selectedNode = devices.find((device) => device.nodeId === nodeId) ?? devices[0];
  const rawReadings = window === "24h" ? getRecentReadings(selectedNode.id, 24) : [];

  return {
    selectedNodeId: selectedNode.nodeId,
    selectedWindow: window,
    mode: window === "24h" ? "raw" : "aggregate",
    availableNodes: devices.map((device) => ({
      id: device.id,
      nodeId: device.nodeId,
      displayName: device.displayName,
    })),
    rawReadings,
    aggregateBuckets: getAggregateBuckets(selectedNode.id, window),
    trendSummary: buildTrendSummary(rawReadings),
  };
}

export function getMockConfiguration(): ConfigurationResponse {
  const activeRevision = [...configRevisions]
    .sort((left, right) => timestampMs(right.createdAt) - timestampMs(left.createdAt))
    .find((revision) => revision.retiredAt === null) ?? null;

  return {
    activeRevision,
    revisions: [...configRevisions].sort((left, right) => timestampMs(right.createdAt) - timestampMs(left.createdAt)),
    neighborTables: devices.map((device) => {
      const revision = getNeighborRevisionForDevice(device.id);
      return {
        nodeId: device.nodeId,
        nodeName: device.displayName,
        revisionId: revision?.id ?? null,
        revisionNo: revision?.revisionNo ?? null,
        radiusMeters: revision?.radiusMeters ?? null,
        neighbors: revision?.neighbors ?? [],
      };
    }),
    downlinks: downlinks.map(toDownlinkActivity).sort((left, right) => timestampMs(right.sentAt) - timestampMs(left.sentAt)),
  };
}

export function createMockConfigRevision(draft: ConfigRevisionDraft): ConfigurationResponse {
  const nextId = Math.max(...configRevisions.map((revision) => revision.id)) + 1;
  const nextConfigId = Math.max(...configRevisions.map((revision) => revision.configId)) + 1;
  const currentActive = configRevisions.find((revision) => revision.retiredAt === null);

  if (currentActive) {
    currentActive.retiredAt = NOW;
  }

  configRevisions.push({
    id: nextId,
    configId: nextConfigId,
    activatedAt: NOW,
    retiredAt: null,
    createdAt: NOW,
    notes: draft.notes ?? null,
    thresholds: draft.thresholds,
  });

  const targetNodeIds = draft.targetNodeIds?.length ? draft.targetNodeIds : devices.map((device) => device.nodeId);
  targetNodeIds.forEach((nodeId, index) => {
    const device = devices.find((entry) => entry.nodeId === nodeId);
    if (!device) {
      return;
    }
    device.activeConfigRevisionId = nextId;
    device.activeConfigRevisionNo = nextConfigId;
    downlinks.unshift({
      id: `dl-0x06-${nextId}-${index}`,
      commandCode: "0x06",
      commandName: "Threshold revision deployment",
      deviceId: device.id,
      status: "sent",
      sentAt: NOW,
      acknowledgedAt: null,
      revisionNo: nextConfigId,
      summary: `Revision ${nextConfigId} queued for ${device.displayName}.`,
    });
  });

  return getMockConfiguration();
}

export function updateMockNeighborRevision(nodeId: string, draft: NeighborRevisionDraft): ConfigurationResponse {
  const device = devices.find((entry) => entry.nodeId === nodeId);
  if (!device) {
    throw new Error(`Unknown node ${nodeId}`);
  }

  const existingRevision = neighborRevisions.find((entry) => entry.id === device.currentNeighborRevisionId);
  const nextRevisionId = Math.max(...neighborRevisions.map((revision) => revision.id)) + 1;
  const nextRevisionNo = (existingRevision?.revisionNo ?? 0) + 1;

  neighborRevisions.push({
    id: nextRevisionId,
    deviceId: device.id,
    revisionNo: nextRevisionNo,
    radiusMeters: draft.radiusMeters,
    revisionSource: "manual",
    activeFrom: NOW,
  });

  for (let index = memberships.length - 1; index >= 0; index -= 1) {
    if (memberships[index]?.ownerDeviceId === device.id) {
      memberships.splice(index, 1);
    }
  }

  draft.neighborNodeIds.forEach((neighborNodeId, index) => {
    const neighbor = devices.find((entry) => entry.nodeId === neighborNodeId);
    if (!neighbor) {
      return;
    }
    memberships.push({
      revisionId: nextRevisionId,
      ownerDeviceId: device.id,
      neighborDeviceId: neighbor.id,
      rank: index + 1,
      distanceMeters: 500 + index * 250,
    });
  });

  device.currentNeighborRevisionId = nextRevisionId;
  device.currentNeighborRevisionNo = nextRevisionNo;

  downlinks.unshift({
    id: `dl-0x04-${nextRevisionId}`,
    commandCode: "0x04",
    commandName: "Neighbor table distribution",
    deviceId: device.id,
    status: "sent",
    sentAt: NOW,
    acknowledgedAt: null,
    revisionNo: nextRevisionNo,
    summary: `Revision ${nextRevisionNo} queued for ${device.displayName}.`,
  });

  return getMockConfiguration();
}

export function createMockTimeSync(request: DownlinkRequest): ConfigurationResponse {
  const targetNodeIds = request.targetNodeIds?.length ? request.targetNodeIds : devices.map((device) => device.nodeId);
  targetNodeIds.forEach((nodeId, index) => {
    const device = devices.find((entry) => entry.nodeId === nodeId);
    if (!device) {
      return;
    }
    downlinks.unshift({
      id: `dl-0x05-${Date.now()}-${index}`,
      commandCode: "0x05",
      commandName: "Daily time synchronization",
      deviceId: device.id,
      status: "sent",
      sentAt: NOW,
      acknowledgedAt: null,
      revisionNo: null,
      summary: `Manual time sync dispatched to ${device.displayName}.`,
    });
  });

  return getMockConfiguration();
}

export function createMockNeighborDistribution(request: DownlinkRequest): ConfigurationResponse {
  const targetNodeIds = request.targetNodeIds?.length ? request.targetNodeIds : devices.map((device) => device.nodeId);
  targetNodeIds.forEach((nodeId, index) => {
    const device = devices.find((entry) => entry.nodeId === nodeId);
    if (!device) {
      return;
    }
    downlinks.unshift({
      id: `dl-0x04-manual-${Date.now()}-${index}`,
      commandCode: "0x04",
      commandName: "Neighbor table distribution",
      deviceId: device.id,
      status: "sent",
      sentAt: NOW,
      acknowledgedAt: null,
      revisionNo: device.currentNeighborRevisionNo,
      summary: `Current neighbor table re-pushed to ${device.displayName}.`,
    });
  });

  return getMockConfiguration();
}

export function createMockThresholdPush(request: DownlinkRequest): ConfigurationResponse {
  const revisionNo =
    configRevisions.find((revision) => revision.id === request.configRevisionId)?.configId ??
    configRevisions.find((revision) => revision.retiredAt === null)?.configId ??
    null;

  const targetNodeIds = request.targetNodeIds?.length ? request.targetNodeIds : devices.map((device) => device.nodeId);
  targetNodeIds.forEach((nodeId, index) => {
    const device = devices.find((entry) => entry.nodeId === nodeId);
    if (!device) {
      return;
    }
    downlinks.unshift({
      id: `dl-0x06-manual-${Date.now()}-${index}`,
      commandCode: "0x06",
      commandName: "Threshold revision deployment",
      deviceId: device.id,
      status: "sent",
      sentAt: NOW,
      acknowledgedAt: null,
      revisionNo,
      summary: `Revision ${revisionNo ?? "active"} re-pushed to ${device.displayName}.`,
    });
  });

  return getMockConfiguration();
}

export function getMockNotificationSettings(): NotificationSettingsResponse {
  return {
    recipients: notificationRecipients.map(toRecipient),
    deliveries: [...deliveries].sort((left, right) => timestampMs(right.occurredAt) - timestampMs(left.occurredAt)),
  };
}

export function updateMockNotificationRecipient(
  recipientId: string,
  update: NotificationRecipientUpdate,
): NotificationSettingsResponse {
  const recipient = notificationRecipients.find((entry) => entry.id === recipientId);
  if (!recipient) {
    throw new Error(`Unknown recipient ${recipientId}`);
  }

  recipient.isEnabled = update.isEnabled;
  notificationPreferences.set(recipientId, update.enabledEventTypes);
  return getMockNotificationSettings();
}
