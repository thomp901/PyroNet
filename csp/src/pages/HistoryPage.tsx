import { useEffect, useState } from "react";
import { clearPacketHistory, getPacketHistory } from "../api/history";
import type { PacketDirection, PacketEventType, PacketHistoryQuery, PacketHistoryResponse, PacketLogCode, PacketLogStatus } from "../api/types";
import { EmptyState } from "../components/common/EmptyState";
import { LoadingState } from "../components/common/LoadingState";
import { PacketHistoryTable } from "../components/common/PacketHistoryTable";
import { PageContainer } from "../components/common/PageContainer";
import { parseNodeId } from "../lib/nodeId";

const PAGE_SIZE = 20;

function getErrorMessage(error: unknown) {
  return error instanceof Error ? error.message : "Packet history is unavailable.";
}

function humanizeLabel(value: string) {
  return value
    .split("_")
    .map((part) => part.charAt(0).toUpperCase() + part.slice(1))
    .join(" ");
}

function packetCodeLabel(code: PacketLogCode) {
  switch (code) {
    case "0x81":
      return "0x81 BR Registration";
    case "0x01":
      return "0x01 Registration";
    case "0x02":
      return "0x02 Sensor Report";
    case "0x03":
      return "0x03 Sensor Alert";
    case "0x04":
      return "0x04 NN Table Update";
    case "0x05":
      return "0x05 Time Sync";
    case "0x06":
      return "0x06 Config Update";
    case "0x08":
      return "0x08 Parent Update";
  }
}

