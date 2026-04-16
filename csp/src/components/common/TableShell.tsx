import type { ReactNode } from "react";

interface TableHeaderCell {
  key: string;
  content: ReactNode;
  ariaSort?: "none" | "ascending" | "descending";
}

interface TableShellProps {
  columns: string[];
  children: ReactNode;
  className?: string;
  headerCells?: TableHeaderCell[];
}

export function TableShell({ columns, children, className, headerCells }: TableShellProps) {
  return (
    <div className={className ? `table-shell ${className}` : "table-shell"}>
      <table>
        <thead>
          <tr>
            {headerCells
              ? headerCells.map((cell) => (
                  <th key={cell.key} aria-sort={cell.ariaSort}>
                    {cell.content}
                  </th>
                ))
              : columns.map((column) => <th key={column}>{column}</th>)}
          </tr>
        </thead>
        <tbody>{children}</tbody>
      </table>
    </div>
  );
}
