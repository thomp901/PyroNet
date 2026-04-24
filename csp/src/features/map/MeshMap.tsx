import { latLngBounds, type Map as LeafletMap } from "leaflet";
import { useEffect, useState } from "react";
import { CircleMarker, MapContainer, Pane, Polyline, Popup, TileLayer, useMap } from "react-leaflet";
import { useNavigate } from "react-router-dom";
import type { GatewayMarker, MeshLink, NodeSummary } from "../../api/types";
import { formatTimestamp, riskLabel } from "../../lib/format";
import "./leaflet";

interface MeshMapProps {
  nodes: NodeSummary[];
  gateways?: GatewayMarker[];
  links: MeshLink[];
  className?: string;
  legendDefaultOpen?: boolean;
  focusNodeId?: number;
  focusZoom?: number;
}

const nodeLegendItems = [
  { label: "Risk Level 5", color: "#cb3a28" },
  { label: "Risk Level 4", color: "#f18b2c" },
  { label: "Risk Level 1-3", color: "#2f9d68" },
  { label: "Offline", color: "#5f6b7a" },
  { label: "Gateway", color: "#1f78ff" },
] as const;

const fallbackCenter: [number, number] = [34.2605, -118.472];
const parentLinkColor = "#1f78ff";

interface ParentLink {
  id: string;
  childNodeId: number;
  parentLabel: string;
  points: [[number, number], [number, number]];
}

function applyMeshViewport(
  map: LeafletMap,
  nodes: NodeSummary[],
  gateways: GatewayMarker[],
  focusNodeId?: number,
  focusZoom = 15,
) {
  if (nodes.length === 0 && gateways.length === 0) {
    return;
  }

  if (focusNodeId !== undefined) {
    const focusedNode = nodes.find((node) => node.nodeId === focusNodeId);
    if (focusedNode) {
      map.setView([focusedNode.location.lat, focusedNode.location.lng], focusZoom, { animate: false });
      return;
    }
  }

  const bounds = latLngBounds([
    ...nodes.map((node) => [node.location.lat, node.location.lng] as [number, number]),
    ...gateways.map((gateway) => [gateway.location.lat, gateway.location.lng] as [number, number]),
  ]);
  map.fitBounds(bounds.pad(0.2), { animate: false });
}

function FitToMesh({
  nodes,
  gateways,
  focusNodeId,
  focusZoom = 15,
}: {
  nodes: NodeSummary[];
  gateways: GatewayMarker[];
  focusNodeId?: number;
  focusZoom?: number;
}) {
  const map = useMap();

  useEffect(() => {
    applyMeshViewport(map, nodes, gateways, focusNodeId, focusZoom);
  }, [focusNodeId, focusZoom, gateways, map, nodes]);

  return null;
}

