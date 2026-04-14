import { useEffect, useState } from "react";
import {
  createConfigRevision,
  getConfiguration,
  triggerNeighborDistribution,
  triggerThresholdPush,
  triggerTimeSync,
  updateNeighborRevision,
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

export function ConfigurationPage() {
  const { data, error, loading, reload } = useAsyncData(getConfiguration, []);
  const [thresholds, setThresholds] = useState<ConfigThresholds>(emptyThresholds);
  const [notes, setNotes] = useState("");
  const [selectedNodeId, setSelectedNodeId] = useState("");
  const [radiusMeters, setRadiusMeters] = useState(1500);
  const [neighborNodeIds, setNeighborNodeIds] = useState<string[]>([]);
  const [statusMessage, setStatusMessage] = useState<string | null>(null);
  const [submitting, setSubmitting] = useState(false);

  useEffect(() => {
    if (!data?.activeRevision) {
      return;
    }
    setThresholds(data.activeRevision.thresholds);
    setNotes(data.activeRevision.notes ?? "");
    const defaultNode = data.neighborTables[0]?.nodeId ?? "";
    setSelectedNodeId((current) => current || defaultNode);
  }, [data]);

  useEffect(() => {
    const selected = data?.neighborTables.find((table) => table.nodeId === selectedNodeId);
    if (!selected) {
      return;
    }
    setRadiusMeters(selected.radiusMeters ?? 1500);
    setNeighborNodeIds(selected.neighbors.map((neighbor) => neighbor.neighborNodeId));
  }, [data, selectedNodeId]);

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

  return (
    <PageContainer
      title="Configuration And Downlinks"
      description="Manage threshold revisions, inspect the active config revision, edit neighbor relationships, and trigger CSP-originated 0x04, 0x05, and 0x06 commands."
    >
      {statusMessage ? <div className="inline-status">{statusMessage}</div> : null}

      <div className="split-grid">
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
          </div>
          <div className="form-grid">
            {Object.entries(thresholds).map(([key, value]) => (
              <label key={key} className="field">
                <span>{key}</span>
                <input
                  type="number"
                  step="0.1"
                  value={value}
                  onChange={(event) =>
                    setThresholds((current) => ({
                      ...current,
                      [key]: Number(event.target.value),
                    }))
                  }
                />
              </label>
            ))}
            <label className="field field-full">
              <span>Revision notes</span>
              <textarea value={notes} onChange={(event) => setNotes(event.target.value)} rows={3} />
            </label>
          </div>
          <button type="submit" className="primary-button" disabled={submitting}>
            Publish new revision
          </button>
        </form>

        <form
          className="card form-card"
          onSubmit={(event) => {
            event.preventDefault();
            void runMutation(
              () =>
                updateNeighborRevision(selectedNodeId, {
                  radiusMeters,
                  neighborNodeIds,
                }),
              `Neighbor revision updated for ${selectedNodeId}.`,
            );
          }}
        >
          <div className="section-heading">
            <div>
              <h2>Neighbor relationships</h2>
              <p>Edit the current NN set for one node at a time.</p>
            </div>
          </div>
          <div className="form-grid">
            <label className="field">
              <span>Node</span>
              <select value={selectedNodeId} onChange={(event) => setSelectedNodeId(event.target.value)}>
                {data.neighborTables.map((table) => (
                  <option key={table.nodeId} value={table.nodeId}>
                    {table.nodeId}
                  </option>
                ))}
              </select>
            </label>
            <label className="field">
              <span>Radius meters</span>
              <input type="number" value={radiusMeters} onChange={(event) => setRadiusMeters(Number(event.target.value))} />
            </label>
            <div className="field field-full">
              <span>Neighbor members</span>
              <div className="checkbox-grid">
                {data.neighborTables
                  .filter((table) => table.nodeId !== selectedNodeId)
                  .map((table) => (
                    <label key={table.nodeId} className="checkbox-option">
                      <input
                        type="checkbox"
                        checked={neighborNodeIds.includes(table.nodeId)}
                        onChange={(event) => {
                          setNeighborNodeIds((current) =>
                            event.target.checked
                              ? [...current, table.nodeId]
                              : current.filter((nodeId) => nodeId !== table.nodeId),
                          );
                        }}
                      />
                      <span>{table.nodeId}</span>
                    </label>
                  ))}
              </div>
            </div>
          </div>
          <button type="submit" className="primary-button" disabled={submitting || !selectedNodeId}>
            Save NN revision
          </button>
        </form>
      </div>

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
            onClick={() => void runMutation(() => triggerNeighborDistribution({}), "0x04 neighbor distribution queued.")}
          >
            Trigger 0x04 distribution
          </button>
          <button
            type="button"
            className="secondary-button"
            disabled={submitting}
            onClick={() => void runMutation(() => triggerTimeSync({}), "0x05 time sync queued.")}
          >
            Trigger 0x05 time sync
          </button>
          <button
            type="button"
            className="secondary-button"
            disabled={submitting}
            onClick={() =>
              void runMutation(
                () => triggerThresholdPush({ configRevisionId: data.activeRevision?.id ?? undefined }),
                "0x06 threshold push queued.",
              )
            }
          >
            Trigger 0x06 threshold push
          </button>
        </div>
      </div>

      <div className="split-grid">
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

        <div className="card">
          <div className="section-heading">
            <div>
              <h2>Downlink monitor</h2>
              <p>Latest CSP-originated 0x04, 0x05, and 0x06 activity.</p>
            </div>
          </div>
          <TableShell columns={["Command", "Node", "Status", "Revision", "Sent"]}>
            {data.downlinks.map((downlink) => (
              <tr key={downlink.id}>
                <td>{downlink.commandCode}</td>
                <td>{downlink.nodeId}</td>
                <td>{downlink.status}</td>
                <td>{downlink.revisionNo ?? "N/A"}</td>
                <td>{formatTimestamp(downlink.sentAt)}</td>
              </tr>
            ))}
          </TableShell>
        </div>
      </div>
    </PageContainer>
  );
}
