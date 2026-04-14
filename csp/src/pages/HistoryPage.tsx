import { useEffect, useState } from "react";
import { getHistory } from "../api/history";
import type { HistoryWindow } from "../api/types";
import { EmptyState } from "../components/common/EmptyState";
import { LoadingState } from "../components/common/LoadingState";
import { PageContainer } from "../components/common/PageContainer";
import { StatCard } from "../components/common/StatCard";
import { TableShell } from "../components/common/TableShell";
import { formatInteger, formatNullableNumber, formatTimestamp } from "../lib/format";
import { useAsyncData } from "../lib/useAsyncData";

const windows: HistoryWindow[] = ["24h", "7d", "30d"];

export function HistoryPage() {
  const [selectedNodeId, setSelectedNodeId] = useState<string | undefined>(undefined);
  const [window, setWindow] = useState<HistoryWindow>("24h");
  const { data, error, loading } = useAsyncData(() => getHistory(selectedNodeId, window), [selectedNodeId, window]);

  useEffect(() => {
    if (!selectedNodeId && data?.selectedNodeId) {
      setSelectedNodeId(data.selectedNodeId);
    }
  }, [data, selectedNodeId]);

  if (loading) {
    return <LoadingState label="Loading telemetry history..." />;
  }

  if (error || !data) {
    return <EmptyState title="Unable to load history" message={error ?? "Telemetry history is unavailable."} />;
  }

  return (
    <PageContainer
      title="Telemetry History"
      description="At least 24 hours of raw telemetry, with longer windows stretched into aggregate views for trend analysis."
      actions={
        <div className="toolbar">
          <label className="field-inline">
            <span>Node</span>
            <select value={selectedNodeId ?? data.selectedNodeId} onChange={(event) => setSelectedNodeId(event.target.value)}>
              {data.availableNodes.map((node) => (
                <option key={node.nodeId} value={node.nodeId}>
                  {node.nodeId}
                </option>
              ))}
            </select>
          </label>
          <label className="field-inline">
            <span>Window</span>
            <select value={window} onChange={(event) => setWindow(event.target.value as HistoryWindow)}>
              {windows.map((entry) => (
                <option key={entry} value={entry}>
                  {entry}
                </option>
              ))}
            </select>
          </label>
        </div>
      }
    >
      <div className="stat-grid">
        <StatCard label="Temperature delta" value={formatNullableNumber(data.trendSummary.temperatureDeltaC, "°C")} helper="Newest sample against oldest in view" />
        <StatCard label="Humidity delta" value={formatNullableNumber(data.trendSummary.humidityDeltaPct, "%")} helper="Relative humidity drift" />
        <StatCard label="VOC peak" value={formatInteger(data.trendSummary.vocPeak)} helper="Highest VOC IAQ in window" />
        <StatCard label="PM2.5 peak" value={formatNullableNumber(data.trendSummary.pm25Peak, " ug/m3")} helper="Highest fine particulate value in window" />
      </div>

      <div className="card">
        {data.mode === "raw" ? (
          <TableShell columns={["Reported", "Source", "Risk", "Temp", "RH", "VOC", "PM2.5", "Battery"]}>
            {data.rawReadings.map((reading) => (
              <tr key={reading.id}>
                <td>{formatTimestamp(reading.reportedAt)}</td>
                <td>{reading.sourceType}</td>
                <td>{reading.riskLevel}</td>
                <td>{formatNullableNumber(reading.temperatureC, "°C")}</td>
                <td>{formatNullableNumber(reading.humidityPct, "%")}</td>
                <td>{formatInteger(reading.vocIaq)}</td>
                <td>{formatNullableNumber(reading.pm25UgM3, " ug/m3")}</td>
                <td>{formatInteger(reading.batteryPct, "%")}</td>
              </tr>
            ))}
          </TableShell>
        ) : (
          <TableShell columns={["Bucket start", "Samples", "Avg temp", "Avg RH", "Avg VOC", "Avg PM2.5", "Max risk"]}>
            {data.aggregateBuckets.map((bucket) => (
              <tr key={bucket.bucketStart}>
                <td>{formatTimestamp(bucket.bucketStart)}</td>
                <td>{bucket.sampleCount}</td>
                <td>{formatNullableNumber(bucket.avgTemperatureC, "°C")}</td>
                <td>{formatNullableNumber(bucket.avgHumidityPct, "%")}</td>
                <td>{formatNullableNumber(bucket.avgVocIaq, "", 0)}</td>
                <td>{formatNullableNumber(bucket.avgPm25UgM3, " ug/m3")}</td>
                <td>{bucket.maxRiskLevel ?? "N/A"}</td>
              </tr>
            ))}
          </TableShell>
        )}
      </div>
    </PageContainer>
  );
}
