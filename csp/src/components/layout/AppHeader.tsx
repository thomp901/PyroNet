interface AppHeaderProps {
  title: string;
}

export function AppHeader({ title }: AppHeaderProps) {
  return (
    <header className="app-header">
      <div>
        <span className="header-kicker">Operations Console</span>
        <p className="app-header-title">{title}</p>
      </div>
    </header>
  );
}
