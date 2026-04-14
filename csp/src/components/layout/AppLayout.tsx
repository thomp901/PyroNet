import { Outlet, useLocation } from "react-router-dom";
import { AppSidebar } from "./AppSidebar";
import { AppHeader } from "./AppHeader";

function getPageTitle(pathname: string) {
  if (pathname === "/") {
    return "Dashboard";
  }
  if (pathname.startsWith("/nodes/")) {
    return "Node Detail";
  }
  if (pathname.startsWith("/nodes")) {
    return "Node Fleet";
  }
  if (pathname.startsWith("/alerts")) {
    return "Alerts";
  }
  if (pathname.startsWith("/history")) {
    return "History";
  }
  if (pathname.startsWith("/configuration")) {
    return "Configuration";
  }
  if (pathname.startsWith("/notifications")) {
    return "Notifications";
  }
  return "PyroNet CSP";
}

export function AppLayout() {
  const location = useLocation();
  const title = getPageTitle(location.pathname);

  return (
    <div className="app-shell">
      <AppSidebar />
      <div className="content-shell">
        <AppHeader title={title} />
        <main className="content-main">
          <Outlet />
        </main>
      </div>
    </div>
  );
}
