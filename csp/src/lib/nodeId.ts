import type { NodeId } from "../api/types";

export const MAX_NODE_ID = 32_767;
export const MIN_NODE_ID = 0;

export function parseNodeId(value: number | string | null | undefined): NodeId | null {
  const numericValue =
    typeof value === "number"
      ? value
      : typeof value === "string" && value.trim() !== ""
        ? Number(value)
        : Number.NaN;

  if (!Number.isInteger(numericValue) || numericValue < MIN_NODE_ID || numericValue > MAX_NODE_ID) {
    return null;
  }

  return numericValue;
}

export function formatNodeId(nodeId: NodeId) {
  return String(nodeId);
}