export function HistoryPage() {
  const [filtersOpen, setFiltersOpen] = useState(false);
  const [filters, setFilters] = useState<PacketHistoryQuery>({});
  const [data, setData] = useState<PacketHistoryResponse | null>(null);
  const [entries, setEntries] = useState<PacketHistoryResponse["entries"]>([]);
  const [loading, setLoading] = useState(true);
  const [loadingMore, setLoadingMore] = useState(false);
  const [clearing, setClearing] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [successMessage, setSuccessMessage] = useState<string | null>(null);
  const [reloadToken, setReloadToken] = useState(0);

  useEffect(() => {
    let cancelled = false;

    async function loadPacketHistory() {
      setLoading(true);
      setError(null);

      try {
        const result = await getPacketHistory({
          ...filters,
          limit: PAGE_SIZE,
          offset: 0,
        });

        if (!cancelled) {
          setData(result);
          setEntries(result.entries);
        }
      } catch (loadError) {
        if (!cancelled) {
          setData(null);
          setEntries([]);
          setError(getErrorMessage(loadError));
        }
      } finally {
        if (!cancelled) {
          setLoading(false);
        }
      }
    }

    void loadPacketHistory();

    return () => {
      cancelled = true;
    };
  }, [filters.direction, filters.eventType, filters.nodeId, filters.packetCode, filters.status, reloadToken]);

  async function handleLoadMore() {
    if (!data?.hasMore) {
      return;
    }

    setLoadingMore(true);
    setError(null);

    try {
      const result = await getPacketHistory({
        ...filters,
        limit: PAGE_SIZE,
        offset: entries.length,
      });

      setData(result);
      setEntries((current) => [...current, ...result.entries]);
    } catch (loadError) {
      setError(getErrorMessage(loadError));
    } finally {
      setLoadingMore(false);
    }
  }

  function updateFilter<Key extends keyof PacketHistoryQuery>(key: Key, value: PacketHistoryQuery[Key]) {
    setFilters((current) => ({
      ...current,
      [key]: value,
    }));
  }

  function clearFilters() {
    setFilters({});
  }

  async function handleClearSqlEntries() {
    const confirmed = window.confirm(
      "Are you sure you want to clear all SQL-backed traffic log entries? This permanently deletes packet history, registrations, telemetry, and related downlink records.",
    );

    if (!confirmed) {
      return;
    }

    setClearing(true);
    setError(null);
    setSuccessMessage(null);

    try {
      await clearPacketHistory();
      setEntries([]);
      setData((current) =>
        current
          ? {
              ...current,
              entries: [],
              totalCount: 0,
              offset: 0,
              hasMore: false,
            }
          : current,
      );
      setSuccessMessage("SQL-backed traffic log entries cleared.");
      setReloadToken((current) => current + 1);
    } catch (clearError) {
      setError(getErrorMessage(clearError));
    } finally {
      setClearing(false);
    }
  }

  const activeFilterCount = [filters.nodeId, filters.direction, filters.packetCode, filters.eventType, filters.status].filter(Boolean).length;

  if (loading && !data) {
    return <LoadingState label="Loading packet history..." />;
  }

  if (error && !data) {
    return <EmptyState title="Unable to load history" message={error} />;
  }

  if (!data) {
    return <EmptyState title="Unable to load history" message="Packet history is unavailable." />;
  }

  return (
    <PageContainer
      title="Traffic Log"
      description="Network traffic log for packets moving between field nodes and the CSP across the full retained history."
      actions={
        <button type="button" className="secondary-button" onClick={() => setFiltersOpen((current) => !current)}>
          {activeFilterCount > 0 ? `Filters (${activeFilterCount})` : "Filters"}
        </button>
      }
    >
      {filtersOpen ? (
        <article className="card history-filter-card">
          <div className="section-heading history-filter-heading">
            <div>
              <h2>Filter traffic</h2>
              <p>Narrow the packet log by node, direction, packet code, event type, or delivery status.</p>
            </div>
            <button type="button" className="secondary-button" onClick={clearFilters}>
              Clear filters
            </button>
          </div>

          <div className="form-grid history-filter-grid">
            <label className="field">
              <span>Node</span>
              <select
                value={filters.nodeId ?? ""}
                onChange={(event) => {
                  const nextNodeId = parseNodeId(event.target.value);
                  updateFilter("nodeId", nextNodeId ?? undefined);
                }}
              >
                <option value="">All nodes</option>
                {data.availableNodes.map((node) => (
                  <option key={node.id} value={node.nodeId}>
                    {node.displayName}
                  </option>
                ))}
              </select>
            </label>

            <label className="field">
              <span>Direction</span>
              <select
                value={filters.direction ?? ""}
                onChange={(event) => updateFilter("direction", (event.target.value || undefined) as PacketDirection | undefined)}
              >
                <option value="">All directions</option>
                {data.availableDirections.map((direction) => (
                  <option key={direction} value={direction}>
                    {direction === "uplink" ? "Inbound" : "Outbound"}
                  </option>
                ))}
              </select>
            </label>

            <label className="field">
              <span>Event Type</span>
              <select
                value={filters.eventType ?? ""}
                onChange={(event) => updateFilter("eventType", (event.target.value || undefined) as PacketEventType | undefined)}
              >
                <option value="">All event types</option>
                {data.availableEventTypes.map((eventType) => (
                  <option key={eventType} value={eventType}>
                    {humanizeLabel(eventType)}
                  </option>
                ))}
              </select>
            </label>

            <label className="field">
              <span>Packet Code</span>
              <select
                value={filters.packetCode ?? ""}
                onChange={(event) => updateFilter("packetCode", (event.target.value || undefined) as PacketLogCode | undefined)}
              >
                <option value="">All packet codes</option>
                {data.availablePacketCodes.map((packetCode) => (
                  <option key={packetCode} value={packetCode}>
                    {packetCodeLabel(packetCode)}
                  </option>
                ))}
              </select>
            </label>

            <label className="field">
              <span>Status</span>
              <select
                value={filters.status ?? ""}
                onChange={(event) => updateFilter("status", (event.target.value || undefined) as PacketLogStatus | undefined)}
              >
                <option value="">All statuses</option>
                {data.availableStatuses.map((status) => (
                  <option key={status} value={status}>
                    {humanizeLabel(status)}
                  </option>
                ))}
              </select>
            </label>
          </div>
        </article>
      ) : null}

      <div className="card history-log-card">
        <div className="section-heading history-log-heading">
          <div>
            <h2>Packet traffic</h2>
            <p>
              Showing {entries.length} of {data.totalCount} packets matching the current filter set.
            </p>
          </div>
          <button type="button" className="danger-button" onClick={() => void handleClearSqlEntries()} disabled={clearing}>
            {clearing ? "Clearing SQL entries..." : "Clear SQL Entries"}
          </button>
        </div>

        {successMessage ? <div className="history-log-feedback history-log-feedback-success">{successMessage}</div> : null}
        {error ? <div className="history-log-feedback history-log-feedback-error">{error}</div> : null}

        {entries.length > 0 ? (
          <>
            <PacketHistoryTable entries={entries} />

            {data.hasMore ? (
              <div className="history-log-actions">
                <button type="button" className="secondary-button" onClick={() => void handleLoadMore()} disabled={loadingMore}>
                  {loadingMore ? "Loading more..." : `Load ${PAGE_SIZE} more`}
                </button>
              </div>
            ) : null}
          </>
        ) : (
          <EmptyState title="No packets found" message="Try widening the filters to bring more CSP traffic into view." />
        )}
      </div>
    </PageContainer>
  );
}
