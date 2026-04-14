import type { NotificationRecipientUpdate, NotificationSettingsResponse } from "./types";
import { apiGet, apiPut } from "../lib/http";
import { appConfig } from "../lib/config";
import { getMockNotificationSettings, updateMockNotificationRecipient } from "../mocks/mockBackend";

export async function getNotificationSettings() {
  if (appConfig.useMockApi) {
    return getMockNotificationSettings();
  }

  try {
    return await apiGet<NotificationSettingsResponse>("/notifications");
  } catch {
    return getMockNotificationSettings();
  }
}

export async function updateNotificationRecipient(recipientId: string, update: NotificationRecipientUpdate) {
  if (appConfig.useMockApi) {
    return updateMockNotificationRecipient(recipientId, update);
  }

  try {
    return await apiPut<NotificationSettingsResponse, NotificationRecipientUpdate>(
      `/notifications/recipients/${encodeURIComponent(recipientId)}`,
      update,
    );
  } catch {
    return updateMockNotificationRecipient(recipientId, update);
  }
}
