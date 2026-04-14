import type { HistoryResponse, HistoryWindow } from "./types";
import { apiGet } from "../lib/http";
import { appConfig } from "../lib/config";
import { getMockHistory } from "../mocks/mockBackend";

export async function getHistory(nodeId?: string, window: HistoryWindow = "24h") {
  if (appConfig.useMockApi) {
    return getMockHistory(nodeId, window);
  }

  const params = new URLSearchParams();
  if (nodeId) {
    params.set("nodeId", nodeId);
  }
  params.set("window", window);

  try {
    return await apiGet<HistoryResponse>(`/history?${params.toString()}`);
  } catch {
    return getMockHistory(nodeId, window);
  }
}
