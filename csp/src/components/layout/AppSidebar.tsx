import { NavLink } from "react-router-dom";

const navItems = [
  { to: "/", label: "Dashboard", end: true },
  { to: "/map", label: "Map" },
  { to: "/nodes", label: "Nodes" },
  { to: "/alerts", label: "Alerts" },
  { to: "/history", label: "History" },
  { to: "/configuration", label: "Configuration" },
  { to: "/notifications", label: "Notifications" },
];

export function AppSidebar() {
  return (
    <aside className="sidebar">
      <div className="sidebar-brand">
        <span className="sidebar-logo">PyroNet</span>
        <strong className="sidebar-product">Centralized Software Platform</strong>
      </div>
      <nav className="sidebar-nav" aria-label="Primary">
        {navItems.map((item) => (
          <NavLink
            key={item.to}
            to={item.to}
            end={item.end}
            className={({ isActive }) => (isActive ? "nav-link active" : "nav-link")}
          >
            {item.label}
          </NavLink>
        ))}
      </nav>
    </aside>
  );
}
