import type { NotificationRecipientUpdate, NotificationSettingsResponse } from "./types";
import { apiGet, apiPut } from "../lib/http";

export async function getNotificationSettings() {
  return apiGet<NotificationSettingsResponse>("/notifications");
}

export async function updateNotificationRecipient(recipientId: string, update: NotificationRecipientUpdate) {
  return apiPut<NotificationSettingsResponse, NotificationRecipientUpdate>(
    `/notifications/recipients/${encodeURIComponent(recipientId)}`,
    update,
  );
}
