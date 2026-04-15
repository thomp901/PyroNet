import { latLngBounds } from "leaflet";
import { useEffect } from "react";
import { CircleMarker, MapContainer, Popup, TileLayer, useMap } from "react-leaflet";
import type { MeshLink, NodeSummary } from "../../api/types";
import { riskLabel } from "../../lib/format";
import "./leaflet";

interface MeshMapProps {
  nodes: NodeSummary[];
  links: MeshLink[];
  className?: string;
  legendDefaultOpen?: boolean;
}

const nodeLegendItems = [
  { label: "Risk Level 5", color: "#cb3a28" },
  { label: "Risk Level 4", color: "#f18b2c" },
  { label: "Risk Level 1-3", color: "#2f9d68" },
  { label: "Offline", color: "#5f6b7a" },
] as const;

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
  if (node.currentRiskLevel === 5) {
    return "#cb3a28";
  }
  if (node.currentRiskLevel === 4) {
    return "#f18b2c";
  }
  return "#2f9d68";
}

export function MeshMap({ nodes, className = "leaflet-map", legendDefaultOpen = false }: MeshMapProps) {
  const center = nodes[0]
    ? ([nodes[0].location.lat, nodes[0].location.lng] as [number, number])
    : fallbackCenter;

  return (
    <div className="mesh-map-shell">
      <MapContainer center={center} zoom={12} scrollWheelZoom className={className}>
        <TileLayer
          attribution='&copy; OpenStreetMap contributors &copy; CARTO'
          url="https://{s}.basemaps.cartocdn.com/rastertiles/voyager/{z}/{x}/{y}{r}.png"
        />
        <FitToMesh nodes={nodes} />
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
              {riskLabel(node.currentRiskLevel)} / {node.connectivity}
              <br />
              {node.ipv6Address ?? "No IPv6"}
            </Popup>
          </CircleMarker>
        ))}
      </MapContainer>

      <details className="map-legend" open={legendDefaultOpen}>
        <summary className="map-legend-title">Node legend</summary>
        <ul className="map-legend-list" aria-label="Node color legend">
          {nodeLegendItems.map((item) => (
            <li key={item.label} className="map-legend-item">
              <span className="map-legend-swatch" style={{ backgroundColor: item.color }} aria-hidden="true" />
              <span>{item.label}</span>
            </li>
          ))}
        </ul>
      </details>
    </div>
  );
}
