import { useParams } from "react-router-dom";
import { getBorderRouterDetail } from "../api/nodes";
import { EmptyState } from "../components/common/EmptyState";
import { LoadingState } from "../components/common/LoadingState";
import { PageContainer } from "../components/common/PageContainer";
import { StatCard } from "../components/common/StatCard";
import { TableShell } from "../components/common/TableShell";
import { MeshMap } from "../features/map/MeshMap";
import { formatCoordinatePair, formatInteger, formatTimestamp } from "../lib/format";
import { parseGatewayId } from "../lib/gatewayId";
import { useAsyncData } from "../lib/useAsyncData";

export function BorderRouterDetailPage() {
  const params = useParams();
  const gatewayId = parseGatewayId(params.gatewayId);

  if (gatewayId === null) {
    return <EmptyState title="Invalid border-router detail request" message="Border router IDs must be valid unsigned 16-bit integers." />;
  }

  const { data, error, loading } = useAsyncData(() => getBorderRouterDetail(gatewayId), [gatewayId]);

  if (loading) {
    return <LoadingState label="Loading border-router detail..." />;
  }

  if (error || !data) {
    return <EmptyState title="Unable to load border router" message={error ?? "Border-router detail is unavailable."} />;
  }

  return (
    <PageContainer title={`Border Router ${data.gateway.gatewayId}`}>
      <div className="stat-grid">
        <StatCard label="Software" value={data.gateway.softwareVersion ?? "Unknown"} />
        <StatCard label="First registered" value={formatTimestamp(data.firstRegisteredAt)} />
        <StatCard label="Last registered" value={formatTimestamp(data.gateway.lastRegisteredAt)} />
        <StatCard label="Registrations" value={formatInteger(data.registrationCount)} />
      </div>

      <div className="dashboard-grid dashboard-grid-stack node-detail-hero">
        <article className="card map-card">
          <div className="section-heading">
            <div>
              <h2>Border-router map</h2>
            </div>
          </div>
          <div className="map-container">
            <MeshMap nodes={[]} gateways={[data.gateway]} links={[]} />
          </div>
        </article>

        <div className="stack-grid">
          <article className="card">
            <div className="section-heading">
              <div>
                <h2>Identity and registration</h2>
              </div>
            </div>
            <dl className="detail-list">
              <div>
                <dt>Border router ID</dt>
                <dd>{data.gateway.gatewayId}</dd>
              </div>
              <div>
                <dt>Software</dt>
                <dd>{data.gateway.softwareVersion ?? "Unknown"}</dd>
              </div>
              <div>
                <dt>Coordinates</dt>
                <dd>{formatCoordinatePair(data.gateway.location.lat, data.gateway.location.lng)}</dd>
              </div>
              <div>
                <dt>First registered</dt>
                <dd>{formatTimestamp(data.firstRegisteredAt)}</dd>
              </div>
              <div>
                <dt>Last registered</dt>
                <dd>{formatTimestamp(data.gateway.lastRegisteredAt)}</dd>
              </div>
              <div>
                <dt>Total registrations</dt>
                <dd>{formatInteger(data.registrationCount)}</dd>
              </div>
            </dl>
          </article>
        </div>
      </div>

      <article className="card">
        <div className="section-heading">
          <div>
            <h2>Registration history</h2>
          </div>
        </div>
        {data.recentRegistrations.length > 0 ? (
          <TableShell className="border-router-history-table" columns={["Observed", "Coordinates", "Software", "Backhaul"]}>
            {data.recentRegistrations.map((registration) => (
              <tr key={`${registration.reportedAt}-${registration.latitude}-${registration.longitude}`}>
                <td>{formatTimestamp(registration.reportedAt)}</td>
                <td>{formatCoordinatePair(registration.latitude, registration.longitude)}</td>
                <td>{registration.softwareVersion ?? "Unknown"}</td>
                <td>v{registration.backhaulVersion}</td>
              </tr>
            ))}
          </TableShell>
        ) : (
          <EmptyState title="No registration history" message="No border-router registrations have been recorded yet." />
        )}
      </article>
    </PageContainer>
  );
}
