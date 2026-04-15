import { useEffect, useState } from "react";
import {
  createConfigRevision,
  getConfiguration,
  triggerNeighborDistribution,
  triggerThresholdPush,
  triggerTimeSync,
} from "../api/configuration";
import type { ConfigThresholds } from "../api/types";
import { EmptyState } from "../components/common/EmptyState";
import { LoadingState } from "../components/common/LoadingState";
import { PageContainer } from "../components/common/PageContainer";
import { TableShell } from "../components/common/TableShell";
import { formatTimestamp } from "../lib/format";
import { useAsyncData } from "../lib/useAsyncData";

const emptyThresholds: ConfigThresholds = {
  l2TempThresh: 0,
  l2HumidityThresh: 0,
  l2VocThresh: 0,
  l3TempThresh: 0,
  l3HumidityThresh: 0,
  l3VocThresh: 0,
  l4VocThresh: 0,
  l5VocThresh: 0,
  l5Pm25Thresh: 0,
};

const thresholdGroups: Array<{
  title: string;
  fields: Array<{
    key: keyof ConfigThresholds;
    label: string;
  }>;
}> = [
  {
    title: "Level 2 thresholds",
    fields: [
      { key: "l2TempThresh", label: "Temperature threshold" },
      { key: "l2HumidityThresh", label: "Humidity threshold" },
      { key: "l2VocThresh", label: "VOC threshold" },
    ],
  },
  {
    title: "Level 3 thresholds",
    fields: [
      { key: "l3TempThresh", label: "Temperature threshold" },
      { key: "l3HumidityThresh", label: "Humidity threshold" },
      { key: "l3VocThresh", label: "VOC threshold" },
    ],
  },
  {
    title: "Level 4 thresholds",
    fields: [{ key: "l4VocThresh", label: "VOC threshold" }],
  },
  {
    title: "Level 5 thresholds",
    fields: [
      { key: "l5VocThresh", label: "VOC threshold" },
      { key: "l5Pm25Thresh", label: "PM2.5 threshold" },
    ],
  },
];

type TemperatureUnit = "C" | "F";

function isTemperatureThreshold(key: keyof ConfigThresholds) {
  return key === "l2TempThresh" || key === "l3TempThresh";
}

function convertCelsiusToFahrenheit(value: number) {
  return (value * 9) / 5 + 32;
}

function convertFahrenheitToCelsius(value: number) {
  return ((value - 32) * 5) / 9;
}

function roundThresholdValue(value: number) {
  return Math.round(value * 10) / 10;
}

