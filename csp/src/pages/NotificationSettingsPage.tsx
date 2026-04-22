<<<<<<< HEAD
import { PageContainer } from "../components/common/PageContainer";

export function NotificationSettingsPage() {
  return <PageContainer>{null}</PageContainer>;
=======
import { useEffect, useMemo, useState } from "react";
import { getNotificationSettings, updateNotificationRecipient } from "../api/notifications";
import { isNotificationEventType, notificationEventTypes } from "../api/types";
import type { NotificationEventType, NotificationPreference, NotificationRecipient, NotificationRecipientUpdate } from "../api/types";
import { EmptyState } from "../components/common/EmptyState";
import { LoadingState } from "../components/common/LoadingState";
import { PageContainer } from "../components/common/PageContainer";
import { TableShell } from "../components/common/TableShell";
import { formatTimestamp } from "../lib/format";
import { useAsyncData } from "../lib/useAsyncData";

const eventLabels: Record<NotificationEventType, string> = {
  critical_risk: "Critical Risk",
  node_registration: "Node Join / Rejoin",
  connectivity_loss: "Connectivity Loss",
  battery_degradation: "Battery Degradation",
  time_sync_failure: "Time Sync Failure",
  nn_update_failure: "Neighbor Update Failure",
  config_update_failure: "Config Update Failure",
};

function normalizePreferences(preferences: NotificationPreference[]): NotificationPreference[] {
  const byType = new Map<NotificationEventType, NotificationPreference>();

  for (const preference of preferences) {
    if (!isNotificationEventType(preference.eventType)) {
      continue;
    }

    byType.set(preference.eventType, {
      eventType: preference.eventType,
      emailEnabled: preference.emailEnabled,
      smsEnabled: preference.smsEnabled,
    });
  }

  return notificationEventTypes.map((eventType) => ({
    eventType,
    emailEnabled: byType.get(eventType)?.emailEnabled ?? false,
    smsEnabled: byType.get(eventType)?.smsEnabled ?? false,
  }));
}

function toDraft(recipient: NotificationRecipient): NotificationRecipientUpdate {
  return {
    displayName: recipient.displayName,
    emailAddress: recipient.emailAddress,
    phoneNumber: recipient.phoneNumber,
    isEnabled: recipient.isEnabled,
    preferences: normalizePreferences(recipient.preferences),
  };
}

export function NotificationSettingsPage() {
  const { data, error, loading, reload } = useAsyncData(getNotificationSettings, []);
  const [drafts, setDrafts] = useState<Record<string, NotificationRecipientUpdate>>({});
  const [savingRecipientId, setSavingRecipientId] = useState<string | null>(null);
  const [saveError, setSaveError] = useState<string | null>(null);

  useEffect(() => {
    if (!data) {
      return;
    }

    setDrafts(
      Object.fromEntries(data.recipients.map((recipient) => [recipient.id, toDraft(recipient)])),
    );
  }, [data]);

  const deliveries = useMemo(() => data?.deliveries ?? [], [data]);

  function updateDraft(recipientId: string, updater: (draft: NotificationRecipientUpdate) => NotificationRecipientUpdate) {
    setDrafts((current) => {
      const existing = current[recipientId];
      if (!existing) {
        return current;
      }

      return {
        ...current,
        [recipientId]: updater(existing),
      };
    });
  }

  async function saveRecipient(recipient: NotificationRecipient) {
    const draft = drafts[recipient.id];
    if (!draft) {
      return;
    }

    setSavingRecipientId(recipient.id);
    setSaveError(null);

    try {
      await updateNotificationRecipient(recipient.id, draft);
      await reload();
    } catch (saveFailure) {
      setSaveError(saveFailure instanceof Error ? saveFailure.message : "Unable to save notification settings.");
    } finally {
      setSavingRecipientId(null);
    }
  }

  if (loading) {
    return <LoadingState label="Loading notification settings..." />;
  }

  if (error || !data) {
    return <EmptyState title="Unable to load notifications" message={error ?? "Notification settings are unavailable."} />;
  }

  return (
    <PageContainer
      description="Route alert channels by recipient and review recent delivery attempts."
    >
      {saveError ? <div className="notification-banner notification-banner-error">{saveError}</div> : null}

      <div className="notification-recipient-grid">
        {data.recipients.map((recipient) => {
          const draft = drafts[recipient.id];
          if (!draft) {
            return null;
          }

          return (
            <section className="card notification-recipient-card" key={recipient.id}>
              <div className="section-heading">
                <div>
                  <h2>{recipient.displayName}</h2>
                  <p>Channel routing and per-event delivery preferences.</p>
                </div>
              </div>

              <div className="notification-recipient-status-row">
                <label className="notification-switch notification-switch-card">
                  <input
                    type="checkbox"
                    checked={draft.isEnabled}
                    onChange={(event) =>
                      updateDraft(recipient.id, (current) => ({
                        ...current,
                        isEnabled: event.target.checked,
                      }))
                    }
                  />
                  <span>Recipient enabled</span>
                </label>
              </div>

              <div className="notification-contact-grid">
                <label className="notification-field">
                  <span>Name</span>
                  <input
                    type="text"
                    value={draft.displayName}
                    onChange={(event) =>
                      updateDraft(recipient.id, (current) => ({
                        ...current,
                        displayName: event.target.value,
                      }))
                    }
                  />
                </label>
                <label className="notification-field">
                  <span>Email</span>
                  <input
                    type="email"
                    value={draft.emailAddress}
                    onChange={(event) =>
                      updateDraft(recipient.id, (current) => ({
                        ...current,
                        emailAddress: event.target.value,
                      }))
                    }
                  />
                </label>
                <label className="notification-field">
                  <span>SMS number</span>
                  <input
                    type="tel"
                    placeholder="+17655551234"
                    value={draft.phoneNumber ?? ""}
                    onChange={(event) =>
                      updateDraft(recipient.id, (current) => ({
                        ...current,
                        phoneNumber: event.target.value.trim() === "" ? null : event.target.value,
                      }))
                    }
                  />
                </label>
              </div>

              <TableShell
                className="notification-preference-table"
                columns={["Event", "Email", "SMS"]}
              >
                {draft.preferences.map((preference) => (
                  <tr key={preference.eventType}>
                    <td>{eventLabels[preference.eventType]}</td>
                    <td>
                      <label className="notification-checkbox notification-checkbox-compact">
                        <input
                          type="checkbox"
                          checked={preference.emailEnabled}
                          onChange={(event) =>
                            updateDraft(recipient.id, (current) => ({
                              ...current,
                              preferences: current.preferences.map((entry) =>
                                entry.eventType === preference.eventType
                                  ? { ...entry, emailEnabled: event.target.checked }
                                  : entry,
                              ),
                            }))
                          }
                        />
                        <span className="sr-only">Enable email for {eventLabels[preference.eventType]}</span>
                      </label>
                    </td>
                    <td>
                      <label className="notification-checkbox notification-checkbox-compact">
                        <input
                          type="checkbox"
                          checked={preference.smsEnabled}
                          onChange={(event) =>
                            updateDraft(recipient.id, (current) => ({
                              ...current,
                              preferences: current.preferences.map((entry) =>
                                entry.eventType === preference.eventType
                                  ? { ...entry, smsEnabled: event.target.checked }
                                  : entry,
                              ),
                            }))
                          }
                        />
                        <span className="sr-only">Enable SMS for {eventLabels[preference.eventType]}</span>
                      </label>
                    </td>
                  </tr>
                ))}
              </TableShell>

              <div className="notification-card-actions">
                <button
                  className="action-button"
                  type="button"
                  onClick={() => void saveRecipient(recipient)}
                  disabled={savingRecipientId === recipient.id}
                >
                  {savingRecipientId === recipient.id ? "Saving..." : "Save recipient"}
                </button>
              </div>
            </section>
          );
        })}
      </div>

      <section className="card">
        <div className="section-heading">
          <div>
            <h2>Recent Deliveries</h2>
            <p>Latest email and SMS attempts recorded by the backend.</p>
          </div>
        </div>

        {deliveries.length === 0 ? (
          <EmptyState title="No deliveries yet" message="Notification attempts will appear here after alerts are dispatched." />
        ) : (
          <TableShell
            className="notification-delivery-table"
            columns={["Time", "Recipient", "Channel", "Destination", "Event", "Status", "Subject"]}
          >
            {deliveries.map((delivery) => (
              <tr key={delivery.id}>
                <td>{formatTimestamp(delivery.occurredAt)}</td>
                <td>{delivery.recipientName}</td>
                <td>
                  <span className={`badge direction-${delivery.channel === "email" ? "downlink" : "lateral"}`}>{delivery.channel}</span>
                </td>
                <td>{delivery.destination}</td>
                <td>{eventLabels[delivery.eventType]}</td>
                <td>
                  <span className={`badge status-${delivery.status}`}>{delivery.status}</span>
                </td>
                <td>
                  <div className="notification-delivery-subject">{delivery.subject}</div>
                  {delivery.failureReason ? <div className="notification-delivery-error">{delivery.failureReason}</div> : null}
                </td>
              </tr>
            ))}
          </TableShell>
        )}
      </section>
    </PageContainer>
  );
>>>>>>> 8ed2bba (notifications)
}
