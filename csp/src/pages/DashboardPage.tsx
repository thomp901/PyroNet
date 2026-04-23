import type { KeyboardEvent, MouseEvent } from "react";
import { Link, useNavigate } from "react-router-dom";
import { getDashboard } from "../api/dashboard";
import { BorderRouterTable } from "../components/common/BorderRouterTable";
import { EmptyState } from "../components/common/EmptyState";
import { LoadingState } from "../components/common/LoadingState";
import { NodeFleetTable } from "../components/common/NodeFleetTable";
import { PacketHistoryTable } from "../components/common/PacketHistoryTable";
import { PageContainer } from "../components/common/PageContainer";
import { StatCard } from "../components/common/StatCard";
import { TableShell } from "../components/common/TableShell";
import { MapPreview } from "../features/map/MapPreview";
import { formatTimestamp, incidentTypeLabel } from "../lib/format";
import { useAsyncData } from "../lib/useAsyncData";

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
        <MapPreview nodes={data.fleet} gateways={data.gateways} links={data.neighborLinks} />

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

          <div className="dashboard-card-static-zone" onClick={stopCardNavigation} onKeyDown={stopCardNavigation}>
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

      <div className="split-grid dashboard-overview-grid">
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
              <h2>Node Overview</h2>
            </div>
          </div>

          <div className="dashboard-card-static-zone" onClick={stopCardNavigation} onKeyDown={stopCardNavigation}>
            <NodeFleetTable nodes={data.fleet} className="table-shell-compact node-fleet-table" showNodeIdLink={false} />
          </div>
        </div>

        <div
          className="card dashboard-link-card"
          onClick={(event) => navigateFromCard("/border-routers", event)}
          onKeyDown={(event) => navigateFromCard("/border-routers", event)}
          role="link"
          tabIndex={0}
          aria-label="Open border routers"
        >
          <div className="section-heading">
            <div>
              <h2>Border Router Overview</h2>
            </div>
          </div>

          <div className="dashboard-card-static-zone" onClick={stopCardNavigation} onKeyDown={stopCardNavigation}>
            <BorderRouterTable borderRouters={data.gateways} />
          </div>
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
            <h2>Packet Traffic</h2>
          </div>
        </div>

        <div className="dashboard-card-static-zone" onClick={stopCardNavigation} onKeyDown={stopCardNavigation}>
          <PacketHistoryTable entries={data.recentPackets} />
        </div>
      </div>
    </PageContainer>
  );
}
