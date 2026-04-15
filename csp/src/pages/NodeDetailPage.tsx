import { useState } from "react";
import { useParams } from "react-router-dom";
import { getHistory } from "../api/history";
import { getNodeDetail } from "../api/nodes";
import { EmptyState } from "../components/common/EmptyState";
import { LoadingState } from "../components/common/LoadingState";
import { PageContainer } from "../components/common/PageContainer";
import { TableShell } from "../components/common/TableShell";
import { TelemetryTrendChart, telemetryMeasurementOptions, type TelemetryMeasurementId } from "../components/common/TelemetryTrendChart";
import { formatCoordinatePair, formatInteger, formatNullableNumber, formatTimestamp, riskLabel } from "../lib/format";
import { parseNodeId } from "../lib/nodeId";
import { useAsyncData } from "../lib/useAsyncData";

export function NodeDetailPage() {
  const params = useParams();
  const nodeId = parseNodeId(params.nodeId);
  const [selectedMeasurement, setSelectedMeasurement] = useState<TelemetryMeasurementId>("temperatureC");

  if (nodeId === null) {
    return <EmptyState title="Invalid node detail request" message="Node IDs must be valid 2-byte integers." />;
  }

  const { data, error, loading } = useAsyncData(() => getNodeDetail(nodeId), [nodeId]);
  const {
    data: historyData,
    error: historyError,
    loading: historyLoading,
  } = useAsyncData(() => getHistory(nodeId, "24h"), [nodeId]);

  if (loading) {
    return <LoadingState label="Loading node detail..." />;
  }

  if (error || !data) {
    return <EmptyState title="Unable to load node detail" message={error ?? "Node detail is unavailable."} />;
  }

  return (
    <PageContainer
      title={`${data.node.nodeId} Detail`}
      description="Per-device identity, network address, topology, latest telemetry, recent history, and alert timeline."
    >
      <article className="card history-card">
        <div className="section-heading history-card-heading">
          <div>
            <h2>Historical telemetry</h2>
            <p>Full-width 24-hour trend view for the selected sensor measurement.</p>
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
        {!historyLoading && historyData ? <TelemetryTrendChart readings={historyData.rawReadings} measurementId={selectedMeasurement} /> : null}
      </article>

      <div className="split-grid">
        <article className="card">
          <div className="section-heading">
            <div>
              <h2>Identity and network</h2>
              <p>Registration, firmware, location, and current address details.</p>
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
              <dt>Latest risk</dt>
              <dd>{riskLabel(data.node.currentRiskLevel)}</dd>
            </div>
          </dl>
        </article>

        <article className="card">
          <div className="section-heading">
            <div>
              <h2>Latest telemetry</h2>
              <p>Current sensor values and config binding for this device.</p>
            </div>
          </div>
          <dl className="detail-list">
            <div>
              <dt>Temperature</dt>
              <dd>{formatNullableNumber(data.node.latestTelemetry?.temperatureC ?? null, "°C")}</dd>
            </div>
            <div>
              <dt>Humidity</dt>
              <dd>{formatNullableNumber(data.node.latestTelemetry?.humidityPct ?? null, "%")}</dd>
            </div>
            <div>
              <dt>VOC</dt>
              <dd>{formatInteger(data.node.latestTelemetry?.vocIaq ?? null)}</dd>
            </div>
            <div>
              <dt>PM2.5</dt>
              <dd>{formatNullableNumber(data.node.latestTelemetry?.pm25UgM3 ?? null, " ug/m3")}</dd>
            </div>
            <div>
              <dt>Battery</dt>
              <dd>{formatInteger(data.node.latestTelemetry?.batteryPct ?? null, "%")}</dd>
            </div>
            <div>
              <dt>Active config revision</dt>
              <dd>{data.node.activeConfigRevisionNo ?? "N/A"}</dd>
            </div>
          </dl>
        </article>
      </div>

      <div className="card">
        <div className="section-heading">
          <div>
            <h2>Current neighbor table</h2>
            <p>Current NN revision, radius, and nearest-neighbor memberships.</p>
          </div>
        </div>
        {data.currentNeighborRevision ? (
          <>
            <p className="section-meta">
              Revision {data.currentNeighborRevision.revisionNo} / Radius {data.currentNeighborRevision.radiusMeters} m / Source{" "}
              {data.currentNeighborRevision.revisionSource}
            </p>
            <TableShell columns={["Rank", "Neighbor", "Distance", "Risk", "Connectivity"]}>
              {data.currentNeighborRevision.neighbors.map((neighbor) => (
                <tr key={neighbor.neighborNodeId}>
                  <td>{neighbor.rank}</td>
                  <td>{neighbor.neighborNodeId}</td>
                  <td>{neighbor.distanceMeters} m</td>
                  <td>{riskLabel(neighbor.riskLevel)}</td>
                  <td>
                    <span className={`badge status-${neighbor.connectivity}`}>{neighbor.connectivity}</span>
                  </td>
                </tr>
              ))}
            </TableShell>
          </>
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
