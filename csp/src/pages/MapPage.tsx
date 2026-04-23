import { getDashboard } from "../api/dashboard";
import { EmptyState } from "../components/common/EmptyState";
import { LoadingState } from "../components/common/LoadingState";
import { MeshMap } from "../features/map/MeshMap";
import { useAsyncData } from "../lib/useAsyncData";

export function MapPage() {
  const { data, error, loading } = useAsyncData(getDashboard, []);

  if (loading) {
    return (
      <div className="map-page-state">
        <LoadingState label="Loading mesh map..." />
      </div>
    );
  }

  if (error || !data) {
    return (
      <div className="map-page-state">
        <EmptyState title="Map unavailable" message={error ?? "Unable to load mesh map data."} />
      </div>
    );
  }

  return (
    <section className="map-page">
      <div className="map-page-toolbar">
        <div>
          <p className="map-page-kicker">Mesh topology</p>
          <h1>Operations map</h1>
        </div>
        <p className="map-page-summary">Live node risk level and connectivity across the deployed fleet.</p>
      </div>
      <div className="map-page-frame">
        <MeshMap
          nodes={data.fleet}
          gateways={data.gateways}
          links={data.neighborLinks}
          className="leaflet-map leaflet-map-fullscreen"
          legendDefaultOpen
        />
      </div>
    </section>
  );
}
