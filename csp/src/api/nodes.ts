import type { NodeDetail, NodeSummary, NodeId } from "./types";
import { apiGet } from "../lib/http";
import { formatNodeId } from "../lib/nodeId";

export async function listNodes() {
  return apiGet<NodeSummary[]>("/nodes");
}

export async function getNodeDetail(nodeId: NodeId) {
  return apiGet<NodeDetail>(`/nodes/${encodeURIComponent(formatNodeId(nodeId))}`);
}