function RegisterMapInstance({ onReady }: { onReady: (map: LeafletMap) => void }) {
  const map = useMap();

  useEffect(() => {
    onReady(map);
  }, [map, onReady]);

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

function buildParentLinks(nodes: NodeSummary[], gateways: GatewayMarker[]): ParentLink[] {
  const nodesByIpv6 = new Map(
    nodes
      .filter((node) => node.ipv6Address !== null)
      .map((node) => [node.ipv6Address, node] as const),
  );
  const gatewaysByIpv6 = new Map(
    gateways
      .filter((gateway) => gateway.ipv6Address !== null)
      .map((gateway) => [gateway.ipv6Address, gateway] as const),
  );

  return nodes.flatMap((node) => {
    if (!node.currentParentIpv6) {
      return [];
    }

    const parentNode = nodesByIpv6.get(node.currentParentIpv6);
    if (parentNode && parentNode.nodeId !== node.nodeId) {
      return [
        {
          id: `${node.nodeId}-node-${parentNode.nodeId}`,
          childNodeId: node.nodeId,
          parentLabel: `node ${parentNode.nodeId}`,
          points: [
            [node.location.lat, node.location.lng],
            [parentNode.location.lat, parentNode.location.lng],
          ],
        },
      ];
    }

    const parentGateway = gatewaysByIpv6.get(node.currentParentIpv6);
    if (!parentGateway) {
      return [];
    }

    return [
      {
        id: `${node.nodeId}-gateway-${parentGateway.gatewayId}`,
        childNodeId: node.nodeId,
        parentLabel: `gateway ${parentGateway.gatewayId}`,
        points: [
          [node.location.lat, node.location.lng],
          [parentGateway.location.lat, parentGateway.location.lng],
        ],
      },
    ];
  });
}

export function MeshMap({
  nodes,
  gateways = [],
  className = "leaflet-map",
  legendDefaultOpen = false,
  focusNodeId,
  focusZoom,
}: MeshMapProps) {
  const navigate = useNavigate();
  const [map, setMap] = useState<LeafletMap | null>(null);
  const [showParentLinks, setShowParentLinks] = useState(false);
  const parentLinks = buildParentLinks(nodes, gateways);
  const center = nodes[0]
    ? ([nodes[0].location.lat, nodes[0].location.lng] as [number, number])
    : gateways[0]
      ? ([gateways[0].location.lat, gateways[0].location.lng] as [number, number])
      : fallbackCenter;

  return (
    <div className="mesh-map-shell">
      <MapContainer center={center} zoom={12} scrollWheelZoom className={className} zoomAnimation={false} markerZoomAnimation={false}>
        <TileLayer
          attribution='&copy; OpenStreetMap contributors &copy; CARTO'
          url="https://{s}.basemaps.cartocdn.com/rastertiles/voyager/{z}/{x}/{y}{r}.png"
        />
        <Pane name="parent-links-pane" style={{ zIndex: 380 }} />
        <Pane name="node-markers-pane" style={{ zIndex: 420 }} />
        <RegisterMapInstance onReady={setMap} />
        <FitToMesh nodes={nodes} gateways={gateways} focusNodeId={focusNodeId} focusZoom={focusZoom} />
        {showParentLinks
          ? parentLinks.map((link) => (
              <Polyline
                key={link.id}
                pane="parent-links-pane"
                positions={link.points}
                pathOptions={{
                  color: parentLinkColor,
                  weight: 2,
                  opacity: 0.8,
                  dashArray: "6 6",
                }}
              >
                <Popup>
                  <strong>Preferred parent</strong>
                  <br />
                  Node {link.childNodeId} to {link.parentLabel}
                </Popup>
              </Polyline>
            ))
          : null}
        {nodes.map((node) => (
          <CircleMarker
            key={node.id}
            pane="node-markers-pane"
            center={[node.location.lat, node.location.lng]}
            radius={10}
            pathOptions={{
              color: "#08111a",
              weight: 2,
              fillColor: markerColor(node),
              fillOpacity: 0.9,
            }}
            eventHandlers={{
              click: () => navigate(`/nodes/${node.nodeId}`),
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
        {gateways.map((gateway) => (
          <CircleMarker
            key={gateway.id}
            pane="node-markers-pane"
            center={[gateway.location.lat, gateway.location.lng]}
            radius={8}
            pathOptions={{
              color: "#08111a",
              weight: 2,
              fillColor: "#1f78ff",
              fillOpacity: 0.95,
            }}
          >
            <Popup>
              <strong>Gateway {gateway.gatewayId}</strong>
              <br />
              {gateway.softwareVersion ? `SW ${gateway.softwareVersion}` : "Software unknown"}
              <br />
              {gateway.lastRegisteredAt ? formatTimestamp(gateway.lastRegisteredAt) : "No registration timestamp"}
            </Popup>
          </CircleMarker>
        ))}
      </MapContainer>

      <button
        className="map-reset-button"
        type="button"
        onClick={() => {
          if (map) {
            applyMeshViewport(map, nodes, gateways, focusNodeId, focusZoom);
          }
        }}
      >
        Reset view
      </button>

      <details className="map-legend" open={legendDefaultOpen}>
        <summary className="map-legend-title">Map legend</summary>
        <ul className="map-legend-list" aria-label="Map color legend">
          {nodeLegendItems.map((item) => (
            <li key={item.label} className="map-legend-item">
              <span className="map-legend-swatch" style={{ backgroundColor: item.color }} aria-hidden="true" />
              <span>{item.label}</span>
            </li>
          ))}
        </ul>
        <label className="map-legend-toggle">
          <input
            type="checkbox"
            checked={showParentLinks}
            onChange={(event) => setShowParentLinks(event.target.checked)}
          />
          <span className="map-legend-line-swatch" aria-hidden="true" />
          <span>Show Links</span>
        </label>
      </details>
    </div>
  );
}
