import type {
  ConfigRevisionDraft,
  ConfigurationResponse,
  DownlinkRequest,
  NearestNeighborGenerationRequest,
  NeighborRevisionDraft,
  NodeId,
} from "./types";
import { apiGet, apiPost } from "../lib/http";
import { formatNodeId } from "../lib/nodeId";

export async function getConfiguration() {
  return apiGet<ConfigurationResponse>("/configuration");
}

export async function createConfigRevision(draft: ConfigRevisionDraft) {
  return apiPost<ConfigurationResponse, ConfigRevisionDraft>("/configuration/revisions", draft);
}

export async function updateNeighborRevision(nodeId: NodeId, draft: NeighborRevisionDraft) {
  return apiPost<ConfigurationResponse, NeighborRevisionDraft>(
    `/configuration/neighbors/${encodeURIComponent(formatNodeId(nodeId))}`,
    draft,
  );
}

export async function generateNearestNeighbors(request: NearestNeighborGenerationRequest) {
  return apiPost<ConfigurationResponse, NearestNeighborGenerationRequest>("/configuration/neighbors/generate", request);
}

export async function triggerNeighborDistribution(request: DownlinkRequest) {
  return apiPost<ConfigurationResponse, DownlinkRequest>("/configuration/downlinks/neighbor-distribution", request);
}

export async function triggerTimeSync(request: DownlinkRequest) {
  return apiPost<ConfigurationResponse, DownlinkRequest>("/configuration/downlinks/time-sync", request);
}

export async function triggerThresholdPush(request: DownlinkRequest) {
  return apiPost<ConfigurationResponse, DownlinkRequest>("/configuration/downlinks/threshold-push", request);
}
