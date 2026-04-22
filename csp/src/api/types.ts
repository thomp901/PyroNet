export type ConnectivityStatus = "online" | "degraded" | "offline";
export type SensorReadingSource = "periodic_report" | "critical_alert";
export type AlertSeverity = "info" | "warning" | "critical";
export type AlertStatus = "open" | "acknowledged" | "cleared" | "derived";
export type AlertIncidentType =
  | "critical_alert"
  | "battery_health_low"
  | "offline"
  | "time_sync_failure"
  | "nn_update_failure"
  | "config_update_failure";
<<<<<<< HEAD
export type DeviceEventCode = "0x01" | "0x02" | "0x03" | "0x04" | "0x05" | "0x06" | "0x07";
=======
export type DownlinkStatus = "pending" | "sent" | "acknowledged" | "failed" | "timed_out";
export type NeighborRevisionSource = "automatic" | "manual" | "imported";
export const notificationEventTypes = [
  "critical_risk",
  "node_registration",
  "connectivity_loss",
  "battery_degradation",
  "time_sync_failure",
  "nn_update_failure",
  "config_update_failure",
] as const;
export type NotificationEventType = (typeof notificationEventTypes)[number];
export function isNotificationEventType(value: string): value is NotificationEventType {
  return (notificationEventTypes as readonly string[]).includes(value);
}
export type NotificationChannel = "email" | "sms";
export type DeviceEventCode = "0x01" | "0x02" | "0x03" | "0x04" | "0x05" | "0x06" | "0x07" | "0x08";
>>>>>>> 8ed2bba (notifications)
export type HistoryWindow = "24h" | "7d" | "30d";
export type NodeId = number;
export const packetDirections = ["uplink", "downlink"] as const;
export type PacketDirection = (typeof packetDirections)[number];
export const packetLogCodes = ["0x01", "0x02", "0x03", "0x04", "0x05", "0x06"] as const;
export type PacketLogCode = (typeof packetLogCodes)[number];
export const packetEventTypes = [
  "registration",
  "periodic_report",
  "critical_alert",
  "neighbor_distribution",
  "time_sync",
  "config_deployment",
] as const;
export type PacketEventType = (typeof packetEventTypes)[number];
export type PacketLogStatus = "received" | DownlinkStatus;

export interface Coordinate {
  lat: number;
  lng: number;
  label: string;
}

export interface TelemetrySnapshot {
  reportedAt: string;
  sourceType: SensorReadingSource;
  riskLevel: number;
  temperatureC: number;
  humidityPct: number;
  vocIaq: number;
  pm25UgM3: number;
  batteryPct?: number | null;
  pressureHpa?: number | null;
  batteryHealthScore?: number | null;
}

export interface NodeSummary {
  id: string;
  nodeId: NodeId;
  ipv6Address: string | null;
  connectivity: ConnectivityStatus;
  location: Coordinate;
  firmwareVersion: string | null;
  firstRegisteredAt: string;
  lastRegisteredAt: string;
  lastSeenAt: string | null;
  lastReportedAt: string | null;
  currentRiskLevel: number | null;
  activeConfigRevisionId: number | null;
  activeConfigRevisionNo: number | null;
  currentNeighborRevisionId: number | null;
  currentNeighborRevisionNo: number | null;
  latestTelemetry: TelemetrySnapshot | null;
}

export interface NeighborMembership {
  neighborId: NodeId;
  neighborNodeId: NodeId;
  neighborName: string;
  rank: number;
  distanceMeters: number;
  location: Coordinate;
  riskLevel: number | null;
  connectivity: ConnectivityStatus;
}

export interface NeighborRevision {
  id: number;
  revisionNo: number;
  radiusMeters: number;
  revisionSource: NeighborRevisionSource;
  activeFrom: string;
  neighbors: NeighborMembership[];
}

export interface MeshLink {
  ownerNodeId: NodeId;
  neighborNodeId: NodeId;
  ownerName: string;
  neighborName: string;
  distanceMeters: number;
  points: [Coordinate, Coordinate];
}

export interface AlertIncident {
  id: string;
  incidentType: AlertIncidentType;
  eventCode: DeviceEventCode | "battery-health-low" | "derived-offline";
  nodeId: NodeId;
  nodeName: string;
  severity: AlertSeverity;
  status: AlertStatus;
  title: string;
  summary: string;
  occurredAt: string;
  detectedAt: string;
  latestEventAt: string;
  locationLabel: string;
  notificationStatus: "sent" | "partial" | "pending" | "skipped";
  lastSeenAt?: string | null;
  latestSnapshot: TelemetrySnapshot | null;
}

export interface AlertTimelineEntry {
  id: string;
  eventCode: DeviceEventCode | "battery-health-low" | "derived-offline";
  title: string;
  summary: string;
  occurredAt: string;
  status: string;
  severity: AlertSeverity;
  actor: string | null;
}

export interface ReadingHistoryPoint extends TelemetrySnapshot {
  id: string;
  nodeId: NodeId;
}

export interface HistoryAggregateBucket {
  bucketStart: string;
  bucketEnd: string;
  sampleCount: number;
  avgTemperatureC: number | null;
  avgHumidityPct: number | null;
  avgVocIaq: number | null;
  avgPm25UgM3: number | null;
  maxRiskLevel: number | null;
}

