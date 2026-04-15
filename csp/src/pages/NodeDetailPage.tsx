import type { KeyboardEvent } from "react";
import { useState } from "react";
import { useNavigate, useParams } from "react-router-dom";
import { getHistory } from "../api/history";
import { getNodeDetail } from "../api/nodes";
import { EmptyState } from "../components/common/EmptyState";
import { LoadingState } from "../components/common/LoadingState";
import { PageContainer } from "../components/common/PageContainer";
import { StatCard } from "../components/common/StatCard";
import { TableShell } from "../components/common/TableShell";
import { TelemetryTrendChart, telemetryMeasurementOptions, type TelemetryMeasurementId } from "../components/common/TelemetryTrendChart";
import { MeshMap } from "../features/map/MeshMap";
import { formatCoordinatePair, formatInteger, formatNullableNumber, formatTimestamp, riskLabel } from "../lib/format";
import { parseNodeId } from "../lib/nodeId";
import { useAsyncData } from "../lib/useAsyncData";

export function NodeDetailPage() {
  const navigate = useNavigate();
  const params = useParams();
  const nodeId = parseNodeId(params.nodeId);
  const [selectedMeasurement, setSelectedMeasurement] = useState<TelemetryMeasurementId>("temperatureC");

  function openNeighborDetail(neighborNodeId: number) {
    navigate(`/nodes/${neighborNodeId}`);
  }

  function handleNeighborRowKeyDown(event: KeyboardEvent<HTMLTableRowElement>, neighborNodeId: number) {
    if (event.key !== "Enter" && event.key !== " ") {
      return;
    }

    event.preventDefault();
    openNeighborDetail(neighborNodeId);
  }

  if (nodeId === null) {
    return <EmptyState title="Invalid node detail request" message="Node IDs must be valid 2-byte integers." />;
  }

  const { data, error, loading } = useAsyncData(() => getNodeDetail(nodeId), [nodeId]);
  const {
    data: historyData,
    error: historyError,
    loading: historyLoading,
  } = useAsyncData(
    async () => {
      const [dayHistory, weekHistory, monthHistory] = await Promise.all([
        getHistory(nodeId, "24h"),
        getHistory(nodeId, "7d"),
        getHistory(nodeId, "30d"),
      ]);

      return {
        "24h": dayHistory,
        "7d": weekHistory,
        "30d": monthHistory,
      };
    },
    [nodeId],
  );

  if (loading) {
    return <LoadingState label="Loading node detail..." />;
  }

  if (error || !data) {
    return <EmptyState title="Unable to load node detail" message={error ?? "Node detail is unavailable."} />;
  }

  return (
    <PageContainer
      title={`Node ${data.node.nodeId}`}
    >
      <div className="stat-grid node-detail-stat-grid">
        <StatCard label="Temperature" value={formatNullableNumber(data.node.latestTelemetry?.temperatureC ?? null, "°C")} />
        <StatCard label="Humidity" value={formatNullableNumber(data.node.latestTelemetry?.humidityPct ?? null, "%")} />
        <StatCard label="VOC" value={formatInteger(data.node.latestTelemetry?.vocIaq ?? null)} />
        <StatCard label="PM2.5" value={formatNullableNumber(data.node.latestTelemetry?.pm25UgM3 ?? null, " ug/m3")} />
        <StatCard label="Battery" value={formatInteger(data.node.latestTelemetry?.batteryPct ?? null, "%")} />
        <StatCard label="Latest risk" value={riskLabel(data.node.currentRiskLevel)} />
      </div>

      <div className="dashboard-grid dashboard-grid-stack node-detail-hero">
        <article className="card map-card">
          <div className="section-heading">
            <div>
              <h2>Node map</h2>
            </div>
          </div>
          <div className="map-container">
            <MeshMap nodes={[data.node]} links={[]} focusNodeId={data.node.nodeId} focusZoom={16} />
          </div>
        </article>

        <div className="stack-grid">
          <article className="card">
            <div className="section-heading">
              <div>
                <h2>Identity and network</h2>
              </div>
            </div>
            <dl className="detail-list">
              <div>
                <dt>Node ID</dt>
                <dd>{data.node.nodeId}</dd>
              </div>
              <div>
                <dt>IPv6</dt>
                <dd>{data.node.ipv6Address ?? "N/A"}</dd>
              </div>
              <div>
                <dt>Firmware</dt>
                <dd>{data.node.firmwareVersion ?? "N/A"}</dd>
              </div>
              <div>
                <dt>Coordinates</dt>
                <dd>{formatCoordinatePair(data.node.location.lat, data.node.location.lng)}</dd>
              </div>
              <div>
                <dt>Last seen</dt>
                <dd>{formatTimestamp(data.node.lastSeenAt)}</dd>
              </div>
              <div>
                <dt>Active config revision</dt>
                <dd>{data.node.activeConfigRevisionNo ?? "N/A"}</dd>
              </div>
            </dl>
          </article>

        </div>
      </div>

      <article className="card history-card">
        <div className="section-heading history-card-heading">
          <div>
            <h2>Historical Graph</h2>
          </div>
          <label className="field-inline history-card-select">
            <span>Measurement</span>
            <select value={selectedMeasurement} onChange={(event) => setSelectedMeasurement(event.target.value as TelemetryMeasurementId)}>
              {telemetryMeasurementOptions.map((option) => (
                <option key={option.id} value={option.id}>
                  {option.label}
                </option>
              ))}
            </select>
          </label>
        </div>

        {historyLoading ? <LoadingState label="Loading historical telemetry..." /> : null}
        {!historyLoading && (historyError || !historyData) ? (
          <EmptyState title="Unable to load telemetry history" message={historyError ?? "Historical telemetry is unavailable."} />
        ) : null}
        {!historyLoading && historyData ? <TelemetryTrendChart historyByWindow={historyData} measurementId={selectedMeasurement} /> : null}
      </article>

      <div className="card node-detail-neighbor-card">
        <div className="section-heading">
          <div>
            <h2>Current Neighbor Table</h2>
          </div>
        </div>
        {data.currentNeighborRevision ? (
          <TableShell className="neighbor-table" columns={["Neighbor ID", "Distance", "Risk", "Connectivity"]}>
            {data.currentNeighborRevision.neighbors.map((neighbor) => (
              <tr
                key={neighbor.neighborNodeId}
                className="neighbor-table-row"
                onClick={() => openNeighborDetail(neighbor.neighborNodeId)}
                onKeyDown={(event) => handleNeighborRowKeyDown(event, neighbor.neighborNodeId)}
                role="link"
                tabIndex={0}
                aria-label={`Open node ${neighbor.neighborNodeId} detail`}
              >
                <td>{neighbor.neighborNodeId}</td>
                <td>{neighbor.distanceMeters} m</td>
                <td>{riskLabel(neighbor.riskLevel)}</td>
                <td>
                  <span className={`badge status-${neighbor.connectivity}`}>{neighbor.connectivity}</span>
                </td>
              </tr>
            ))}
          </TableShell>
        ) : (
          <EmptyState title="No neighbor revision" message="This node does not have an active NN table." />
        )}
      </div>

      <div className="split-grid">
        <div className="card">
          <div className="section-heading">
            <div>
              <h2>Recent telemetry</h2>
              <p>Most recent raw telemetry samples for the last 24 hours.</p>
            </div>
          </div>
          <TableShell columns={["Reported", "Source", "Risk", "Temp", "RH", "VOC", "PM2.5"]}>
            {data.recentReadings.map((reading) => (
              <tr key={reading.id}>
                <td>{formatTimestamp(reading.reportedAt)}</td>
                <td>{reading.sourceType}</td>
                <td>{riskLabel(reading.riskLevel)}</td>
                <td>{formatNullableNumber(reading.temperatureC, "°C")}</td>
                <td>{formatNullableNumber(reading.humidityPct, "%")}</td>
                <td>{formatInteger(reading.vocIaq)}</td>
                <td>{formatNullableNumber(reading.pm25UgM3, " ug/m3")}</td>
              </tr>
            ))}
          </TableShell>
        </div>

        <div className="card">
          <div className="section-heading">
            <div>
              <h2>Alert timeline</h2>
              <p>Per-node incident and operator timeline.</p>
            </div>
          </div>
          <TableShell columns={["When", "Event", "Status", "Actor"]}>
            {data.alertTimeline.map((event) => (
              <tr key={event.id}>
                <td>{formatTimestamp(event.occurredAt)}</td>
                <td>
                  {event.title}
                  <div className="table-subtle">{event.summary}</div>
                </td>
                <td>{event.status}</td>
                <td>{event.actor ?? "system"}</td>
              </tr>
            ))}
          </TableShell>
        </div>
      </div>

      <div className="split-grid">
        <div className="card">
          <div className="section-heading">
            <div>
              <h2>Registration history</h2>
              <p>Recent 0x01 register or re-register events.</p>
            </div>
          </div>
          <TableShell columns={["Observed", "IPv6", "Coordinates", "Firmware", "Battery"]}>
            {data.recentRegistrations.map((registration) => (
              <tr key={`${registration.observedAt}-${registration.ipv6Address}`}>
                <td>{formatTimestamp(registration.observedAt)}</td>
                <td>{registration.ipv6Address}</td>
                <td>{formatCoordinatePair(registration.latitude, registration.longitude)}</td>
                <td>{registration.firmwareVersion ?? "N/A"}</td>
                <td>{formatInteger(registration.batteryPct, "%")}</td>
              </tr>
            ))}
          </TableShell>
        </div>

        <div className="card">
          <div className="section-heading">
            <div>
              <h2>IPv6 history</h2>
              <p>Current and historical address bindings for the node.</p>
            </div>
          </div>
          <TableShell columns={["Address", "Valid from", "Valid to"]}>
            {data.ipv6History.map((entry) => (
              <tr key={`${entry.address}-${entry.validFrom}`}>
                <td>{entry.address}</td>
                <td>{formatTimestamp(entry.validFrom)}</td>
                <td>{formatTimestamp(entry.validTo)}</td>
              </tr>
            ))}
          </TableShell>
        </div>
      </div>
    </PageContainer>
  );
}
