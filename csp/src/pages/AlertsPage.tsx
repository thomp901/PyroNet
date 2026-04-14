import { Link } from "react-router-dom";
import { listAlerts } from "../api/alerts";
import { EmptyState } from "../components/common/EmptyState";
import { LoadingState } from "../components/common/LoadingState";
import { PageContainer } from "../components/common/PageContainer";
import { TableShell } from "../components/common/TableShell";
import { formatTimestamp, riskLabel } from "../lib/format";
import { useAsyncData } from "../lib/useAsyncData";

export function AlertsPage() {
  const { data, error, loading } = useAsyncData(listAlerts, []);

  if (loading) {
    return <LoadingState label="Loading alert queue..." />;
  }

  if (error || !data) {
    return <EmptyState title="Unable to load alerts" message={error ?? "Alert data is unavailable."} />;
  }

  const criticalAlerts = data.filter((alert) => alert.incidentType === "critical_alert");
  const offlineAlerts = data.filter((alert) => alert.incidentType === "offline");

  return (
    <PageContainer
      title="Alerts And Incidents"
      description="Critical 0x03 events must surface within 10 minutes, and offline incidents are derived automatically after 24 hours of silence."
    >
      <div className="split-grid">
        <div className="card">
          <div className="section-heading">
            <div>
              <h2>Critical 0x03 events</h2>
              <p>Direct node-reported alerts with snapshot telemetry and notification delivery state.</p>
            </div>
          </div>
          {criticalAlerts.length === 0 ? (
            <EmptyState title="No critical alerts" message="Critical node alerts will appear here." />
          ) : (
            <TableShell columns={["Node", "Severity", "Status", "Snapshot", "Notifications", "Detected"]}>
              {criticalAlerts.map((alert) => (
                <tr key={alert.id}>
                  <td>
                    <Link className="table-link" to={`/nodes/${alert.nodeId}`}>
                      {alert.nodeId}
                    </Link>
                  </td>
                  <td>
                    <span className={`badge severity-${alert.severity}`}>{alert.severity}</span>
                  </td>
                  <td>{alert.status}</td>
                  <td>
                    {riskLabel(alert.latestSnapshot?.riskLevel)} / VOC {alert.latestSnapshot?.vocIaq ?? "N/A"} / PM2.5{" "}
                    {alert.latestSnapshot?.pm25UgM3 ?? "N/A"}
                  </td>
                  <td>{alert.notificationStatus}</td>
                  <td>
                    {formatTimestamp(alert.detectedAt)}
                    <div className="table-subtle">{alert.visibleWithinSla ? "within SLA" : "outside SLA"}</div>
                  </td>
                </tr>
              ))}
            </TableShell>
          )}
        </div>

        <div className="card">
          <div className="section-heading">
            <div>
              <h2>Offline incidents</h2>
              <p>Derived connectivity loss incidents based on the 24-hour silence rule.</p>
            </div>
          </div>
          {offlineAlerts.length === 0 ? (
            <EmptyState title="No offline incidents" message="All nodes are checking in within the required window." />
          ) : (
            <TableShell columns={["Node", "Location", "Last snapshot", "Notification state", "Derived at"]}>
              {offlineAlerts.map((alert) => (
                <tr key={alert.id}>
                  <td>
                    <Link className="table-link" to={`/nodes/${alert.nodeId}`}>
                      {alert.nodeId}
                    </Link>
                  </td>
                  <td>{alert.locationLabel}</td>
                  <td>
                    {riskLabel(alert.latestSnapshot?.riskLevel)} / Temp {alert.latestSnapshot?.temperatureC ?? "N/A"} / RH{" "}
                    {alert.latestSnapshot?.humidityPct ?? "N/A"}
                  </td>
                  <td>{alert.notificationStatus}</td>
                  <td>{formatTimestamp(alert.detectedAt)}</td>
                </tr>
              ))}
            </TableShell>
          )}
        </div>
      </div>
    </PageContainer>
  );
}
