import { Link } from "react-router-dom";
import { getDashboard } from "../api/dashboard";
import { EmptyState } from "../components/common/EmptyState";
import { LoadingState } from "../components/common/LoadingState";
import { PageContainer } from "../components/common/PageContainer";
import { StatCard } from "../components/common/StatCard";
import { TableShell } from "../components/common/TableShell";
import { MapPreview } from "../features/map/MapPreview";
import { formatInteger, formatNullableNumber, formatRelativeMinutes, formatTimestamp, riskLabel } from "../lib/format";
import { useAsyncData } from "../lib/useAsyncData";

function latestSensorSummary(temperature?: number | null, humidity?: number | null, voc?: number | null, pm25?: number | null) {
  return [
    `Temp ${formatNullableNumber(temperature ?? null, "°C")}`,
    `RH ${formatNullableNumber(humidity ?? null, "%")}`,
    `VOC ${formatInteger(voc ?? null)}`,
    `PM2.5 ${formatNullableNumber(pm25 ?? null, " ug/m3")}`,
  ].join(" / ");
}

export function DashboardPage() {
  const { data, error, loading } = useAsyncData(getDashboard, []);

  if (loading) {
    return <LoadingState label="Loading mesh overview..." />;
  }

  if (error || !data) {
    return <EmptyState title="Dashboard unavailable" message={error ?? "Unable to load dashboard data."} />;
  }

  return (
    <PageContainer
      title="Operations Dashboard"
      description="Fleet-wide status, mesh topology, active incidents, and downlink activity for the centralized software platform."
    >
      <div className="stat-grid stat-grid-wide">
        <StatCard label="Fleet size" value={String(data.summary.totalNodes)} helper={`${data.summary.onlineNodes} online`} />
        <StatCard label="Degraded nodes" value={String(data.summary.degradedNodes)} helper="Seen but outside healthy freshness band" />
        <StatCard label="Offline incidents" value={String(data.summary.offlineNodes)} helper="Silent for 24 hours or more" />
        <StatCard label="Critical 0x03 alerts" value={String(data.summary.criticalAlerts)} helper="Open or acknowledged incidents" />
        <StatCard label="Pending downlinks" value={String(data.summary.pendingDownlinks)} helper="0x04, 0x05, and 0x06 in flight" />
      </div>

      <div className="dashboard-grid dashboard-grid-stack">
        <MapPreview nodes={data.fleet} links={data.neighborLinks} />

        <div className="card">
          <div className="section-heading">
            <div>
              <h2>Incident queue</h2>
              <p>Critical 0x03 events and derived offline incidents that need operator attention.</p>
            </div>
            <Link className="text-link" to="/alerts">
              Open alerts
            </Link>
          </div>

          <TableShell columns={["Node", "Type", "Severity", "Visible", "Detected"]}>
            {data.alertQueue.slice(0, 6).map((alert) => (
              <tr key={alert.id}>
                <td>
                  <Link className="table-link" to={`/nodes/${alert.nodeId}`}>
                    {alert.nodeId}
                  </Link>
                </td>
                <td>{alert.incidentType === "critical_alert" ? "0x03 critical" : "Offline derived"}</td>
                <td>
                  <span className={`badge severity-${alert.severity}`}>{alert.severity}</span>
                </td>
                <td>{alert.visibleWithinSla ? "Within 10 min" : "SLA miss"}</td>
                <td>{formatTimestamp(alert.detectedAt)}</td>
              </tr>
            ))}
          </TableShell>
        </div>
      </div>

      <div className="card">
        <div className="section-heading">
          <div>
            <h2>Fleet overview</h2>
            <p>Node identity, network address, current risk, location, freshness, and latest sensor values.</p>
          </div>
          <Link className="text-link" to="/nodes">
            Full node view
          </Link>
        </div>

        <TableShell columns={["Node", "IPv6", "Coordinates", "Last seen", "Risk", "Latest sensor values", "Connectivity"]}>
          {data.fleet.map((node) => (
            <tr key={node.id}>
              <td>
                <Link className="table-link" to={`/nodes/${node.nodeId}`}>
                  {node.nodeId}
                </Link>
              </td>
              <td>{node.ipv6Address ?? "N/A"}</td>
              <td>{`${node.location.lat.toFixed(4)}, ${node.location.lng.toFixed(4)}`}</td>
              <td>
                {formatTimestamp(node.lastSeenAt)}
                <div className="table-subtle">{formatRelativeMinutes(node.lastSeenAt)}</div>
              </td>
              <td>
                <span className={`badge risk-${node.currentRiskLevel ?? 0}`}>{riskLabel(node.currentRiskLevel)}</span>
              </td>
              <td>
                {latestSensorSummary(
                  node.latestTelemetry?.temperatureC,
                  node.latestTelemetry?.humidityPct,
                  node.latestTelemetry?.vocIaq,
                  node.latestTelemetry?.pm25UgM3,
                )}
              </td>
              <td>
                <span className={`badge status-${node.connectivity}`}>{node.connectivity}</span>
              </td>
            </tr>
          ))}
        </TableShell>
      </div>

      <div className="card">
        <div className="section-heading">
          <div>
            <h2>Recent downlinks</h2>
            <p>Latest 0x04 neighbor tables, 0x05 time syncs, and 0x06 threshold pushes originating from the CSP.</p>
          </div>
          <Link className="text-link" to="/configuration">
            Configuration controls
          </Link>
        </div>

        <TableShell columns={["Command", "Node", "Status", "Revision", "Sent"]}>
          {data.downlinks.slice(0, 6).map((downlink) => (
            <tr key={downlink.id}>
              <td>{`${downlink.commandCode} ${downlink.commandName}`}</td>
              <td>{downlink.nodeId}</td>
              <td>
                <span className={`badge downlink-${downlink.status}`}>{downlink.status}</span>
              </td>
              <td>{downlink.revisionNo ?? "N/A"}</td>
              <td>{formatTimestamp(downlink.sentAt)}</td>
            </tr>
          ))}
        </TableShell>
      </div>
    </PageContainer>
  );
}
