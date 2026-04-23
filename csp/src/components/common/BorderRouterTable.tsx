import type { GatewayMarker } from "../../api/types";
import { Link } from "react-router-dom";
import { formatLocation, formatTimestamp } from "../../lib/format";
import { TableShell } from "./TableShell";

interface BorderRouterTableProps {
  borderRouters: GatewayMarker[];
  className?: string;
}

export function BorderRouterTable({
  borderRouters,
  className = "table-shell-compact border-router-table",
}: BorderRouterTableProps) {
  return (
    <TableShell className={className} columns={["Border router", "Location", "Software", "Last registered"]}>
      {borderRouters.map((borderRouter) => (
        <tr key={borderRouter.id}>
          <td>
            <Link className="table-link" to={`/border-routers/${borderRouter.gatewayId}`}>
              {borderRouter.gatewayId}
            </Link>
          </td>
          <td>{formatLocation(borderRouter.location.label, borderRouter.location.lat, borderRouter.location.lng)}</td>
          <td>{borderRouter.softwareVersion ?? "Unknown"}</td>
          <td>{formatTimestamp(borderRouter.lastRegisteredAt)}</td>
        </tr>
      ))}
    </TableShell>
  );
}
