import { Outlet, useMatches } from "react-router-dom";
import { AppSidebar } from "./AppSidebar";
import { AppHeader } from "./AppHeader";

interface RouteHandle {
  title?: string;
  immersive?: boolean;
}

function isRouteHandle(handle: unknown): handle is RouteHandle {
  return typeof handle === "object" && handle !== null;
}

export function AppLayout() {
  const matches = useMatches();
  const routeHandle = matches.reduce<RouteHandle>((currentHandle, match) => {
    if (isRouteHandle(match.handle)) {
      return { ...currentHandle, ...match.handle };
    }
    return currentHandle;
  }, {});
  const title = routeHandle.title ?? "PyroNet CSP";
  const isImmersive = routeHandle.immersive === true;

  return (
    <div className={`app-shell${isImmersive ? " app-shell-immersive" : ""}`}>
      <AppSidebar />
      <div className={`content-shell${isImmersive ? " content-shell-immersive" : ""}`}>
        {isImmersive ? null : <AppHeader title={title} />}
        <main className={`content-main${isImmersive ? " content-main-immersive" : ""}`}>
          <Outlet />
        </main>
      </div>
    </div>
  );
}