export interface HistoryTrendSummary {
  temperatureDeltaC: number | null;
  humidityDeltaPct: number | null;
  vocPeak: number | null;
  pm25Peak: number | null;
}

export interface NodeRegistration {
  observedAt: string;
  ipv6Address: string;
  latitude: number;
  longitude: number;
  firmwareVersion: string | null;
  batteryPct: number | null;
}

export interface Ipv6HistoryEntry {
  address: string;
  validFrom: string;
  validTo: string | null;
}

export interface NodeDetail {
  node: NodeSummary;
  currentNeighborRevision: NeighborRevision | null;
  recentReadings: ReadingHistoryPoint[];
  alertTimeline: AlertTimelineEntry[];
  ipv6History: Ipv6HistoryEntry[];
  recentRegistrations: NodeRegistration[];
}

export interface DashboardSummary {
  totalNodes: number;
  onlineNodes: number;
  degradedNodes: number;
  offlineNodes: number;
  criticalAlerts: number;
  offlineIncidents: number;
  pendingDownlinks: number;
}

export interface DownlinkActivity {
  id: string;
  commandCode: "0x04" | "0x05" | "0x06";
  commandName: string;
  nodeId: NodeId;
  nodeName: string;
  status: DownlinkStatus;
  sentAt: string;
  acknowledgedAt: string | null;
  revisionNo: number | null;
  summary: string;
}

export interface DashboardResponse {
  summary: DashboardSummary;
  fleet: NodeSummary[];
  neighborLinks: MeshLink[];
  alertQueue: AlertIncident[];
  downlinks: DownlinkActivity[];
  recentPackets: PacketLogEntry[];
}

export interface HistoryNodeOption {
  id: string;
  nodeId: NodeId;
  displayName: string;
}

export interface HistoryResponse {
  selectedNodeId: NodeId;
  selectedWindow: HistoryWindow;
  mode: "raw" | "aggregate";
  availableNodes: HistoryNodeOption[];
  rawReadings: ReadingHistoryPoint[];
  aggregateBuckets: HistoryAggregateBucket[];
  trendSummary: HistoryTrendSummary;
}

export interface PacketLogEntry {
  id: string;
  occurredAt: string;
  nodeId: NodeId;
  nodeName: string;
  direction: PacketDirection;
  packetCode: PacketLogCode;
  eventType: PacketEventType;
  status: PacketLogStatus;
  summary: string;
  detail: string | null;
}

export interface PacketHistoryQuery {
  limit?: number;
  offset?: number;
  nodeId?: NodeId;
  direction?: PacketDirection;
  packetCode?: PacketLogCode;
  eventType?: PacketEventType;
  status?: PacketLogStatus;
}

export interface PacketHistoryResponse {
  entries: PacketLogEntry[];
  totalCount: number;
  limit: number;
  offset: number;
  hasMore: boolean;
  availableNodes: HistoryNodeOption[];
  availableDirections: PacketDirection[];
  availablePacketCodes: PacketLogCode[];
  availableEventTypes: PacketEventType[];
  availableStatuses: PacketLogStatus[];
}

export interface ConfigThresholds {
  l2TempThresh: number;
  l2HumidityThresh: number;
  l2VocThresh: number;
  l3TempThresh: number;
  l3HumidityThresh: number;
  l3VocThresh: number;
  l4VocThresh: number;
  l5VocThresh: number;
  l5Pm25Thresh: number;
}

export interface ConfigRevision {
  id: number;
  configId: number;
  activatedAt: string | null;
  retiredAt: string | null;
  createdAt: string;
  notes: string | null;
  thresholds: ConfigThresholds;
}

export interface NeighborTableRecord {
  nodeId: NodeId;
  nodeName: string;
  revisionId: number | null;
  revisionNo: number | null;
  radiusMeters: number | null;
  neighbors: NeighborMembership[];
}

export interface ConfigurationResponse {
  activeRevision: ConfigRevision | null;
  revisions: ConfigRevision[];
  neighborTables: NeighborTableRecord[];
  downlinks: DownlinkActivity[];
}

export interface ConfigRevisionDraft {
  notes?: string;
  targetNodeIds?: NodeId[];
  thresholds: ConfigThresholds;
}

export interface NeighborRevisionDraft {
  radiusMeters: number;
  neighborNodeIds: NodeId[];
}

export interface DownlinkRequest {
  targetNodeIds?: NodeId[];
  configRevisionId?: number;
}

export interface NotificationPreference {
  eventType: NotificationEventType;
  isEnabled: boolean;
}

export interface NotificationRecipient {
  id: string;
  displayName: string;
  emailAddress: string;
  isEnabled: boolean;
  preferences: NotificationPreference[];
}

export interface NotificationDelivery {
  id: string;
  recipientId: string;
  recipientName: string;
  eventType: NotificationEventType;
  subject: string;
  status: "queued" | "accepted" | "sent" | "failed" | "skipped";
  occurredAt: string;
  deliveredAt: string | null;
  nodeId: NodeId | null;
  alertId: string | null;
  failureReason: string | null;
}

export interface NotificationSettingsResponse {
  recipients: NotificationRecipient[];
  deliveries: NotificationDelivery[];
}

export interface NotificationRecipientUpdate {
  isEnabled: boolean;
  enabledEventTypes: NotificationEventType[];
}
