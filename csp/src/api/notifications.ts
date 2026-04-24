import type { NotificationRecipientUpdate, NotificationRecipientWrite, NotificationSettingsResponse } from "./types";
import { apiGet, apiPost, apiPut } from "../lib/http";

export async function getNotificationSettings() {
  return apiGet<NotificationSettingsResponse>("/notifications");
}

export async function updateNotificationRecipient(recipientId: string, update: NotificationRecipientUpdate) {
  return apiPut<NotificationSettingsResponse, NotificationRecipientUpdate>(
    `/notifications/recipients/${encodeURIComponent(recipientId)}`,
    update,
  );
}

export async function createNotificationRecipient(recipient: NotificationRecipientWrite) {
  return apiPost<NotificationSettingsResponse, NotificationRecipientWrite>("/notifications/recipients", recipient);
}
