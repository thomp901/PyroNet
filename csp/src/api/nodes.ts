import type { BorderRouterDetail, GatewayMarker, NodeDetail, NodeSummary, NodeId } from "./types";
import { apiGet } from "../lib/http";
import { formatGatewayId } from "../lib/gatewayId";
import { formatNodeId } from "../lib/nodeId";

export async function listNodes() {
  return apiGet<NodeSummary[]>("/nodes");
}

export async function listBorderRouters() {
  return apiGet<GatewayMarker[]>("/gateways");
}

export async function getBorderRouterDetail(gatewayId: number) {
  return apiGet<BorderRouterDetail>(`/gateways/${encodeURIComponent(formatGatewayId(gatewayId))}`);
}

export async function getNodeDetail(nodeId: NodeId) {
  return apiGet<NodeDetail>(`/nodes/${encodeURIComponent(formatNodeId(nodeId))}`);
}
