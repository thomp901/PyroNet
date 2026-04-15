import type { KeyboardEvent, MouseEvent } from "react";
import { Link, useNavigate } from "react-router-dom";
import { getDashboard } from "../api/dashboard";
import { EmptyState } from "../components/common/EmptyState";
import { LoadingState } from "../components/common/LoadingState";
import { PageContainer } from "../components/common/PageContainer";
import { StatCard } from "../components/common/StatCard";
import { TableShell } from "../components/common/TableShell";
import { MapPreview } from "../features/map/MapPreview";
import {
  formatCoordinatePair,
  formatInteger,
  formatNullableNumber,
  formatRelativeMinutes,
  formatTimestamp,
  incidentTypeLabel,
  riskLabel,
} from "../lib/format";
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
  const navigate = useNavigate();
  const { data, error, loading } = useAsyncData(getDashboard, []);

  function stopCardNavigation(event: MouseEvent<HTMLDivElement> | KeyboardEvent<HTMLDivElement>) {
    event.stopPropagation();
  }

  function shouldIgnoreCardNavigation(target: EventTarget | null, currentTarget: HTMLElement) {
    if (!(target instanceof Element)) {
      return false;
    }

    const interactiveAncestor = target.closest("a, button, input, select, textarea, summary, [role='button'], [role='link']");
    return interactiveAncestor !== null && interactiveAncestor !== currentTarget;
  }

  function navigateFromCard(path: string, event: MouseEvent<HTMLElement> | KeyboardEvent<HTMLElement>) {
    if (shouldIgnoreCardNavigation(event.target, event.currentTarget)) {
      return;
    }

    if ("key" in event && event.key !== "Enter" && event.key !== " ") {
      return;
    }

    if ("preventDefault" in event && "key" in event) {
      event.preventDefault();
    }

    navigate(path);
  }

  if (loading) {
    return <LoadingState label="Loading mesh overview..." />;
  }

  if (error || !data) {
    return <EmptyState title="Dashboard unavailable" message={error ?? "Unable to load dashboard data."} />;
  }

  return (
    <PageContainer>
      <div className="stat-grid">
        <StatCard
          className="dashboard-link-card"
          label="Deployed nodes"
          value={String(data.summary.totalNodes)}
          onClick={(event) => navigateFromCard("/nodes", event)}
          onKeyDown={(event) => navigateFromCard("/nodes", event)}
          role="link"
          tabIndex={0}
          aria-label="Open node fleet"
        />
        <StatCard
          className="dashboard-link-card"
          label="Battery service needed"
          value={String(data.summary.degradedNodes)}
          onClick={(event) => navigateFromCard("/nodes", event)}
          onKeyDown={(event) => navigateFromCard("/nodes", event)}
          role="link"
          tabIndex={0}
          aria-label="Open node fleet"
        />
        <StatCard
          className="dashboard-link-card"
          label="Offline nodes"
          value={String(data.summary.offlineNodes)}
          onClick={(event) => navigateFromCard("/nodes", event)}
          onKeyDown={(event) => navigateFromCard("/nodes", event)}
          role="link"
          tabIndex={0}
          aria-label="Open node fleet"
        />
      </div>

      <div className="dashboard-grid dashboard-grid-stack">
        <MapPreview nodes={data.fleet} links={data.neighborLinks} />

        <div
          className="card dashboard-link-card"
          onClick={(event) => navigateFromCard("/alerts", event)}
          onKeyDown={(event) => navigateFromCard("/alerts", event)}
          role="link"
          tabIndex={0}
          aria-label="Open alerts"
        >
          <div className="section-heading">
            <div>
              <h2>Incident queue</h2>
              <p>Require operator attention.</p>
            </div>
          </div>

          <div className="dashboard-card-static-zone" onClick={stopCardNavigation} onKeyDownCapture={stopCardNavigation}>
            <TableShell className="table-shell-compact incident-queue-table" columns={["Node", "Type", "Severity", "Detected"]}>
              {data.alertQueue.slice(0, 6).map((alert) => (
                <tr key={alert.id}>
                  <td>
                    <Link className="table-link" to={`/nodes/${alert.nodeId}`}>
                      {alert.nodeId}
                    </Link>
                  </td>
                  <td>{incidentTypeLabel(alert.incidentType)}</td>
                  <td>
                    <span className={`badge severity-${alert.severity}`}>{alert.severity}</span>
                  </td>
                  <td>{formatTimestamp(alert.detectedAt)}</td>
                </tr>
              ))}
            </TableShell>
          </div>
        </div>
      </div>

      <div
        className="card dashboard-link-card"
        onClick={(event) => navigateFromCard("/nodes", event)}
        onKeyDown={(event) => navigateFromCard("/nodes", event)}
        role="link"
        tabIndex={0}
        aria-label="Open node fleet"
      >
        <div className="section-heading">
          <div>
            <h2>Fleet overview</h2>
          </div>
        </div>

        <div className="dashboard-card-static-zone" onClick={stopCardNavigation} onKeyDownCapture={stopCardNavigation}>
          <TableShell columns={["Node", "IPv6", "Coordinates", "Last seen", "Risk", "Latest sensor values", "Connectivity"]}>
            {data.fleet.map((node) => (
              <tr key={node.id}>
                <td>
                  <Link className="table-link" to={`/nodes/${node.nodeId}`}>
                    {node.nodeId}
                  </Link>
                </td>
                <td>{node.ipv6Address ?? "N/A"}</td>
                <td>{formatCoordinatePair(node.location.lat, node.location.lng)}</td>
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
      </div>

      <div
        className="card dashboard-link-card"
        onClick={(event) => navigateFromCard("/history", event)}
        onKeyDown={(event) => navigateFromCard("/history", event)}
        role="link"
        tabIndex={0}
        aria-label="Open history"
      >
        <div className="section-heading">
          <div>
            <h2>Recent downlinks</h2>
            <p>Latest 0x04 neighbor tables, 0x05 time syncs, and 0x06 threshold pushes originating from the CSP.</p>
          </div>
        </div>

        <div className="dashboard-card-static-zone" onClick={stopCardNavigation} onKeyDownCapture={stopCardNavigation}>
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
      </div>
    </PageContainer>
  );
}
