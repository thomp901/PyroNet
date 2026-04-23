import type { AlertIncident } from "./types";
import { apiGet } from "../lib/http";

export async function listAlerts() {
  return apiGet<AlertIncident[]>("/alerts");
}
