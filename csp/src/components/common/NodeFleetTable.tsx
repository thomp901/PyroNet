import type { KeyboardEvent, MouseEvent } from "react";
import { useState } from "react";
import { Link, useNavigate } from "react-router-dom";
import type { NodeSummary } from "../../api/types";
import { formatInteger, formatLocation, formatNullableNumber, formatTimestamp, riskLabel } from "../../lib/format";
import { TableShell } from "./TableShell";

type SortColumn = "node" | "location" | "risk" | "temp" | "rh" | "voc" | "pm25" | "battery" | "lastSeen";
type SortDirection = "asc" | "desc";

interface NodeFleetTableProps {
  nodes: NodeSummary[];
  className?: string;
  sortable?: boolean;
  showNodeIdLink?: boolean;
}

const columns = ["Node", "Location", "Risk", "Temp", "RH", "VOC", "PM2.5", "Battery %", "Last Seen"];

function compareNullableNumbers(left: number | null | undefined, right: number | null | undefined, direction: SortDirection) {
  if (left == null && right == null) {
    return 0;
  }
  if (left == null) {
    return 1;
  }
  if (right == null) {
    return -1;
  }

  return direction === "asc" ? left - right : right - left;
}

function compareNullableTimestamps(left: string | null | undefined, right: string | null | undefined, direction: SortDirection) {
  if (!left && !right) {
    return 0;
  }
  if (!left) {
    return 1;
  }
  if (!right) {
    return -1;
  }

  const leftTime = new Date(left).getTime();
  const rightTime = new Date(right).getTime();
  return direction === "asc" ? leftTime - rightTime : rightTime - leftTime;
}

function compareText(left: string, right: string, direction: SortDirection) {
  const result = left.localeCompare(right);
  return direction === "asc" ? result : -result;
}

