interface LoadingStateProps {
  label?: string;
}

export function LoadingState({ label = "Loading data..." }: LoadingStateProps) {
  return (
    <div className="state-card card">
      <div className="loading-dot" />
      <p>{label}</p>
    </div>
  );
}
