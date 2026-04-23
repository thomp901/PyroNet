import { useState, type CSSProperties } from "react";
import { Area, AreaChart, CartesianGrid, ResponsiveContainer, Tooltip, XAxis, YAxis } from "recharts";
import type { HistoryAggregateBucket, HistoryResponse, ReadingHistoryPoint } from "../../api/types";
import { formatNullableNumber, formatVocPpm, riskLabel } from "../../lib/format";

export type TelemetryMeasurementId = "temperatureC" | "humidityPct" | "vocIaq" | "pm25UgM3";
type TelemetryRangeId = "1h" | "6h" | "1d" | "1w" | "1m";

interface TelemetryMeasurementOption {
  id: TelemetryMeasurementId;
  label: string;
  accent: string;
  formatter: (value: number | null | undefined) => string;
  getRawValue: (reading: ReadingHistoryPoint) => number | null | undefined;
  getAggregateValue: (bucket: HistoryAggregateBucket) => number | null | undefined;
}

interface ChartPoint {
  timestamp: number;
  value: number;
  kind: "raw" | "aggregate";
  reportedAt: string;
  bucketEnd?: string;
  sampleCount?: number;
  sourceType?: ReadingHistoryPoint["sourceType"];
  riskLevel?: number | null;
  maxRiskLevel?: number | null;
  temperatureC?: number | null;
  humidityPct?: number | null;
  vocIaq?: number | null;
  pm25UgM3?: number | null;
}

interface TelemetryTrendChartProps {
  historyByWindow: {
    "24h": HistoryResponse;
    "7d": HistoryResponse;
    "30d": HistoryResponse;
  };
  measurementId: TelemetryMeasurementId;
}

interface TooltipContentProps {
  active?: boolean;
  payload?: Array<{ payload: ChartPoint }>;
  measurement: TelemetryMeasurementOption;
  rangeId: TelemetryRangeId;
}

const telemetryRangeOptions: Array<{
  id: TelemetryRangeId;
  label: string;
  sourceWindow: "24h" | "7d" | "30d";
  mode: "raw" | "aggregate";
  durationMs?: number;
}> = [
  { id: "1h", label: "1H", sourceWindow: "24h", mode: "raw", durationMs: 60 * 60 * 1000 },
  { id: "6h", label: "6H", sourceWindow: "24h", mode: "raw", durationMs: 6 * 60 * 60 * 1000 },
  { id: "1d", label: "1D", sourceWindow: "24h", mode: "raw", durationMs: 24 * 60 * 60 * 1000 },
  { id: "1w", label: "1W", sourceWindow: "7d", mode: "aggregate" },
  { id: "1m", label: "1M", sourceWindow: "30d", mode: "aggregate" },
];

export const telemetryMeasurementOptions: TelemetryMeasurementOption[] = [
  {
    id: "temperatureC",
    label: "Temperature",
    accent: "#ef954f",
    formatter: (value) => formatNullableNumber(value, "°C"),
    getRawValue: (reading) => reading.temperatureC,
    getAggregateValue: (bucket) => bucket.avgTemperatureC,
  },
  {
    id: "humidityPct",
    label: "Humidity",
    accent: "#6bc48d",
    formatter: (value) => formatNullableNumber(value, "%"),
    getRawValue: (reading) => reading.humidityPct,
    getAggregateValue: (bucket) => bucket.avgHumidityPct,
  },
  {
    id: "vocIaq",
    label: "VOC (ppm)",
    accent: "#f0c55b",
    formatter: (value) => formatVocPpm(value),
    getRawValue: (reading) => reading.vocIaq,
    getAggregateValue: (bucket) => bucket.avgVocIaq,
  },
  {
    id: "pm25UgM3",
    label: "PM2.5",
    accent: "#f06b59",
    formatter: (value) => formatNullableNumber(value, " ug/m3"),
    getRawValue: (reading) => reading.pm25UgM3,
    getAggregateValue: (bucket) => bucket.avgPm25UgM3,
  },
];

function formatAxisTick(value: number, rangeId: TelemetryRangeId) {
  const date = new Date(value);

  if (rangeId === "1h" || rangeId === "6h") {
    return date.toLocaleTimeString([], { hour: "numeric", minute: "2-digit" });
  }

  if (rangeId === "1d") {
    return date.toLocaleTimeString([], { hour: "numeric" });
  }

  return date.toLocaleDateString([], { month: "short", day: "numeric" });
}

