import type { HistoryResponse, HistoryWindow, NodeId, PacketHistoryQuery, PacketHistoryResponse } from "./types";
import { apiGet } from "../lib/http";
import { appConfig } from "../lib/config";
import { getMockHistory, getMockPacketHistory } from "../mocks/mockBackend";
import { formatNodeId } from "../lib/nodeId";

export async function getHistory(nodeId?: NodeId, window: HistoryWindow = "24h") {
  if (appConfig.useMockApi) {
    return getMockHistory(nodeId, window);
  }

  const params = new URLSearchParams();
  if (nodeId !== undefined) {
    params.set("nodeId", formatNodeId(nodeId));
  }
  params.set("window", window);

  try {
    return await apiGet<HistoryResponse>(`/history?${params.toString()}`);
  } catch {
    return getMockHistory(nodeId, window);
  }
}

export async function getPacketHistory(query: PacketHistoryQuery = {}) {
  if (appConfig.useMockApi) {
    return getMockPacketHistory(query);
  }

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

  try {
    return await apiGet<PacketHistoryResponse>(path);
  } catch {
    return getMockPacketHistory(query);
  }
}
