import type { HistoryResponse, HistoryWindow, NodeId } from "./types";
import { apiGet } from "../lib/http";
import { appConfig } from "../lib/config";
import { getMockHistory } from "../mocks/mockBackend";
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