function formatTooltipTimestamp(point: ChartPoint, rangeId: TelemetryRangeId) {
  if (point.kind === "raw") {
    return new Date(point.reportedAt).toLocaleString([], {
      month: "short",
      day: "numeric",
      hour: "numeric",
      minute: "2-digit",
    });
  }

  const start = new Date(point.reportedAt).toLocaleString([], {
    month: "short",
    day: "numeric",
    hour: rangeId === "1m" ? undefined : "numeric",
    minute: rangeId === "1m" ? undefined : "2-digit",
  });
  const end = point.bucketEnd
    ? new Date(point.bucketEnd).toLocaleString([], {
        month: "short",
        day: "numeric",
        hour: rangeId === "1m" ? undefined : "numeric",
        minute: rangeId === "1m" ? undefined : "2-digit",
      })
    : null;

  return end ? `${start} to ${end}` : start;
}

function buildRawPoints(readings: ReadingHistoryPoint[], measurement: TelemetryMeasurementOption, durationMs: number) {
  const chronologicalReadings = [...readings].sort((left, right) => {
    return new Date(left.reportedAt).getTime() - new Date(right.reportedAt).getTime();
  });

  if (!chronologicalReadings.length) {
    return [];
  }

  const latestTimestamp = new Date(chronologicalReadings[chronologicalReadings.length - 1].reportedAt).getTime();
  const earliestTimestamp = latestTimestamp - durationMs;

  return chronologicalReadings
    .filter((reading) => new Date(reading.reportedAt).getTime() >= earliestTimestamp)
    .map((reading) => ({
      timestamp: new Date(reading.reportedAt).getTime(),
      value: measurement.getRawValue(reading),
      kind: "raw" as const,
      reportedAt: reading.reportedAt,
      sampleCount: 1,
      sourceType: reading.sourceType,
      riskLevel: reading.riskLevel,
      temperatureC: reading.temperatureC,
      humidityPct: reading.humidityPct,
      vocIaq: reading.vocIaq,
      pm25UgM3: reading.pm25UgM3,
    }))
    .filter((point) => typeof point.value === "number") as ChartPoint[];
}

function buildAggregatePoints(buckets: HistoryAggregateBucket[], measurement: TelemetryMeasurementOption) {
  return [...buckets]
    .sort((left, right) => new Date(left.bucketStart).getTime() - new Date(right.bucketStart).getTime())
    .map((bucket) => ({
      timestamp: new Date(bucket.bucketStart).getTime(),
      value: measurement.getAggregateValue(bucket),
      kind: "aggregate" as const,
      reportedAt: bucket.bucketStart,
      bucketEnd: bucket.bucketEnd,
      sampleCount: bucket.sampleCount,
      maxRiskLevel: bucket.maxRiskLevel,
      temperatureC: bucket.avgTemperatureC,
      humidityPct: bucket.avgHumidityPct,
      vocIaq: bucket.avgVocIaq,
      pm25UgM3: bucket.avgPm25UgM3,
    }))
    .filter((point) => typeof point.value === "number") as ChartPoint[];
}

function TelemetryTooltip({ active, payload, measurement, rangeId }: TooltipContentProps) {
  if (!active || !payload?.length) {
    return null;
  }

  const point = payload[0].payload;

  return (
    <div className="history-tooltip">
      <div className="history-tooltip-title">{formatTooltipTimestamp(point, rangeId)}</div>
      <div className="history-tooltip-value">
        {measurement.label}: <strong>{measurement.formatter(point.value)}</strong>
      </div>
    </div>
  );
}

