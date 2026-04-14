import type { ReactNode } from "react";

interface TableShellProps {
  columns: string[];
  children: ReactNode;
}

export function TableShell({ columns, children }: TableShellProps) {
  return (
    <div className="table-shell">
      <table>
        <thead>
          <tr>
            {columns.map((column) => (
              <th key={column}>{column}</th>
            ))}
          </tr>
        </thead>
        <tbody>{children}</tbody>
      </table>
    </div>
  );
}
