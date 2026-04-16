import { useState } from "react";
import { Link } from "react-router-dom";
import type { PacketDirection, PacketEventType, PacketLogEntry, PacketLogStatus } from "../../api/types";
import { formatTimestamp } from "../../lib/format";
import { TableShell } from "./TableShell";

type SortColumn = "time" | "node" | "direction" | "event" | "status" | "summary";
type SortDirection = "asc" | "desc";

interface PacketHistoryTableProps {
  entries: PacketLogEntry[];
}

const columns = ["Time", "Node", "Direction", "Event", "Status", "Summary"];

function compareNumbers(left: number, right: number, direction: SortDirection) {
  return direction === "asc" ? left - right : right - left;
}

function compareText(left: string, right: string, direction: SortDirection) {
  const result = left.localeCompare(right);
  return direction === "asc" ? result : -result;
}

function eventTypeLabel(eventType: PacketEventType) {
  switch (eventType) {
    case "registration":
      return "Registration";
    case "periodic_report":
      return "Periodic Report";
    case "critical_alert":
      return "Critical Alert";
    case "neighbor_distribution":
      return "Neighbor Distribution";
    case "time_sync":
      return "Time Sync";
    case "config_deployment":
      return "Config Deployment";
  }
}

function directionLabel(direction: PacketDirection) {
  return direction === "uplink" ? "Inbound" : "Outbound";
}

function statusLabel(status: PacketLogStatus) {
  if (status === "timed_out") {
    return "Timed Out";
  }
  if (status === "received") {
    return "Received";
  }
  return status.charAt(0).toUpperCase() + status.slice(1);
}

function statusClassName(status: PacketLogStatus) {
  return status === "received" ? "packet-status-received" : `downlink-${status}`;
}

export function PacketHistoryTable({ entries }: PacketHistoryTableProps) {
  const [sortColumn, setSortColumn] = useState<SortColumn>("time");
  const [sortDirection, setSortDirection] = useState<SortDirection>("desc");

  function toggleSort(column: SortColumn) {
    if (sortColumn === column) {
      setSortDirection((current) => (current === "asc" ? "desc" : "asc"));
      return;
    }

    setSortColumn(column);
    setSortDirection(column === "time" ? "desc" : "asc");
  }

  function sortEntries(left: PacketLogEntry, right: PacketLogEntry) {
    switch (sortColumn) {
      case "time":
        return compareNumbers(new Date(left.occurredAt).getTime(), new Date(right.occurredAt).getTime(), sortDirection);
      case "node":
        return compareNumbers(left.nodeId, right.nodeId, sortDirection);
      case "direction":
        return compareText(directionLabel(left.direction), directionLabel(right.direction), sortDirection);
      case "event":
        return compareText(eventTypeLabel(left.eventType), eventTypeLabel(right.eventType), sortDirection);
      case "status":
        return compareText(statusLabel(left.status), statusLabel(right.status), sortDirection);
      case "summary":
        return compareText(left.summary, right.summary, sortDirection);
    }
  }

  function sortIndicator(column: SortColumn) {
    if (sortColumn !== column) {
      return "";
    }

    return sortDirection === "asc" ? "↑" : "↓";
  }

  function ariaSort(column: SortColumn) {
    if (sortColumn !== column) {
      return "none" as const;
    }

    return sortDirection === "asc" ? ("ascending" as const) : ("descending" as const);
  }

  function sortButtonLabel(label: string, column: SortColumn) {
    if (sortColumn !== column) {
      return `Sort by ${label}`;
    }

    return sortDirection === "asc" ? `Sort by ${label}, ascending` : `Sort by ${label}, descending`;
  }

  const displayedEntries = [...entries].sort(sortEntries);
  const headerCells = [
    {
      key: "time",
      ariaSort: ariaSort("time"),
      content: (
        <button type="button" className="table-sort-button" onClick={() => toggleSort("time")} aria-label={sortButtonLabel("Time", "time")}>
          Time
          <span className="table-sort-indicator" aria-hidden="true">
            {sortIndicator("time")}
          </span>
        </button>
      ),
    },
    {
      key: "node",
      ariaSort: ariaSort("node"),
      content: (
        <button type="button" className="table-sort-button" onClick={() => toggleSort("node")} aria-label={sortButtonLabel("Node", "node")}>
          Node
          <span className="table-sort-indicator" aria-hidden="true">
            {sortIndicator("node")}
          </span>
        </button>
      ),
    },
    {
      key: "direction",
      ariaSort: ariaSort("direction"),
      content: (
        <button
          type="button"
          className="table-sort-button"
          onClick={() => toggleSort("direction")}
          aria-label={sortButtonLabel("Direction", "direction")}
        >
          Direction
          <span className="table-sort-indicator" aria-hidden="true">
            {sortIndicator("direction")}
          </span>
        </button>
      ),
    },
    {
      key: "event",
      ariaSort: ariaSort("event"),
      content: (
        <button type="button" className="table-sort-button" onClick={() => toggleSort("event")} aria-label={sortButtonLabel("Event", "event")}>
          Event
          <span className="table-sort-indicator" aria-hidden="true">
            {sortIndicator("event")}
          </span>
        </button>
      ),
    },
    {
      key: "status",
      ariaSort: ariaSort("status"),
      content: (
        <button type="button" className="table-sort-button" onClick={() => toggleSort("status")} aria-label={sortButtonLabel("Status", "status")}>
          Status
          <span className="table-sort-indicator" aria-hidden="true">
            {sortIndicator("status")}
          </span>
        </button>
      ),
    },
    {
      key: "summary",
      ariaSort: ariaSort("summary"),
      content: (
        <button type="button" className="table-sort-button" onClick={() => toggleSort("summary")} aria-label={sortButtonLabel("Summary", "summary")}>
          Summary
          <span className="table-sort-indicator" aria-hidden="true">
            {sortIndicator("summary")}
          </span>
        </button>
      ),
    },
  ];

  return (
    <TableShell className="packet-history-table" columns={columns} headerCells={headerCells}>
      {displayedEntries.map((entry) => (
        <tr key={entry.id}>
          <td>{formatTimestamp(entry.occurredAt)}</td>
          <td>
            <Link className="table-link" to={`/nodes/${entry.nodeId}`}>
              {entry.nodeId}
            </Link>
          </td>
          <td>
            <span className={`badge packet-direction-${entry.direction}`}>{directionLabel(entry.direction)}</span>
          </td>
          <td>{eventTypeLabel(entry.eventType)}</td>
          <td>
            <span className={`badge ${statusClassName(entry.status)}`}>{statusLabel(entry.status)}</span>
          </td>
          <td>
            <div className="packet-history-summary">
              <strong>{entry.summary}</strong>
              {entry.detail ? <span className="table-subtle">{entry.detail}</span> : null}
            </div>
          </td>
        </tr>
      ))}
    </TableShell>
  );
}
