import type { AlertIncident } from "./types";
import { apiGet } from "../lib/http";
import { appConfig } from "../lib/config";
import { listMockAlerts } from "../mocks/mockBackend";

export async function listAlerts() {
  if (appConfig.useMockApi) {
    return listMockAlerts();
  }

  try {
    return await apiGet<AlertIncident[]>("/alerts");
  } catch {
    return listMockAlerts();
  }
}
