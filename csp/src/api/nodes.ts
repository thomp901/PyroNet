import type { NodeDetail, NodeSummary } from "./types";
import { apiGet } from "../lib/http";
import { appConfig } from "../lib/config";
import { getMockNodeDetail, listMockNodes } from "../mocks/mockBackend";

export async function listNodes() {
  if (appConfig.useMockApi) {
    return listMockNodes();
  }

  try {
    return await apiGet<NodeSummary[]>("/nodes");
  } catch {
    return listMockNodes();
  }
}

export async function getNodeDetail(nodeId: string) {
  if (appConfig.useMockApi) {
    return getMockNodeDetail(nodeId);
  }

  try {
    return await apiGet<NodeDetail>(`/nodes/${encodeURIComponent(nodeId)}`);
  } catch {
    return getMockNodeDetail(nodeId);
  }
}
