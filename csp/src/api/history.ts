import type { HistoryResponse, HistoryWindow, NodeId, PacketHistoryQuery, PacketHistoryResponse } from "./types";
import { apiGet } from "../lib/http";
import { formatNodeId } from "../lib/nodeId";

export async function getHistory(nodeId?: NodeId, window: HistoryWindow = "24h") {
  const params = new URLSearchParams();
  if (nodeId !== undefined) {
    params.set("nodeId", formatNodeId(nodeId));
  }
  params.set("window", window);

  return apiGet<HistoryResponse>(`/history?${params.toString()}`);
}

export async function getPacketHistory(query: PacketHistoryQuery = {}) {
  const params = new URLSearchParams();
  if (query.limit !== undefined) {
    params.set("limit", String(query.limit));
  }
  if (query.offset !== undefined) {
    params.set("offset", String(query.offset));
  }
  if (query.nodeId !== undefined) {
    params.set("nodeId", formatNodeId(query.nodeId));
  }
  if (query.direction) {
    params.set("direction", query.direction);
  }
  if (query.packetCode) {
    params.set("packetCode", query.packetCode);
  }
  if (query.eventType) {
    params.set("eventType", query.eventType);
  }
  if (query.status) {
    params.set("status", query.status);
  }

  const path = params.size > 0 ? `/history/packets?${params.toString()}` : "/history/packets";

  return apiGet<PacketHistoryResponse>(path);
}
