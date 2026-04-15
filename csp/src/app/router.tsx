import { createBrowserRouter } from "react-router-dom";
import { AppLayout } from "../components/layout/AppLayout";
import { AlertsPage } from "../pages/AlertsPage";
import { ConfigurationPage } from "../pages/ConfigurationPage";
import { DashboardPage } from "../pages/DashboardPage";
import { HistoryPage } from "../pages/HistoryPage";
import { MapPage } from "../pages/MapPage";
import { NodeDetailPage } from "../pages/NodeDetailPage";
import { NodesPage } from "../pages/NodesPage";
import { NotificationSettingsPage } from "../pages/NotificationSettingsPage";

export const router = createBrowserRouter([
  {
    path: "/",
    element: <AppLayout />,
    handle: {
      title: "Dashboard",
    },
    children: [
      {
        index: true,
        element: <DashboardPage />,
        handle: {
          title: "Dashboard",
        },
      },
      {
        path: "map",
        element: <MapPage />,
        handle: {
          title: "Map",
          immersive: true,
        },
      },
      {
        path: "nodes",
        element: <NodesPage />,
        handle: {
          title: "Node Fleet",
        },
      },
      {
        path: "nodes/:nodeId",
        element: <NodeDetailPage />,
        handle: {
          title: "Node Detail",
        },
      },
      {
        path: "alerts",
        element: <AlertsPage />,
        handle: {
          title: "Alerts",
        },
      },
      {
        path: "history",
        element: <HistoryPage />,
        handle: {
          title: "History",
        },
      },
      {
        path: "configuration",
        element: <ConfigurationPage />,
        handle: {
          title: "Configuration",
        },
      },
      {
        path: "notifications",
        element: <NotificationSettingsPage />,
        handle: {
          title: "Notifications",
        },
      },
    ],
  },
]);