export function ConfigurationPage() {
  const { data, error, loading, reload } = useAsyncData(getConfiguration, []);
  const [thresholds, setThresholds] = useState<ConfigThresholds>(emptyThresholds);
  const [notes, setNotes] = useState("");
  const [temperatureUnit, setTemperatureUnit] = useState<TemperatureUnit>("C");
  const [statusMessage, setStatusMessage] = useState<string | null>(null);
  const [submitting, setSubmitting] = useState(false);

  useEffect(() => {
    if (!data?.activeRevision) {
      return;
    }
    setThresholds(data.activeRevision.thresholds);
    setNotes(data.activeRevision.notes ?? "");
  }, [data]);

  if (loading) {
    return <LoadingState label="Loading configuration..." />;
  }

  if (error || !data) {
    return <EmptyState title="Unable to load configuration" message={error ?? "Configuration data is unavailable."} />;
  }

  async function runMutation(task: () => Promise<unknown>, message: string) {
    setSubmitting(true);
    setStatusMessage(null);
    try {
      await task();
      setStatusMessage(message);
      reload();
    } catch (mutationError) {
      setStatusMessage(mutationError instanceof Error ? mutationError.message : "Mutation failed.");
    } finally {
      setSubmitting(false);
    }
  }

  function getThresholdInputValue(key: keyof ConfigThresholds) {
    const value = thresholds[key];
    if (!isTemperatureThreshold(key)) {
      return value;
    }
    if (temperatureUnit === "F") {
      return roundThresholdValue(convertCelsiusToFahrenheit(value));
    }
    return value;
  }

  function updateThresholdValue(key: keyof ConfigThresholds, nextValue: number) {
    const normalizedValue =
      isTemperatureThreshold(key) && temperatureUnit === "F"
        ? roundThresholdValue(convertFahrenheitToCelsius(nextValue))
        : nextValue;

    setThresholds((current) => ({
      ...current,
      [key]: normalizedValue,
    }));
  }

  return (
    <PageContainer
      title="Configuration And Downlinks"
      description="Manage threshold revisions, inspect recent config revisions, and trigger CSP-originated downlink commands."
    >
      {statusMessage ? <div className="inline-status">{statusMessage}</div> : null}

      <div className="card">
        <div className="section-heading">
          <div>
            <h2>Downlink controls</h2>
            <p>Trigger mesh-level distribution and synchronization workflows from the CSP.</p>
          </div>
        </div>
        <div className="button-row">
          <button
            type="button"
            className="secondary-button"
            disabled={submitting}
            onClick={() => void runMutation(() => triggerNeighborDistribution({}), "NN Table Update queued.")}
          >
            Trigger NN Table Update
          </button>
          <button
            type="button"
            className="secondary-button"
            disabled={submitting}
            onClick={() => void runMutation(() => triggerTimeSync({}), "Time Sync queued.")}
          >
            Trigger Time Sync
          </button>
          <button
            type="button"
            className="secondary-button"
            disabled={submitting}
            onClick={() =>
              void runMutation(
                () => triggerThresholdPush({ configRevisionId: data.activeRevision?.id ?? undefined }),
                "Threshold Push queued.",
              )
            }
          >
            Trigger Threshold Push
          </button>
        </div>
      </div>

      <div className="stack-grid">
        <form
          className="card form-card"
          onSubmit={(event) => {
            event.preventDefault();
            void runMutation(
              () =>
                createConfigRevision({
                  thresholds,
                  notes,
                }),
              "New threshold revision created and queued for deployment.",
            );
          }}
        >
          <div className="section-heading">
            <div>
              <h2>Threshold revision</h2>
              <p>Edit the active thresholds and create a new revision.</p>
            </div>
            <label className="field threshold-units-field">
              <span>Temperature entry units</span>
              <select value={temperatureUnit} onChange={(event) => setTemperatureUnit(event.target.value as TemperatureUnit)}>
                <option value="C">Celsius (C)</option>
                <option value="F">Fahrenheit (F)</option>
              </select>
            </label>
          </div>
          <div className="threshold-groups">
            {thresholdGroups.map((group) => (
              <section key={group.title} className="threshold-group">
                <div className="threshold-group-heading">
                  <h3>{group.title}</h3>
                </div>
                <div className="form-grid threshold-group-grid">
                  {group.fields.map((field) => (
                    <label key={field.key} className="field">
                      <span>{isTemperatureThreshold(field.key) ? `${field.label} (${temperatureUnit})` : field.label}</span>
                      <input
                        type="number"
                        step="0.1"
                        value={getThresholdInputValue(field.key)}
                        onChange={(event) => updateThresholdValue(field.key, Number(event.target.value))}
                      />
                    </label>
                  ))}
                </div>
              </section>
            ))}
          </div>
          <div className="form-grid">
            <label className="field field-full">
              <span>Revision notes</span>
              <textarea value={notes} onChange={(event) => setNotes(event.target.value)} rows={3} />
            </label>
          </div>
          <button type="submit" className="primary-button" disabled={submitting}>
            Publish new revision
          </button>
        </form>

        <div className="card">
          <div className="section-heading">
            <div>
              <h2>Config revisions</h2>
              <p>Recent threshold revisions and activation status.</p>
            </div>
          </div>
          <TableShell columns={["Config ID", "Activated", "Retired", "Notes"]}>
            {data.revisions.map((revision) => (
              <tr key={revision.id}>
                <td>{revision.configId}</td>
                <td>{formatTimestamp(revision.activatedAt)}</td>
                <td>{formatTimestamp(revision.retiredAt)}</td>
                <td>{revision.notes ?? "N/A"}</td>
              </tr>
            ))}
          </TableShell>
        </div>
      </div>
    </PageContainer>
  );
}
