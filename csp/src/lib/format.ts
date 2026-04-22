import type { AlertIncidentType } from "../api/types";

function formatCoordinateDms(value: number, positiveHemisphere: string, negativeHemisphere: string) {
  const hemisphere = value >= 0 ? positiveHemisphere : negativeHemisphere;
  const absoluteValue = Math.abs(value);
  let degrees = Math.floor(absoluteValue);
  const minutesFloat = (absoluteValue - degrees) * 60;
  let minutes = Math.floor(minutesFloat);
  let seconds = Number((((minutesFloat - minutes) * 60)).toFixed(1));

  if (seconds >= 60) {
    seconds = 0;
    minutes += 1;
  }

  if (minutes >= 60) {
    minutes = 0;
    degrees += 1;
  }

  const secondsText = seconds.toFixed(1).padStart(4, "0");
  return `${degrees}°${String(minutes).padStart(2, "0")}'${secondsText}"${hemisphere}`;
}

export function formatTimestamp(value: string | null | undefined) {
  if (!value) {
    return "N/A";
  }

  return new Date(value).toLocaleString();
}

export function formatRelativeMinutes(value: string | null | undefined) {
  if (!value) {
    return "No signal";
  }

  const diffMs = Date.now() - new Date(value).getTime();
  const diffMinutes = Math.max(0, Math.round(diffMs / 60000));

  if (diffMinutes < 60) {
    return `${diffMinutes} min ago`;
  }

  const hours = Math.floor(diffMinutes / 60);
  const minutes = diffMinutes % 60;
  return `${hours}h ${minutes}m ago`;
}

export function formatNullableNumber(value: number | null | undefined, unit = "", digits = 1) {
  if (typeof value !== "number") {
    return "N/A";
  }

  return `${value.toFixed(digits)}${unit}`;
}

export function formatInteger(value: number | null | undefined, unit = "") {
  if (typeof value !== "number") {
    return "N/A";
  }

  return `${Math.round(value)}${unit}`;
}

export function riskLabel(value: number | null | undefined) {
  if (typeof value !== "number") {
    return "Unknown";
  }

  return `Risk ${value}`;
}

export function formatCoordinatePair(latitude: number, longitude: number) {
  return `${formatCoordinateDms(latitude, "N", "S")}, ${formatCoordinateDms(longitude, "E", "W")}`;
}

export function formatLocation(label: string | null | undefined, latitude: number, longitude: number) {
  void label;
  return formatCoordinatePair(latitude, longitude);
}

export function incidentTypeLabel(value: AlertIncidentType) {
  switch (value) {
    case "critical_alert":
      return "Critical Alert";
    case "battery_health_low":
      return "Battery Health Low";
    case "offline":
      return "Offline";
    case "time_sync_failure":
      return "Time Sync Failure";
    case "nn_update_failure":
      return "Neighbor Update Failure";
    case "config_update_failure":
      return "Config Update Failure";
  }
}
