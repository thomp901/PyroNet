import type { HTMLAttributes } from "react";

interface StatCardProps extends HTMLAttributes<HTMLElement> {
  label: string;
  value: string;
  helper?: string;
}

export function StatCard({ label, value, helper, className, ...props }: StatCardProps) {
  return (
    <article className={className ? `card stat-card ${className}` : "card stat-card"} {...props}>
      <span className="stat-label">{label}</span>
      <strong className="stat-value">{value}</strong>
      {helper ? <span className="stat-helper">{helper}</span> : null}
    </article>
  );
}
