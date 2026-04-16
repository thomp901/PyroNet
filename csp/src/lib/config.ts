interface AppConfig {
  apiBaseUrl: string;
  useMockApi: boolean;
}

const apiBaseUrl = import.meta.env.VITE_API_BASE_URL?.trim() || "/api";
const useMockApi = import.meta.env.VITE_USE_MOCK_API === "true";

export const appConfig: AppConfig = {
  apiBaseUrl,
  useMockApi,
};
