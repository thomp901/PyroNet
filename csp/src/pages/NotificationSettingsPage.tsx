import { useEffect, useMemo, useState } from "react";
import { createNotificationRecipient, getNotificationSettings, updateNotificationRecipient } from "../api/notifications";
import type {
  NotificationDelivery,
  NotificationEventType,
  NotificationRecipient,
  NotificationRecipientWrite,
  NotificationSettingsResponse,
} from "../api/types";
import { EmptyState } from "../components/common/EmptyState";
import { LoadingState } from "../components/common/LoadingState";
import { PageContainer } from "../components/common/PageContainer";
import { TableShell } from "../components/common/TableShell";
import { formatTimestamp } from "../lib/format";
import { useAsyncData } from "../lib/useAsyncData";

type RecipientSlotKey = "ops-primary" | "fire-analyst";

interface RecipientSlot {
  key: RecipientSlotKey;
  title: string;
  defaultName: string;
  defaultEnabledEventTypes: NotificationEventType[];
}

interface RecipientDraft extends NotificationRecipientWrite {
  recipientId: string | null;
}

const notificationEventOrder: NotificationEventType[] = [
  "critical_risk",
  "system",
  "connectivity_loss",
  "battery_degradation",
  "time_sync_failure",
  "nn_update_failure",
  "config_update_failure",
];

const notificationEventLabels: Record<NotificationEventType, string> = {
  critical_risk: "Critical Risk",
  connectivity_loss: "Connectivity Loss",
  battery_degradation: "Battery Degradation",
  system: "Node Join / Rejoin",
  time_sync_failure: "Time Sync Failure",
  nn_update_failure: "Neighbor Update Failure",
  config_update_failure: "Config Update Failure",
};

const recipientSlots: RecipientSlot[] = [
  {
    key: "ops-primary",
    title: "Ops Primary",
    defaultName: "Ops Primary",
    defaultEnabledEventTypes: [
      "critical_risk",
      "connectivity_loss",
      "battery_degradation",
      "time_sync_failure",
      "nn_update_failure",
      "config_update_failure",
    ],
  },
  {
    key: "fire-analyst",
    title: "Fire Analyst",
    defaultName: "Fire Analyst",
    defaultEnabledEventTypes: ["critical_risk", "connectivity_loss"],
  },
];

function normalizeRecipientLabel(value: string) {
  return value.trim().toLowerCase();
}

function isValidEmailAddress(value: string) {
  const normalized = value.trim();
  return normalized.length > 3 && normalized.includes("@");
}

function areEventListsEqual(left: NotificationEventType[], right: NotificationEventType[]) {
  if (left.length !== right.length) {
    return false;
  }

  const leftSorted = [...left].sort();
  const rightSorted = [...right].sort();
  return leftSorted.every((value, index) => value === rightSorted[index]);
}

function getEnabledEventTypes(recipient: NotificationRecipient | null, slot: RecipientSlot) {
  if (!recipient) {
    return slot.defaultEnabledEventTypes;
  }

  return recipient.preferences.filter((preference) => preference.isEnabled).map((preference) => preference.eventType);
}

function buildDraft(slot: RecipientSlot, recipient: NotificationRecipient | null): RecipientDraft {
  return {
    recipientId: recipient?.id ?? null,
    displayName: recipient?.displayName ?? slot.defaultName,
    emailAddress: recipient?.emailAddress ?? "",
    isEnabled: recipient?.isEnabled ?? true,
    enabledEventTypes: getEnabledEventTypes(recipient, slot),
  };
}

function mapRecipientsToSlots(recipients: NotificationRecipient[]) {
  const remainingRecipients = [...recipients];
  const slotMap = new Map<RecipientSlotKey, NotificationRecipient | null>();

  for (const slot of recipientSlots) {
    const matchingIndex = remainingRecipients.findIndex(
      (recipient) => normalizeRecipientLabel(recipient.displayName) === normalizeRecipientLabel(slot.title),
    );

    if (matchingIndex >= 0) {
      slotMap.set(slot.key, remainingRecipients[matchingIndex]);
      remainingRecipients.splice(matchingIndex, 1);
      continue;
    }

    slotMap.set(slot.key, null);
  }

  for (const slot of recipientSlots) {
    if (!slotMap.get(slot.key)) {
      slotMap.set(slot.key, remainingRecipients.shift() ?? null);
    }
  }

  return slotMap;
}

