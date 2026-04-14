import { latLngBounds } from "leaflet";
import { useEffect } from "react";
import { CircleMarker, MapContainer, Polyline, Popup, TileLayer, useMap } from "react-leaflet";
import type { MeshLink, NodeSummary } from "../../api/types";
import { riskLabel } from "../../lib/format";
import "./leaflet";

interface MapPreviewProps {
  nodes: NodeSummary[];
  links: MeshLink[];
}

const fallbackCenter: [number, number] = [34.2605, -118.472];

function FitToMesh({ nodes }: { nodes: NodeSummary[] }) {
  const map = useMap();

  useEffect(() => {
    if (nodes.length === 0) {
      return;
    }

    const bounds = latLngBounds(nodes.map((node) => [node.location.lat, node.location.lng] as [number, number]));
    map.fitBounds(bounds.pad(0.2));
  }, [map, nodes]);

  return null;
}

function markerColor(node: NodeSummary) {
  if (node.connectivity === "offline") {
    return "#5f6b7a";
  }
  if ((node.currentRiskLevel ?? 0) >= 5) {
    return "#cb3a28";
  }
  if ((node.currentRiskLevel ?? 0) >= 3) {
    return "#f18b2c";
  }
  return "#2f9d68";
}

export function MapPreview({ nodes, links }: MapPreviewProps) {
  const center = nodes[0]
    ? ([nodes[0].location.lat, nodes[0].location.lng] as [number, number])
    : fallbackCenter;

  return (
    <div className="card map-card">
      <div className="section-heading">
        <div>
          <h2>Mesh map</h2>
          <p>Leaflet topology view with node risk/connectivity state and nearest-neighbor links.</p>
        </div>
      </div>
      <div className="map-container">
        <MapContainer center={center} zoom={12} scrollWheelZoom className="leaflet-map">
          <TileLayer
            attribution="&copy; OpenStreetMap contributors"
            url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png"
          />
          <FitToMesh nodes={nodes} />
          {links.map((link) => (
            <Polyline
              key={`${link.ownerNodeId}-${link.neighborNodeId}-${link.distanceMeters}`}
              positions={link.points.map((point) => [point.lat, point.lng])}
              pathOptions={{ color: "#f4d35e", opacity: 0.45, weight: 2 }}
            />
          ))}
          {nodes.map((node) => (
            <CircleMarker
              key={node.id}
              center={[node.location.lat, node.location.lng]}
              radius={10}
              pathOptions={{
                color: "#08111a",
                weight: 2,
                fillColor: markerColor(node),
                fillOpacity: 0.9,
              }}
            >
              <Popup>
                <strong>{node.nodeId}</strong>
                <br />
                {node.displayName}
                <br />
                {riskLabel(node.currentRiskLevel)} / {node.connectivity}
                <br />
                {node.ipv6Address ?? "No IPv6"}
              </Popup>
            </CircleMarker>
          ))}
        </MapContainer>
      </div>
    </div>
  );
}
