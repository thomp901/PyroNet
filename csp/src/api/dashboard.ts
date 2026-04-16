import type { DashboardResponse } from "./types";
import { apiGet } from "../lib/http";
import { appConfig } from "../lib/config";
import { getMockDashboard } from "../mocks/mockBackend";

export async function getDashboard() {
  if (appConfig.useMockApi) {
    return getMockDashboard();
  }

  try {
    return await apiGet<DashboardResponse>("/dashboard");
  } catch {
    return getMockDashboard();
  }
}