function deliveryStatusLabel(status: NotificationDelivery["status"]) {
  return status.replace(/_/g, " ");
}

function getDeliveryDestination(
  delivery: NotificationDelivery,
  recipients: NotificationSettingsResponse["recipients"],
) {
  const recipient = recipients.find((entry) => entry.id === delivery.recipientId);
  return recipient?.emailAddress ?? "N/A";
}

function isDraftDirty(slot: RecipientSlot, recipient: NotificationRecipient | null, draft: RecipientDraft) {
  const baseline = buildDraft(slot, recipient);
  return (
    baseline.displayName !== draft.displayName ||
    baseline.emailAddress !== draft.emailAddress ||
    baseline.isEnabled !== draft.isEnabled ||
    !areEventListsEqual(baseline.enabledEventTypes, draft.enabledEventTypes)
  );
}

export function NotificationSettingsPage() {
  const { data, error, loading } = useAsyncData(getNotificationSettings, []);
  const [settings, setSettings] = useState<NotificationSettingsResponse | null>(null);
  const [drafts, setDrafts] = useState<Record<RecipientSlotKey, RecipientDraft>>({
    "ops-primary": buildDraft(recipientSlots[0], null),
    "fire-analyst": buildDraft(recipientSlots[1], null),
  });
  const [savingSlotKey, setSavingSlotKey] = useState<RecipientSlotKey | null>(null);
  const [statusMessage, setStatusMessage] = useState<string | null>(null);
  const [statusTone, setStatusTone] = useState<"success" | "error">("success");

  useEffect(() => {
    if (data) {
      setSettings(data);
    }
  }, [data]);

  const recipientSlotMap = useMemo(() => mapRecipientsToSlots(settings?.recipients ?? []), [settings]);

  useEffect(() => {
    setDrafts({
      "ops-primary": buildDraft(recipientSlots[0], recipientSlotMap.get("ops-primary") ?? null),
      "fire-analyst": buildDraft(recipientSlots[1], recipientSlotMap.get("fire-analyst") ?? null),
    });
  }, [recipientSlotMap]);

  if (loading) {
    return <LoadingState label="Loading notification settings..." />;
  }

  if (error || !settings) {
    return <EmptyState title="Unable to load notifications" message={error ?? "Notification settings are unavailable."} />;
  }

  async function saveRecipient(slot: RecipientSlot) {
    const recipient = recipientSlotMap.get(slot.key) ?? null;
    const draft = drafts[slot.key];
    const displayName = draft.displayName.trim();
    const emailAddress = draft.emailAddress.trim();

    if (!displayName) {
      setStatusTone("error");
      setStatusMessage(`Enter a recipient name for ${slot.title}.`);
      return;
    }

    if (!isValidEmailAddress(emailAddress)) {
      setStatusTone("error");
      setStatusMessage(`Enter a valid email address for ${slot.title}.`);
      return;
    }

    setSavingSlotKey(slot.key);
    setStatusMessage(null);

    try {
      const payload: NotificationRecipientWrite = {
        displayName,
        emailAddress,
        isEnabled: draft.isEnabled,
        enabledEventTypes: draft.enabledEventTypes,
      };

      const updated = recipient?.id
        ? await updateNotificationRecipient(recipient.id, payload)
        : await createNotificationRecipient(payload);

      setSettings(updated);
      setStatusTone("success");
      setStatusMessage(`${slot.title} saved.`);
    } catch (mutationError) {
      setStatusTone("error");
      setStatusMessage(mutationError instanceof Error ? mutationError.message : "Unable to save recipient.");
    } finally {
      setSavingSlotKey(null);
    }
  }

  function setRecipientDraft(slotKey: RecipientSlotKey, updater: (current: RecipientDraft) => RecipientDraft) {
    setDrafts((current) => ({
      ...current,
      [slotKey]: updater(current[slotKey]),
    }));
  }

  function toggleEvent(slotKey: RecipientSlotKey, eventType: NotificationEventType) {
    setRecipientDraft(slotKey, (current) => ({
      ...current,
      enabledEventTypes: current.enabledEventTypes.includes(eventType)
        ? current.enabledEventTypes.filter((value) => value !== eventType)
        : [...current.enabledEventTypes, eventType],
    }));
  }

  return (
    <PageContainer description="Route alert channels by recipient and review recent delivery attempts.">
      {statusMessage ? (
        <div className={statusTone === "error" ? "inline-status inline-status-error" : "inline-status"}>{statusMessage}</div>
      ) : null}

      <div className="notifications-grid">
        {recipientSlots.map((slot) => {
          const recipient = recipientSlotMap.get(slot.key) ?? null;
          const draft = drafts[slot.key];
          const isDirty = isDraftDirty(slot, recipient, draft);
          const isSaving = savingSlotKey === slot.key;

          return (
            <form
              key={slot.key}
              className="card notification-card"
              onSubmit={(event) => {
                event.preventDefault();
                void saveRecipient(slot);
              }}
            >
              <div className="section-heading notification-card-heading">
                <div>
                  <h2>{slot.title}</h2>
                  <p>Channel routing and per-event delivery preferences.</p>
                </div>

                <label className="notification-recipient-toggle">
                  <input
                    type="checkbox"
                    checked={draft.isEnabled}
                    onChange={(event) =>
                      setRecipientDraft(slot.key, (current) => ({
                        ...current,
                        isEnabled: event.target.checked,
                      }))
                    }
                  />
                  <span>Recipient enabled</span>
                </label>
              </div>

              <div className="form-grid notification-recipient-grid">
                <label className="field">
                  <span>Name</span>
                  <input
                    type="text"
                    value={draft.displayName}
                    onChange={(event) =>
                      setRecipientDraft(slot.key, (current) => ({
                        ...current,
                        displayName: event.target.value,
                      }))
                    }
                  />
                </label>

                <label className="field">
                  <span>Email</span>
                  <input
                    type="email"
                    value={draft.emailAddress}
                    onChange={(event) =>
                      setRecipientDraft(slot.key, (current) => ({
                        ...current,
                        emailAddress: event.target.value,
                      }))
                    }
                  />
                </label>

                <label className="field notification-sms-field">
                  <span>SMS number</span>
                  <input type="text" value="Coming soon" disabled readOnly />
                </label>
              </div>

              <TableShell className="notification-preferences-table" columns={["Event", "Email", "SMS"]}>
                {notificationEventOrder.map((eventType) => (
                  <tr key={`${slot.key}-${eventType}`}>
                    <td>{notificationEventLabels[eventType]}</td>
                    <td className="notification-preference-cell">
                      <input
                        type="checkbox"
                        checked={draft.enabledEventTypes.includes(eventType)}
                        disabled={!draft.isEnabled}
                        onChange={() => toggleEvent(slot.key, eventType)}
                        aria-label={`${notificationEventLabels[eventType]} email delivery`}
                      />
                    </td>
                    <td className="notification-preference-cell">
                      <input
                        type="checkbox"
                        checked={false}
                        disabled
                        aria-label={`${notificationEventLabels[eventType]} SMS delivery unavailable`}
                      />
                    </td>
                  </tr>
                ))}
              </TableShell>

              <p className="notification-support-note">SMS choices are displayed for future support but are not saved yet.</p>

              <div className="notification-card-actions">
                <button type="submit" className="primary-button" disabled={!isDirty || isSaving}>
                  {isSaving ? "Saving..." : "Save recipient"}
                </button>
              </div>
            </form>
          );
        })}
      </div>

      <div className="card">
        <div className="section-heading">
          <div>
            <h2>Recent Deliveries</h2>
            <p>Latest notification attempts recorded by the backend.</p>
          </div>
        </div>

        {settings.deliveries.length === 0 ? (
          <EmptyState title="No recent deliveries" message="Delivery attempts will appear here once alerts are routed." />
        ) : (
          <TableShell
            className="notification-deliveries-table"
            columns={["Time", "Recipient", "Channel", "Destination", "Event", "Status", "Subject"]}
          >
            {settings.deliveries.map((delivery) => (
              <tr key={delivery.id}>
                <td>{formatTimestamp(delivery.deliveredAt ?? delivery.occurredAt)}</td>
                <td>{delivery.recipientName}</td>
                <td>Email</td>
                <td>{getDeliveryDestination(delivery, settings.recipients)}</td>
                <td>{notificationEventLabels[delivery.eventType]}</td>
                <td>
                  <span className={`badge notification-status-${delivery.status}`}>{deliveryStatusLabel(delivery.status)}</span>
                </td>
                <td>{delivery.subject}</td>
              </tr>
            ))}
          </TableShell>
        )}
      </div>
    </PageContainer>
  );
}
