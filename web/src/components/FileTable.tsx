import type { DirEntry } from "../api/client";
import { formatDate, formatSize, isArchive, isPackage } from "../lib/format";
import { t } from "../i18n";
import { Icon } from "./Icon";
import type { IconName } from "./Icon";

export type SortKey = "name" | "size" | "date";
export type SortDir = "asc" | "desc";

export interface Sort {
  key: SortKey;
  dir: SortDir;
}

interface FileTableProps {
  entries: DirEntry[];
  selected: Set<string>;
  loading: boolean;
  sort: Sort;
  onSort: (key: SortKey) => void;
  onToggle: (name: string, additive: boolean) => void;
  onToggleAll: () => void;
  onOpen: (entry: DirEntry) => void;
  onContext?: (entry: DirEntry | null, event: React.MouseEvent) => void;
  /** When set, rows become draggable and call this on drag start. */
  onRowDragStart?: (entry: DirEntry, event: React.DragEvent) => void;
}

function entryIcon(entry: DirEntry): IconName {
  if (entry.type === "dir") return "folder";
  if (isPackage(entry.name)) return "package";
  if (isArchive(entry.name)) return "archive";
  return "file";
}

function SortButton({
  label,
  column,
  sort,
  onSort,
  className,
}: {
  label: string;
  column: SortKey;
  sort: Sort;
  onSort: (key: SortKey) => void;
  className?: string;
}) {
  const active = sort.key === column;
  return (
    <button
      type="button"
      onClick={() => onSort(column)}
      className={`flex items-center gap-1 transition hover:text-mist-100 ${active ? "text-mist-100" : "text-mist-500"} ${className ?? ""}`}
    >
      {t(label)}
      {active ? <Icon name={sort.dir === "asc" ? "sortAsc" : "sortDesc"} size={13} /> : null}
    </button>
  );
}

export function FileTable({
  onRowDragStart, entries, selected, loading, sort, onSort, onToggle, onToggleAll, onOpen, onContext }: FileTableProps) {
  const allSelected = entries.length > 0 && entries.every((entry) => selected.has(entry.name));

  return (
    <div className="flex min-h-0 flex-1 flex-col overflow-hidden rounded-xl border border-abyss-600 bg-abyss-800">
      <div className="grid grid-cols-[2.25rem_1fr_5.5rem] items-center gap-2 border-b border-abyss-600 px-3 py-2 text-[11px] font-medium uppercase tracking-wider sm:grid-cols-[2.25rem_1fr_6.5rem_10rem]">
        <label className="flex items-center justify-center">
          <input
            type="checkbox"
            className="size-3.5 accent-accent-500"
            checked={allSelected}
            onChange={onToggleAll}
            aria-label="Select all"
          />
        </label>
        <SortButton label="Name" column="name" sort={sort} onSort={onSort} />
        <SortButton label="Size" column="size" sort={sort} onSort={onSort} className="justify-end" />
        <SortButton label="Modified" column="date" sort={sort} onSort={onSort} className="hidden sm:flex" />
      </div>

      <div
        className="min-h-0 flex-1 overflow-y-auto"
        onContextMenu={(event) => {
          if (!onContext) return;
          event.preventDefault();
          onContext(null, event);
        }}
      >
        {loading ? (
          <div className="flex h-full items-center justify-center gap-3 text-sm text-mist-500">
            <span className="size-4 animate-spin rounded-full border-2 border-mist-500/30 border-t-accent-400" />
            Loading…
          </div>
        ) : entries.length === 0 ? (
          <div className="flex h-full flex-col items-center justify-center gap-2 text-sm text-mist-500">
            <Icon name="folder" size={36} className="text-abyss-600" />
            <p>This folder is empty</p>
          </div>
        ) : (
          entries.map((entry) => {
            const isSelected = selected.has(entry.name);
            return (
              <div
                key={entry.name}
                role="row"
                tabIndex={0}
                onClick={(event) => onToggle(entry.name, event.ctrlKey || event.metaKey || event.shiftKey)}
                onDoubleClick={() => onOpen(entry)}
                onContextMenu={(event) => {
                  if (!onContext) return;
                  event.preventDefault();
                  event.stopPropagation();
                  onContext(entry, event);
                }}
                draggable={Boolean(onRowDragStart)}
                onDragStart={(event) => onRowDragStart?.(entry, event)}
                onKeyDown={(event) => {
                  if (event.key === "Enter") onOpen(entry);
                  if (event.key === " ") {
                    event.preventDefault();
                    onToggle(entry.name, true);
                  }
                }}
                className={`grid cursor-default grid-cols-[2.25rem_1fr_5.5rem] items-center gap-2 border-b border-abyss-600 px-3 py-2 text-sm transition outline-none sm:grid-cols-[2.25rem_1fr_6.5rem_10rem] ${
                  isSelected ? "bg-accent-500 text-white" : "text-mist-300 hover:bg-abyss-800"
                }`}
              >
                <span className="flex items-center justify-center" onClick={(event) => event.stopPropagation()}>
                  <input
                    type="checkbox"
                    className="size-3.5 accent-accent-500"
                    checked={isSelected}
                    onChange={(event) => onToggle(entry.name, event.nativeEvent instanceof MouseEvent && (event.nativeEvent.ctrlKey || event.nativeEvent.metaKey))}
                    aria-label={`Select ${entry.name}`}
                  />
                </span>
                <span className="flex min-w-0 items-center gap-2.5">
                  <Icon
                    name={entryIcon(entry)}
                    size={17}
                    className={entry.type === "dir" ? "shrink-0 text-accent-400" : "shrink-0 text-mist-500"}
                  />
                  <span className={`truncate ${entry.type === "dir" ? "font-medium text-mist-100" : ""}`} title={entry.name}>
                    {entry.name}
                  </span>
                </span>
                <span className="text-right font-mono text-xs text-mist-500">{entry.type === "dir" ? "—" : formatSize(entry.size)}</span>
                <span className="hidden font-mono text-xs text-mist-500 sm:block">{formatDate(entry.date)}</span>
              </div>
            );
          })
        )}
      </div>
    </div>
  );
}
