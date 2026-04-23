export type GatewayId = number;

export const MAX_GATEWAY_ID = 65_535;
export const MIN_GATEWAY_ID = 0;

export function parseGatewayId(value: number | string | null | undefined): GatewayId | null {
  const numericValue =
    typeof value === "number"
      ? value
      : typeof value === "string" && value.trim() !== ""
        ? Number(value)
        : Number.NaN;

  if (!Number.isInteger(numericValue) || numericValue < MIN_GATEWAY_ID || numericValue > MAX_GATEWAY_ID) {
    return null;
  }

  return numericValue;
}

export function formatGatewayId(gatewayId: GatewayId) {
  return String(gatewayId);
}
