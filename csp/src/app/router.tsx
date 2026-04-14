import { createBrowserRouter } from "react-router-dom";
import { AppLayout } from "../components/layout/AppLayout";
import { AlertsPage } from "../pages/AlertsPage";
import { ConfigurationPage } from "../pages/ConfigurationPage";
import { DashboardPage } from "../pages/DashboardPage";
import { HistoryPage } from "../pages/HistoryPage";
import { NodeDetailPage } from "../pages/NodeDetailPage";
import { NodesPage } from "../pages/NodesPage";
import { NotificationSettingsPage } from "../pages/NotificationSettingsPage";

export const router = createBrowserRouter([
  {
    path: "/",
    element: <AppLayout />,
    children: [
      {
        index: true,
        element: <DashboardPage />,
      },
      {
        path: "nodes",
        element: <NodesPage />,
      },
      {
        path: "nodes/:nodeId",
        element: <NodeDetailPage />,
      },
      {
        path: "alerts",
        element: <AlertsPage />,
      },
      {
        path: "history",
        element: <HistoryPage />,
      },
      {
        path: "configuration",
        element: <ConfigurationPage />,
      },
      {
        path: "notifications",
        element: <NotificationSettingsPage />,
      },
    ],
  },
]);
