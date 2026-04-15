import type { CSSProperties } from "react";
import type { ReadingHistoryPoint } from "../../api/types";
import { formatInteger, formatNullableNumber } from "../../lib/format";

export type TelemetryMeasurementId = "temperatureC" | "humidityPct" | "vocIaq" | "pm25UgM3" | "batteryPct";

interface TelemetryMeasurementOption {
  id: TelemetryMeasurementId;
  label: string;
  description: string;
  accent: string;
  unit?: string;
  digits?: number;
  formatter: (value: number | null | undefined) => string;
  getValue: (reading: ReadingHistoryPoint) => number | null | undefined;
}

const chartWidth = 1000;
const chartHeight = 320;
const chartPadding = {
  top: 18,
  right: 10,
  bottom: 18,
  left: 10,
};

export const telemetryMeasurementOptions: TelemetryMeasurementOption[] = [
  {
    id: "temperatureC",
    label: "Temperature",
    description: "Ambient temperature over the last 24 hours.",
    accent: "#ef954f",
    unit: "°C",
    digits: 1,
    formatter: (value) => formatNullableNumber(value, "°C"),
    getValue: (reading) => reading.temperatureC,
  },
  {
    id: "humidityPct",
    label: "Humidity",
    description: "Relative humidity trend across recent samples.",
    accent: "#6bc48d",
    unit: "%",
    digits: 1,
    formatter: (value) => formatNullableNumber(value, "%"),
    getValue: (reading) => reading.humidityPct,
  },
  {
    id: "vocIaq",
    label: "VOC IAQ",
    description: "Volatile organic compound air-quality readings.",
    accent: "#f0c55b",
    formatter: (value) => formatInteger(value),
    getValue: (reading) => reading.vocIaq,
  },
  {
    id: "pm25UgM3",
    label: "PM2.5",
    description: "Fine particulate concentration over time.",
    accent: "#f06b59",
    unit: " ug/m3",
    digits: 1,
    formatter: (value) => formatNullableNumber(value, " ug/m3"),
    getValue: (reading) => reading.pm25UgM3,
  },
  {
    id: "batteryPct",
    label: "Battery",
    description: "Battery level reported with each telemetry sample.",
    accent: "#77b4ff",
    unit: "%",
    digits: 0,
    formatter: (value) => formatInteger(value, "%"),
    getValue: (reading) => reading.batteryPct,
  },
];

interface TelemetryTrendChartProps {
  readings: ReadingHistoryPoint[];
  measurementId: TelemetryMeasurementId;
}

function formatChartTimestamp(value: string) {
  return new Date(value).toLocaleString([], {
    month: "short",
    day: "numeric",
    hour: "numeric",
    minute: "2-digit",
  });
}

function buildAreaPath(points: Array<{ x: number; y: number }>) {
  if (!points.length) {
    return "";
  }

  const line = points.map((point, index) => `${index === 0 ? "M" : "L"} ${point.x} ${point.y}`).join(" ");
  const firstPoint = points[0];
  const lastPoint = points[points.length - 1];
  const baseline = chartHeight - chartPadding.bottom;
  return `${line} L ${lastPoint.x} ${baseline} L ${firstPoint.x} ${baseline} Z`;
}

function buildLinePath(points: Array<{ x: number; y: number }>) {
  if (!points.length) {
    return "";
  }

  return points.map((point, index) => `${index === 0 ? "M" : "L"} ${point.x} ${point.y}`).join(" ");
}

