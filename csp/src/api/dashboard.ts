import type { DashboardResponse } from "./types";
import { apiGet } from "../lib/http";

export async function getDashboard() {
  return apiGet<DashboardResponse>("/dashboard");
}
