import { useEffect, useState } from "react";
import { getNotificationSettings, updateNotificationRecipient } from "../api/notifications";
import type { NotificationRecipient } from "../api/types";
import { EmptyState } from "../components/common/EmptyState";
import { LoadingState } from "../components/common/LoadingState";
import { PageContainer } from "../components/common/PageContainer";
import { TableShell } from "../components/common/TableShell";
import { formatTimestamp } from "../lib/format";
import { useAsyncData } from "../lib/useAsyncData";

type RecipientDrafts = Record<
  string,
  {
    isEnabled: boolean;
    enabledEventTypes: string[];
  }
>;

export function NotificationSettingsPage() {
  const { data, error, loading, reload } = useAsyncData(getNotificationSettings, []);
  const [drafts, setDrafts] = useState<RecipientDrafts>({});
  const [statusMessage, setStatusMessage] = useState<string | null>(null);

  useEffect(() => {
    if (!data) {
      return;
    }
    const nextDrafts = data.recipients.reduce<RecipientDrafts>((accumulator, recipient) => {
      accumulator[recipient.id] = {
        isEnabled: recipient.isEnabled,
        enabledEventTypes: recipient.preferences.filter((preference) => preference.isEnabled).map((preference) => preference.eventType),
      };
      return accumulator;
    }, {});
    setDrafts(nextDrafts);
  }, [data]);

  if (loading) {
    return <LoadingState label="Loading notification settings..." />;
  }

  if (error || !data) {
    return (
      <EmptyState title="Unable to load notifications" message={error ?? "Notification settings are unavailable."} />
    );
  }

  async function saveRecipient(recipient: NotificationRecipient) {
    const draft = drafts[recipient.id];
    if (!draft) {
      return;
    }
    await updateNotificationRecipient(recipient.id, {
      isEnabled: draft.isEnabled,
      enabledEventTypes: draft.enabledEventTypes as NotificationRecipient["preferences"][number]["eventType"][],
    });
    setStatusMessage(`Updated notification settings for ${recipient.displayName}.`);
    reload();
  }

  return (
    <PageContainer
      title="Notification Settings"
      description="Choose which event types generate email and track whether notifications were sent."
    >
      {statusMessage ? <div className="inline-status">{statusMessage}</div> : null}

      <div className="stack-grid">
        {data.recipients.map((recipient) => {
          const draft = drafts[recipient.id];
          return (
            <article key={recipient.id} className="card form-card">
              <div className="section-heading">
                <div>
                  <h2>{recipient.displayName}</h2>
                  <p>{recipient.emailAddress}</p>
                </div>
                <label className="checkbox-option">
                  <input
                    type="checkbox"
                    checked={draft?.isEnabled ?? recipient.isEnabled}
                    onChange={(event) =>
                      setDrafts((current) => ({
                        ...current,
                        [recipient.id]: {
                          ...(current[recipient.id] ?? { enabledEventTypes: [], isEnabled: recipient.isEnabled }),
                          isEnabled: event.target.checked,
                        },
                      }))
                    }
                  />
                  <span>Recipient enabled</span>
                </label>
              </div>

              <div className="checkbox-grid">
                {recipient.preferences.map((preference) => {
                  const checked = draft?.enabledEventTypes.includes(preference.eventType) ?? preference.isEnabled;
                  return (
                    <label key={preference.eventType} className="checkbox-option">
                      <input
                        type="checkbox"
                        checked={checked}
                        onChange={(event) =>
                          setDrafts((current) => {
                            const currentDraft = current[recipient.id] ?? {
                              isEnabled: recipient.isEnabled,
                              enabledEventTypes: recipient.preferences
                                .filter((entry) => entry.isEnabled)
                                .map((entry) => entry.eventType),
                            };
                            return {
                              ...current,
                              [recipient.id]: {
                                ...currentDraft,
                                enabledEventTypes: event.target.checked
                                  ? [...currentDraft.enabledEventTypes, preference.eventType]
                                  : currentDraft.enabledEventTypes.filter((entry) => entry !== preference.eventType),
                              },
                            };
                          })
                        }
                      />
                      <span>{preference.eventType}</span>
                    </label>
                  );
                })}
              </div>

              <div className="button-row">
                <button type="button" className="primary-button" onClick={() => void saveRecipient(recipient)}>
                  Save recipient settings
                </button>
              </div>
            </article>
          );
        })}
      </div>

      <div className="card">
        <div className="section-heading">
          <div>
            <h2>Delivery log</h2>
            <p>Recent email delivery attempts and results.</p>
          </div>
        </div>
        <TableShell columns={["Occurred", "Recipient", "Event", "Subject", "Status", "Delivered"]}>
          {data.deliveries.map((delivery) => (
            <tr key={delivery.id}>
              <td>{formatTimestamp(delivery.occurredAt)}</td>
              <td>{delivery.recipientName}</td>
              <td>{delivery.eventType}</td>
              <td>{delivery.subject}</td>
              <td>{delivery.status}</td>
              <td>{formatTimestamp(delivery.deliveredAt)}</td>
            </tr>
          ))}
        </TableShell>
      </div>
    </PageContainer>
  );
}