export function TelemetryTrendChart({ historyByWindow, measurementId }: TelemetryTrendChartProps) {
  const [selectedRange, setSelectedRange] = useState<TelemetryRangeId>("1d");
  const measurement = telemetryMeasurementOptions.find((option) => option.id === measurementId) ?? telemetryMeasurementOptions[0];
  const range = telemetryRangeOptions.find((option) => option.id === selectedRange) ?? telemetryRangeOptions[2];
  const chartData: ChartPoint[] =
    range.mode === "raw"
      ? buildRawPoints(historyByWindow["24h"].rawReadings, measurement, range.durationMs ?? 24 * 60 * 60 * 1000)
      : buildAggregatePoints(historyByWindow[range.sourceWindow].aggregateBuckets, measurement);
  const latestPoint = chartData.length ? chartData[chartData.length - 1] : null;
  const earliestPoint = chartData.length ? chartData[0] : null;
  const latestValue = latestPoint?.value ?? null;
  const earliestValue = earliestPoint?.value ?? null;
  const deltaValue = latestPoint && earliestPoint ? Number((latestPoint.value - earliestPoint.value).toFixed(measurementId === "vocIaq" ? 0 : 1)) : null;
  const minValue = chartData.length ? Math.min(...chartData.map((point) => point.value)) : null;
  const maxValue = chartData.length ? Math.max(...chartData.map((point) => point.value)) : null;
  const averageValue = chartData.length ? chartData.reduce((sum, point) => sum + point.value, 0) / chartData.length : null;
  const chartStyle = {
    "--history-accent": measurement.accent,
  } as CSSProperties;

  return (
    <section className="history-chart-shell" style={chartStyle}>
      <div className="history-chart-meta">
        <div className="history-chart-meta-item">
          <span className="history-chart-meta-label">Latest</span>
          <strong>{measurement.formatter(latestValue)}</strong>
        </div>
        <div className="history-chart-meta-item">
          <span className="history-chart-meta-label">Change</span>
          <strong>{deltaValue === null ? "N/A" : measurement.formatter(deltaValue)}</strong>
        </div>
        <div className="history-chart-meta-item">
          <span className="history-chart-meta-label">Range</span>
          <strong>
            {measurement.formatter(minValue)} to {measurement.formatter(maxValue)}
          </strong>
        </div>
        <div className="history-chart-meta-item">
          <span className="history-chart-meta-label">Average</span>
          <strong>{measurement.formatter(averageValue)}</strong>
        </div>
      </div>

      {chartData.length ? (
        <div className="history-chart-frame">
          <ResponsiveContainer width="100%" height={360}>
            <AreaChart data={chartData} margin={{ top: 18, right: 12, bottom: 18, left: 0 }}>
              <defs>
                <linearGradient id="node-history-area-fill" x1="0" x2="0" y1="0" y2="1">
                  <stop offset="0%" stopColor="var(--history-accent)" stopOpacity="0.4" />
                  <stop offset="100%" stopColor="var(--history-accent)" stopOpacity="0.03" />
                </linearGradient>
              </defs>
              <CartesianGrid stroke="rgba(255, 243, 233, 0.08)" strokeDasharray="4 4" vertical={false} />
              <XAxis
                axisLine={false}
                dataKey="timestamp"
                minTickGap={24}
                tick={{ fill: "rgba(248, 243, 233, 0.68)", fontSize: 12 }}
                tickFormatter={(value) => formatAxisTick(value, selectedRange)}
                tickLine={false}
                type="number"
                domain={["dataMin", "dataMax"]}
              />
              <YAxis
                axisLine={false}
                tick={{ fill: "rgba(248, 243, 233, 0.68)", fontSize: 12 }}
                tickFormatter={(value: number) => measurement.formatter(value)}
                tickLine={false}
                width={84}
                domain={["auto", "auto"]}
              />
              <Tooltip content={<TelemetryTooltip measurement={measurement} rangeId={selectedRange} />} cursor={{ stroke: "rgba(255, 243, 233, 0.16)" }} />
              <Area
                type="monotone"
                dataKey="value"
                stroke="var(--history-accent)"
                fill="url(#node-history-area-fill)"
                fillOpacity={1}
                strokeWidth={3}
                dot={false}
                activeDot={{ r: 6, stroke: "rgba(15, 20, 24, 0.92)", strokeWidth: 2, fill: "var(--history-accent)" }}
                isAnimationActive={false}
              />
            </AreaChart>
          </ResponsiveContainer>
        </div>
      ) : (
        <div className="history-chart-empty">
          <h3>No telemetry available</h3>
          <p>{measurement.label} has no usable samples in the selected window.</p>
        </div>
      )}

      <div className="history-range-selector" aria-label="Historical range selector">
        {telemetryRangeOptions.map((option) => (
          <button
            key={option.id}
            className={`history-range-button${option.id === selectedRange ? " active" : ""}`}
            type="button"
            onClick={() => setSelectedRange(option.id)}
          >
            {option.label}
          </button>
        ))}
      </div>
    </section>
  );
}
