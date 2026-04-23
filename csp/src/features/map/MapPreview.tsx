import type { KeyboardEvent, MouseEvent } from "react";
import { useNavigate } from "react-router-dom";
import type { GatewayMarker, MeshLink, NodeSummary } from "../../api/types";
import { MeshMap } from "./MeshMap";

interface MapPreviewProps {
  nodes: NodeSummary[];
  gateways: GatewayMarker[];
  links: MeshLink[];
}

export function MapPreview({ nodes, gateways, links }: MapPreviewProps) {
  const navigate = useNavigate();

  function openMapPage() {
    navigate("/map");
  }

  function handleKeyDown(event: KeyboardEvent<HTMLDivElement>) {
    if (event.key !== "Enter" && event.key !== " ") {
      return;
    }

    event.preventDefault();
    openMapPage();
  }

  function stopMapPreviewNavigation(event: MouseEvent<HTMLDivElement> | KeyboardEvent<HTMLDivElement>) {
    event.stopPropagation();
  }

  return (
    <div
      className="card map-card dashboard-link-card map-preview-card"
      onClick={openMapPage}
      onKeyDown={handleKeyDown}
      role="link"
      tabIndex={0}
      aria-label="Open full mesh map"
    >
      <div className="section-heading">
        <div>
          <h2>Mesh map</h2>
          <p>Map view with node risk level and connectivity state.</p>
        </div>
      </div>
      <div className="map-container" onClick={stopMapPreviewNavigation} onKeyDownCapture={stopMapPreviewNavigation}>
        <MeshMap nodes={nodes} gateways={gateways} links={links} />
      </div>
    </div>
  );
}