export function NodeFleetTable({
  nodes,
  className = "node-fleet-table",
  sortable = false,
  showNodeIdLink = true,
}: NodeFleetTableProps) {
  const navigate = useNavigate();
  const [sortColumn, setSortColumn] = useState<SortColumn>("node");
  const [sortDirection, setSortDirection] = useState<SortDirection>("asc");

  function toggleSort(column: SortColumn) {
    if (sortColumn === column) {
      setSortDirection((current) => (current === "asc" ? "desc" : "asc"));
      return;
    }

    setSortColumn(column);
    setSortDirection("asc");
  }

  function sortNodes(left: NodeSummary, right: NodeSummary) {
    switch (sortColumn) {
      case "node":
        return sortDirection === "asc" ? left.nodeId - right.nodeId : right.nodeId - left.nodeId;
      case "location":
        return compareText(
          formatLocation(left.location.label, left.location.lat, left.location.lng),
          formatLocation(right.location.label, right.location.lat, right.location.lng),
          sortDirection,
        );
      case "risk":
        return compareNullableNumbers(left.currentRiskLevel, right.currentRiskLevel, sortDirection);
      case "temp":
        return compareNullableNumbers(left.latestTelemetry?.temperatureC, right.latestTelemetry?.temperatureC, sortDirection);
      case "rh":
        return compareNullableNumbers(left.latestTelemetry?.humidityPct, right.latestTelemetry?.humidityPct, sortDirection);
      case "voc":
        return compareNullableNumbers(left.latestTelemetry?.vocIaq, right.latestTelemetry?.vocIaq, sortDirection);
      case "pm25":
        return compareNullableNumbers(left.latestTelemetry?.pm25UgM3, right.latestTelemetry?.pm25UgM3, sortDirection);
      case "battery":
        return compareNullableNumbers(left.latestTelemetry?.batteryPct, right.latestTelemetry?.batteryPct, sortDirection);
      case "lastSeen":
        return compareNullableTimestamps(left.lastSeenAt, right.lastSeenAt, sortDirection);
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

  const displayedNodes = sortable ? [...nodes].sort(sortNodes) : nodes;
  const headerCells = sortable
    ? [
        {
          key: "node",
          ariaSort: ariaSort("node"),
          content: (
            <button
              type="button"
              className="table-sort-button"
              onClick={() => toggleSort("node")}
              aria-label={sortButtonLabel("Node", "node")}
            >
              Node
              <span className="table-sort-indicator" aria-hidden="true">
                {sortIndicator("node")}
              </span>
            </button>
          ),
        },
        {
          key: "location",
          ariaSort: ariaSort("location"),
          content: (
            <button
              type="button"
              className="table-sort-button"
              onClick={() => toggleSort("location")}
              aria-label={sortButtonLabel("Location", "location")}
            >
              Location
              <span className="table-sort-indicator" aria-hidden="true">
                {sortIndicator("location")}
              </span>
            </button>
          ),
        },
        {
          key: "risk",
          ariaSort: ariaSort("risk"),
          content: (
            <button
              type="button"
              className="table-sort-button"
              onClick={() => toggleSort("risk")}
              aria-label={sortButtonLabel("Risk", "risk")}
            >
              Risk
              <span className="table-sort-indicator" aria-hidden="true">
                {sortIndicator("risk")}
              </span>
            </button>
          ),
        },
        {
          key: "temp",
          ariaSort: ariaSort("temp"),
          content: (
            <button
              type="button"
              className="table-sort-button"
              onClick={() => toggleSort("temp")}
              aria-label={sortButtonLabel("Temp", "temp")}
            >
              Temp
              <span className="table-sort-indicator" aria-hidden="true">
                {sortIndicator("temp")}
              </span>
            </button>
          ),
        },
        {
          key: "rh",
          ariaSort: ariaSort("rh"),
          content: (
            <button
              type="button"
              className="table-sort-button"
              onClick={() => toggleSort("rh")}
              aria-label={sortButtonLabel("RH", "rh")}
            >
              RH
              <span className="table-sort-indicator" aria-hidden="true">
                {sortIndicator("rh")}
              </span>
            </button>
          ),
        },
        {
          key: "voc",
          ariaSort: ariaSort("voc"),
          content: (
            <button
              type="button"
              className="table-sort-button"
              onClick={() => toggleSort("voc")}
              aria-label={sortButtonLabel("VOC", "voc")}
            >
              VOC
              <span className="table-sort-indicator" aria-hidden="true">
                {sortIndicator("voc")}
              </span>
            </button>
          ),
        },
        {
          key: "pm25",
          ariaSort: ariaSort("pm25"),
          content: (
            <button
              type="button"
              className="table-sort-button"
              onClick={() => toggleSort("pm25")}
              aria-label={sortButtonLabel("PM2.5", "pm25")}
            >
              PM2.5
              <span className="table-sort-indicator" aria-hidden="true">
                {sortIndicator("pm25")}
              </span>
            </button>
          ),
        },
        {
          key: "battery",
          ariaSort: ariaSort("battery"),
          content: (
            <button
              type="button"
              className="table-sort-button"
              onClick={() => toggleSort("battery")}
              aria-label={sortButtonLabel("Battery %", "battery")}
            >
              Battery %
              <span className="table-sort-indicator" aria-hidden="true">
                {sortIndicator("battery")}
              </span>
            </button>
          ),
        },
        {
          key: "lastSeen",
          ariaSort: ariaSort("lastSeen"),
          content: (
            <button
              type="button"
              className="table-sort-button"
              onClick={() => toggleSort("lastSeen")}
              aria-label={sortButtonLabel("Last Seen", "lastSeen")}
            >
              Last Seen
              <span className="table-sort-indicator" aria-hidden="true">
                {sortIndicator("lastSeen")}
              </span>
            </button>
          ),
        },
      ]
    : undefined;

  return (
    <TableShell className={className} columns={columns} headerCells={headerCells}>
      {displayedNodes.map((node) => (
        <tr
          key={node.id}
          className="node-fleet-row"
          onClick={(event) => handleRowClick(event, node.nodeId)}
          onKeyDown={(event) => handleRowKeyDown(event, node.nodeId)}
          role="link"
          tabIndex={0}
          aria-label={`Open node ${node.nodeId} detail`}
        >
          <td>
            {showNodeIdLink ? (
              <Link className="table-link" to={`/nodes/${node.nodeId}`}>
                {node.nodeId}
              </Link>
            ) : (
              node.nodeId
            )}
          </td>
          <td>{formatLocation(node.location.label, node.location.lat, node.location.lng)}</td>
          <td>
            <span className={`badge risk-${node.currentRiskLevel ?? 0}`}>{riskLabel(node.currentRiskLevel)}</span>
          </td>
          <td>{formatNullableNumber(node.latestTelemetry?.temperatureC ?? null, "°C")}</td>
          <td>{formatNullableNumber(node.latestTelemetry?.humidityPct ?? null, "%")}</td>
          <td>{formatInteger(node.latestTelemetry?.vocIaq ?? null)}</td>
          <td>{formatNullableNumber(node.latestTelemetry?.pm25UgM3 ?? null, " ug/m3")}</td>
          <td>{formatInteger(node.latestTelemetry?.batteryPct ?? null, "%")}</td>
          <td>
            <span className={node.connectivity === "offline" ? "node-last-seen-offline" : undefined}>
              {formatTimestamp(node.lastSeenAt)}
            </span>
          </td>
        </tr>
      ))}
    </TableShell>
  );
}
