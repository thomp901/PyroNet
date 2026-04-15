import type {
  ConfigRevisionDraft,
  ConfigurationResponse,
  DownlinkRequest,
  NeighborRevisionDraft,
  NodeId,
} from "./types";
import { apiGet, apiPost } from "../lib/http";
import { appConfig } from "../lib/config";
import { formatNodeId } from "../lib/nodeId";
import {
  createMockConfigRevision,
  createMockNeighborDistribution,
  createMockThresholdPush,
  createMockTimeSync,
  getMockConfiguration,
  updateMockNeighborRevision,
} from "../mocks/mockBackend";

export async function getConfiguration() {
  if (appConfig.useMockApi) {
    return getMockConfiguration();
  }

  try {
    return await apiGet<ConfigurationResponse>("/configuration");
  } catch {
    return getMockConfiguration();
  }
}

export async function createConfigRevision(draft: ConfigRevisionDraft) {
  if (appConfig.useMockApi) {
    return createMockConfigRevision(draft);
  }

  try {
    return await apiPost<ConfigurationResponse, ConfigRevisionDraft>("/configuration/revisions", draft);
  } catch {
    return createMockConfigRevision(draft);
  }
}

export async function updateNeighborRevision(nodeId: NodeId, draft: NeighborRevisionDraft) {
  if (appConfig.useMockApi) {
    return updateMockNeighborRevision(nodeId, draft);
  }

  try {
    return await apiPost<ConfigurationResponse, NeighborRevisionDraft>(
      `/configuration/neighbors/${encodeURIComponent(formatNodeId(nodeId))}`,
      draft,
    );
  } catch {
    return updateMockNeighborRevision(nodeId, draft);
  }
}

export async function triggerNeighborDistribution(request: DownlinkRequest) {
  if (appConfig.useMockApi) {
    return createMockNeighborDistribution(request);
  }

  try {
    return await apiPost<ConfigurationResponse, DownlinkRequest>(
      "/configuration/downlinks/neighbor-distribution",
      request,
    );
  } catch {
    return createMockNeighborDistribution(request);
  }
}

export async function triggerTimeSync(request: DownlinkRequest) {
  if (appConfig.useMockApi) {
    return createMockTimeSync(request);
  }

  try {
    return await apiPost<ConfigurationResponse, DownlinkRequest>("/configuration/downlinks/time-sync", request);
  } catch {
    return createMockTimeSync(request);
  }
}

export async function triggerThresholdPush(request: DownlinkRequest) {
  if (appConfig.useMockApi) {
    return createMockThresholdPush(request);
  }

  try {
    return await apiPost<ConfigurationResponse, DownlinkRequest>(
      "/configuration/downlinks/threshold-push",
      request,
    );
  } catch {
    return createMockThresholdPush(request);
  }
}
