import type { KeyboardEvent, MouseEvent } from "react";
import { Link, useNavigate } from "react-router-dom";
import { listAlerts } from "../api/alerts";
import { EmptyState } from "../components/common/EmptyState";
import { LoadingState } from "../components/common/LoadingState";
import { PageContainer } from "../components/common/PageContainer";
import { TableShell } from "../components/common/TableShell";
import { formatInteger, formatTimestamp, riskLabel } from "../lib/format";
import { useAsyncData } from "../lib/useAsyncData";

export function AlertsPage() {
  const navigate = useNavigate();
  const { data, error, loading } = useAsyncData(listAlerts, []);

  function openNodeDetail(nodeId: number) {
    navigate(`/nodes/${nodeId}`);
  }

  function shouldIgnoreRowNavigation(target: EventTarget | null, currentTarget: HTMLElement) {
    if (!(target instanceof Element)) {
      return false;
    }

    const interactiveAncestor = target.closest("a, button, input, select, textarea, summary, [role='button'], [role='link']");
    return interactiveAncestor !== null && interactiveAncestor !== currentTarget;
  }

  function handleRowClick(event: MouseEvent<HTMLTableRowElement>, nodeId: number) {
    if (shouldIgnoreRowNavigation(event.target, event.currentTarget)) {
      return;
    }

    openNodeDetail(nodeId);
  }

  function handleRowKeyDown(event: KeyboardEvent<HTMLTableRowElement>, nodeId: number) {
    if (shouldIgnoreRowNavigation(event.target, event.currentTarget)) {
      return;
    }

    if (event.key !== "Enter" && event.key !== " ") {
      return;
    }

    event.preventDefault();
    openNodeDetail(nodeId);
  }

  if (loading) {
    return <LoadingState label="Loading alert queue..." />;
  }

  if (error || !data) {
    return <EmptyState title="Unable to load alerts" message={error ?? "Alert data is unavailable."} />;
  }

  const criticalAlerts = data.filter((alert) => alert.incidentType === "critical_alert");
  const batteryHealthAlerts = data.filter((alert) => alert.incidentType === "battery_health_low");
  const offlineAlerts = data.filter((alert) => alert.incidentType === "offline");
  const operationalFailureAlerts = data.filter(
    (alert) =>
      alert.incidentType === "time_sync_failure" ||
      alert.incidentType === "nn_update_failure" ||
      alert.incidentType === "config_update_failure",
  );

  return (
    <PageContainer>
      <div className="alerts-grid">
        <div className="card">
          <div className="section-heading">
            <div>
              <h2>Critical Alert</h2>
              <p>Direct node-reported alerts with snapshot telemetry and notification delivery state.</p>
            </div>
          </div>
          {criticalAlerts.length === 0 ? (
            <EmptyState title="No critical alerts" message="Critical node alerts will appear here." />
          ) : (
            <TableShell className="alerts-summary-table" columns={["Node", "Detected"]}>
              {criticalAlerts.map((alert) => (
                <tr
                  key={alert.id}
                  className="alerts-summary-row"
                  onClick={(event) => handleRowClick(event, alert.nodeId)}
                  onKeyDown={(event) => handleRowKeyDown(event, alert.nodeId)}
                  role="link"
                  tabIndex={0}
                  aria-label={`Open node ${alert.nodeId} detail`}
                >
                  <td>{alert.nodeId}</td>
                  <td>{formatTimestamp(alert.detectedAt)}</td>
                </tr>
              ))}
            </TableShell>
          )}
        </div>

        <div className="card">
          <div className="section-heading">
            <div>
              <h2>Battery Health Low</h2>
              <p>Battery degradation alerts sourced from the database alert stream.</p>
            </div>
          </div>
          {batteryHealthAlerts.length === 0 ? (
            <EmptyState title="No battery health alerts" message="Battery health incidents will appear here when available." />
          ) : (
            <TableShell columns={["Node", "Severity", "Status", "Battery snapshot", "Notifications", "Detected"]}>
              {batteryHealthAlerts.map((alert) => (
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
                    Battery {formatInteger(alert.latestSnapshot?.batteryPct, "%")} / {riskLabel(alert.latestSnapshot?.riskLevel)}
                  </td>
                  <td>{alert.notificationStatus}</td>
                  <td>{formatTimestamp(alert.detectedAt)}</td>
                </tr>
              ))}
            </TableShell>
          )}
        </div>

        <div className="card">
          <div className="section-heading">
            <div>
              <h2>Offline</h2>
              <p>Derived connectivity loss incidents based on the 5-minute silence rule.</p>
            </div>
          </div>
          {offlineAlerts.length === 0 ? (
            <EmptyState title="No offline incidents" message="All nodes are checking in within the required window." />
          ) : (
            <TableShell className="alerts-summary-table" columns={["Node", "Last Seen"]}>
              {offlineAlerts.map((alert) => (
                <tr
                  key={alert.id}
                  className="alerts-summary-row"
                  onClick={(event) => handleRowClick(event, alert.nodeId)}
                  onKeyDown={(event) => handleRowKeyDown(event, alert.nodeId)}
                  role="link"
                  tabIndex={0}
                  aria-label={`Open node ${alert.nodeId} detail`}
                >
                  <td>{alert.nodeId}</td>
                  <td>{formatTimestamp(alert.lastSeenAt ?? null)}</td>
                </tr>
              ))}
            </TableShell>
          )}
        </div>

        <div className="card">
          <div className="section-heading">
            <div>
              <h2>Operational Failures</h2>
              <p>Observed downlink failures and timeouts for neighbor updates, time sync, and config pushes.</p>
            </div>
          </div>
          {operationalFailureAlerts.length === 0 ? (
            <EmptyState title="No operational failures" message="Downlink-related failures will appear here when commands miss acknowledgements or fail." />
          ) : (
            <TableShell columns={["Node", "Type", "Status", "Notifications", "Detected"]}>
              {operationalFailureAlerts.map((alert) => (
                <tr key={alert.id}>
                  <td>
                    <Link className="table-link" to={`/nodes/${alert.nodeId}`}>
                      {alert.nodeId}
                    </Link>
                  </td>
                  <td>{alert.title}</td>
                  <td>{alert.status}</td>
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