export function TelemetryTrendChart({ readings, measurementId }: TelemetryTrendChartProps) {
  const measurement = telemetryMeasurementOptions.find((option) => option.id === measurementId) ?? telemetryMeasurementOptions[0];
  const chronologicalReadings = [...readings].sort((left, right) => {
    return new Date(left.reportedAt).getTime() - new Date(right.reportedAt).getTime();
  });
  const values = chronologicalReadings
    .map((reading) => ({
      reading,
      value: measurement.getValue(reading),
    }))
    .filter((entry): entry is { reading: ReadingHistoryPoint; value: number } => typeof entry.value === "number");

  if (!values.length) {
    return (
      <div className="history-chart-empty">
        <h3>No telemetry available</h3>
        <p>{measurement.label} has no usable samples in the last 24 hours.</p>
      </div>
    );
  }

  const minValue = Math.min(...values.map((entry) => entry.value));
  const maxValue = Math.max(...values.map((entry) => entry.value));
  const range = maxValue - minValue || 1;
  const latestValue = values[values.length - 1]?.value ?? null;
  const linePoints = values.map((entry, index) => {
    const x =
      values.length === 1
        ? chartWidth / 2
        : chartPadding.left + (index / (values.length - 1)) * (chartWidth - chartPadding.left - chartPadding.right);
    const y =
      chartPadding.top +
      ((maxValue - entry.value) / range) * (chartHeight - chartPadding.top - chartPadding.bottom);

    return {
      x,
      y,
      reading: entry.reading,
      value: entry.value,
    };
  });
  const chartStyle = {
    "--history-accent": measurement.accent,
  } as CSSProperties;

  return (
    <section className="history-chart-shell" style={chartStyle}>
      <div className="history-chart-summary">
        <div className="history-chart-stat">
          <span className="history-chart-stat-label">Latest</span>
          <strong>{measurement.formatter(latestValue)}</strong>
        </div>
        <div className="history-chart-stat">
          <span className="history-chart-stat-label">Peak</span>
          <strong>{measurement.formatter(maxValue)}</strong>
        </div>
        <div className="history-chart-stat">
          <span className="history-chart-stat-label">Low</span>
          <strong>{measurement.formatter(minValue)}</strong>
        </div>
        <div className="history-chart-stat">
          <span className="history-chart-stat-label">Samples</span>
          <strong>{values.length}</strong>
        </div>
      </div>

      <div className="history-chart-stage">
        <div className="history-chart-scale">
          <span>{measurement.formatter(maxValue)}</span>
          <span>{measurement.formatter(minValue)}</span>
        </div>
        <div className="history-chart-canvas">
          <svg viewBox={`0 0 ${chartWidth} ${chartHeight}`} aria-label={`${measurement.label} trend over the last 24 hours`} role="img">
            <defs>
              <linearGradient id="history-area-fill" x1="0" x2="0" y1="0" y2="1">
                <stop offset="0%" stopColor="var(--history-accent)" stopOpacity="0.35" />
                <stop offset="100%" stopColor="var(--history-accent)" stopOpacity="0.02" />
              </linearGradient>
            </defs>
            <line
              className="history-chart-baseline"
              x1={chartPadding.left}
              x2={chartWidth - chartPadding.right}
              y1={chartHeight - chartPadding.bottom}
              y2={chartHeight - chartPadding.bottom}
            />
            <path className="history-chart-area" d={buildAreaPath(linePoints)} />
            <path className="history-chart-line" d={buildLinePath(linePoints)} />
            {linePoints.map((point) => (
              <circle key={point.reading.id} className="history-chart-point" cx={point.x} cy={point.y} r="4">
                <title>
                  {`${formatChartTimestamp(point.reading.reportedAt)}: ${measurement.formatter(point.value)}`}
                </title>
              </circle>
            ))}
          </svg>
        </div>
      </div>

      <div className="history-chart-footer">
        <div>
          <span className="history-chart-footer-label">Start</span>
          <strong>{formatChartTimestamp(values[0].reading.reportedAt)}</strong>
        </div>
        <div className="history-chart-caption">
          <span className="history-chart-footer-label">Signal</span>
          <strong>{measurement.description}</strong>
        </div>
        <div className="history-chart-footer-end">
          <span className="history-chart-footer-label">Latest sample</span>
          <strong>{formatChartTimestamp(values[values.length - 1].reading.reportedAt)}</strong>
        </div>
      </div>
    </section>
  );
}
