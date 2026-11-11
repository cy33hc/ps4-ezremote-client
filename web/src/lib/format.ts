const UNITS = ["B", "KB", "MB", "GB", "TB"] as const;

/** The console reports sizes as raw byte counts in a string. */
export function formatSize(size: string): string {
  if (!size) return "";
  const bytes = Number(size);
  if (!Number.isFinite(bytes)) return size;
  if (bytes < 1024) return `${bytes} B`;

  let value = bytes;
  let unit = 0;
  while (value >= 1024 && unit < UNITS.length - 1) {
    value /= 1024;
    unit += 1;
  }
  return `${value >= 100 ? value.toFixed(0) : value.toFixed(1)} ${UNITS[unit]}`;
}

/** "2025-06-14 00:57:00" -> "2025-06-14 00:57" */
export function formatDate(date: string): string {
  if (!date) return "";
  return date.length >= 16 ? date.slice(0, 16) : date;
}

export function parentPath(path: string): string {
  const trimmed = path.replace(/\/+$/, "");
  const index = trimmed.lastIndexOf("/");
  if (index <= 0) return "/";
  return trimmed.slice(0, index);
}

export function joinPath(base: string, name: string): string {
  if (base === "/" || base === "") return `/${name}`;
  return `${base.replace(/\/+$/, "")}/${name}`;
}

export interface Crumb {
  label: string;
  path: string;
}

export function breadcrumbs(path: string): Crumb[] {
  const parts = path.split("/").filter(Boolean);
  const crumbs: Crumb[] = [{ label: "/", path: "/" }];
  let current = "";
  for (const part of parts) {
    current += `/${part}`;
    crumbs.push({ label: part, path: current });
  }
  return crumbs;
}

export function isPackage(name: string): boolean {
  return /\.pkg$/i.test(name);
}

export function isArchive(name: string): boolean {
  return /\.(zip|rar|7z)$/i.test(name);
}

export function baseName(path: string): string {
  const parts = path.split("/").filter(Boolean);
  return parts.length > 0 ? parts[parts.length - 1] : path;
}
