import type { ReactNode } from "react";

interface PageContainerProps {
  title?: string;
  description?: string;
  actions?: ReactNode;
  children: ReactNode;
}

export function PageContainer({ title, description, actions, children }: PageContainerProps) {
  const hasHeading = Boolean(title || description || actions);

  return (
    <section className="page-container">
      {hasHeading ? (
        <div className="page-heading">
          <div>
            {title ? <h1>{title}</h1> : null}
            {description ? <p>{description}</p> : null}
          </div>
          {actions ? <div className="page-actions">{actions}</div> : null}
        </div>
      ) : null}
      {children}
    </section>
  );
}
