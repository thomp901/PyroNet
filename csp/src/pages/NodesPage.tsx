import { Link } from "react-router-dom";
import { listNodes } from "../api/nodes";
import { EmptyState } from "../components/common/EmptyState";
import { LoadingState } from "../components/common/LoadingState";
import { PageContainer } from "../components/common/PageContainer";
import { TableShell } from "../components/common/TableShell";
import { formatInteger, formatNullableNumber, formatTimestamp, riskLabel } from "../lib/format";
import { useAsyncData } from "../lib/useAsyncData";

export function NodesPage() {
  const { data, error, loading } = useAsyncData(listNodes, []);

  if (loading) {
    return <LoadingState label="Loading node inventory..." />;
  }

  if (error || !data) {
    return <EmptyState title="Unable to load nodes" message={error ?? "Node inventory is unavailable."} />;
  }

  return (
    <PageContainer
      title="Node Fleet"
      description="Complete node inventory with drill-down access to per-device identity, topology, telemetry, and incident history."
    >
      <div className="card">
        <TableShell columns={["Node", "IPv6", "Location", "Last seen", "Risk", "Latest telemetry", "Active revisions"]}>
          {data.map((node) => (
            <tr key={node.id}>
              <td>
                <Link className="table-link" to={`/nodes/${node.nodeId}`}>
                  {node.nodeId}
                </Link>
                <div className="table-subtle">{node.displayName}</div>
              </td>
              <td>{node.ipv6Address ?? "N/A"}</td>
              <td>{`${node.location.label} (${node.location.lat.toFixed(4)}, ${node.location.lng.toFixed(4)})`}</td>
              <td>
                {formatTimestamp(node.lastSeenAt)}
                <div className="table-subtle">{node.connectivity}</div>
              </td>
              <td>
                <span className={`badge risk-${node.currentRiskLevel ?? 0}`}>{riskLabel(node.currentRiskLevel)}</span>
              </td>
              <td>
                Temp {formatNullableNumber(node.latestTelemetry?.temperatureC ?? null, "°C")} / RH{" "}
                {formatNullableNumber(node.latestTelemetry?.humidityPct ?? null, "%")} / VOC{" "}
                {formatInteger(node.latestTelemetry?.vocIaq ?? null)} / PM2.5{" "}
                {formatNullableNumber(node.latestTelemetry?.pm25UgM3 ?? null, " ug/m3")}
              </td>
              <td>
                Config {node.activeConfigRevisionNo ?? "N/A"}
                <div className="table-subtle">NN rev {node.currentNeighborRevisionNo ?? "N/A"}</div>
              </td>
            </tr>
          ))}
        </TableShell>
      </div>
    </PageContainer>
  );
}
