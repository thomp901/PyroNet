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
