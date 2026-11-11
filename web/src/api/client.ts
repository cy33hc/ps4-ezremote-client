/** Thin typed wrapper around the console's /__local__ HTTP API. */

export interface DirEntry {
  name: string;
  rights: string;
  date: string;
  size: string;
  type: "dir" | "file";
}

export interface InstallUrlOptions {
  url: string;
  use_alldebrid: boolean;
  use_realdebrid: boolean;
  use_disk_cache: boolean;
  enable_rpi: boolean;
}

export interface DownloadUrlOptions {
  url: string;
  dest: string;
  use_alldebrid: boolean;
  use_realdebrid: boolean;
}

export class ApiError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "ApiError";
  }
}

async function request<T>(url: string, body?: unknown): Promise<T> {
  let response: Response;
  try {
    response = await fetch(url, {
      method: body === undefined ? "GET" : "POST",
      headers: body === undefined ? undefined : { "Content-Type": "application/json" },
      body: body === undefined ? undefined : JSON.stringify(body),
    });
  } catch {
    throw new ApiError("Cannot reach the console");
  }

  if (response.status === 401) {
    // Session expired or revoked: let the app show the login form. Reloading
    // here would loop, because mounted effects retry on every load.
    window.dispatchEvent(new Event("ezremote:unauthorized"));
    throw new ApiError("Session expired");
  }

  if (!response.ok) {
    throw new ApiError(`Request failed (HTTP ${response.status})`);
  }

  let json: unknown;
  try {
    json = await response.json();
  } catch {
    throw new ApiError("The console returned a malformed response");
  }

  const result = (json as { result?: unknown }).result;
  if (result !== null && typeof result === "object" && !Array.isArray(result)) {
    const envelope = result as { success?: boolean; error?: string | null };
    if (envelope.success === false) {
      throw new ApiError(envelope.error || "Operation failed");
    }
  }

  return json as T;
}

export async function list(path: string): Promise<DirEntry[]> {
  const data = await request<{ result: DirEntry[] }>("/__local__/list", {
    path,
    onlyFolders: "false",
  });
  return Array.isArray(data.result) ? data.result : [];
}

export async function createFolder(newPath: string): Promise<void> {
  await request("/__local__/createFolder", { newPath });
}

export async function rename(item: string, newItemPath: string): Promise<void> {
  await request("/__local__/rename", { item, newItemPath });
}

export async function remove(items: string[]): Promise<void> {
  await request("/__local__/remove", { items });
}

export interface CopyConflict {
  src: string;
  dest: string;
}

export interface CopyOptions {
  /** Target name for a single item, chosen by the caller. */
  singleFilename?: string;
  /** Copy over existing targets instead of reporting them. */
  overwrite?: boolean;
  /** Report conflicts without copying anything. */
  probe?: boolean;
  /** Source paths to leave alone. */
  skip?: string[];
}

export async function copy(items: string[], newPath: string, options: CopyOptions = {}): Promise<CopyConflict[]> {
  const data = await request<{ conflicts?: CopyConflict[] }>("/__local__/copy", { items, newPath, ...options });
  return Array.isArray(data.conflicts) ? data.conflicts : [];
}

export async function move(items: string[], newPath: string, options: CopyOptions = {}): Promise<CopyConflict[]> {
  const data = await request<{ conflicts?: CopyConflict[] }>("/__local__/move", { items, newPath, ...options });
  return Array.isArray(data.conflicts) ? data.conflicts : [];
}

export async function install(items: string[]): Promise<void> {
  await request("/__local__/install", { items });
}

export async function extract(item: string, destination: string, folderName: string): Promise<void> {
  await request("/__local__/extract", { item, destination, folderName });
}

/** Extracts an archive that lives on the remote server into a local folder. */
export async function extractRemote(site: string, item: string, destination: string, folderName: string): Promise<void> {
  await request("/__remote__/extract", { site, item, destination, folderName });
}

export async function chmod(path: string, rights: string): Promise<void> {
  await request("/__local__/chmod", { path, rights });
}

export async function createLocalFile(path: string): Promise<void> {
  await request("/__local__/newfile", { path });
}

export async function compress(items: string[], destination: string, compressedFilename: string): Promise<void> {
  await request("/__local__/compress", { items, destination, compressedFilename });
}

/** Returns the raw text content of a local file. */
export async function getContent(item: string): Promise<string> {
  const data = await request<{ result: string }>("/__local__/getContent", { item });
  return typeof data.result === "string" ? data.result : "";
}

export async function edit(item: string, content: string): Promise<void> {
  await request("/__local__/edit", { item, content });
}

export async function installFromUrl(options: InstallUrlOptions): Promise<void> {
  await request("/__local__/install_url", options);
}

export async function downloadFromUrl(options: DownloadUrlOptions): Promise<void> {
  await request("/__local__/download_url", options);
}

export interface ServerProfile {
  name: string;
  server: string;
  username: string;
  hasPassword: boolean;
  type: number;
  httpServerType: string;
  defaultDirectory: string;
  enableRpi: boolean;
  enableDiskCache: boolean;
  /** RemoteClient::SupportedActions() bitmask for this profile. */
  supportedActions: number;
}

export interface ServerList {
  active: string;
  sites: ServerProfile[];
  clientTypes: string[];
  httpServerTypes: string[];
}

export interface ServerInput {
  name: string;
  server: string;
  username: string;
  /** Omitted entirely to keep the stored password. */
  password?: string;
  httpServerType: string;
  defaultDirectory: string;
  enableRpi: boolean;
  enableDiskCache: boolean;
}

export async function listServers(): Promise<ServerList> {
  return request<ServerList>("/__local__/servers");
}

export async function saveServer(input: ServerInput): Promise<void> {
  await request("/__local__/server", input);
}

export async function selectServer(name: string): Promise<void> {
  await request("/__local__/server/select", { name });
}

export interface RemoteList {
  site: string;
  path: string;
  entries: DirEntry[];
}

/** Lists a directory on the active (or named) remote site, connecting on demand. */
export async function listRemote(site: string, path: string): Promise<RemoteList> {
  // /__remote__/list answers with {site, path, entries} at the top level, unlike
  // the other endpoints; accept either shape and always return a usable object.
  const data = await request<RemoteList & { result?: RemoteList }>("/__remote__/list", { site, path });
  const payload = data.result ?? data;
  return {
    site: payload.site ?? site,
    path: payload.path ?? path,
    entries: Array.isArray(payload.entries) ? payload.entries : [],
  };
}

export async function disconnectRemote(site: string): Promise<void> {
  await request("/__remote__/disconnect", { site });
}

export function remoteDownloadUrl(site: string, path: string): string {
  return `/__remote__/download?site=${encodeURIComponent(site)}&path=${encodeURIComponent(path)}`;
}

export async function remoteMkdir(site: string, path: string): Promise<void> {
  await request("/__remote__/mkdir", { site, path });
}

export async function remoteRename(site: string, item: string, newItemPath: string): Promise<void> {
  await request("/__remote__/rename", { site, item, newItemPath });
}

export async function remoteRemove(site: string, files: string[], dirs: string[]): Promise<void> {
  await request("/__remote__/remove", { site, files, dirs });
}

export interface Favorite {
  index: number;
  url: string;
}

export async function listFavorites(): Promise<{ favorites: Favorite[]; capacity: number }> {
  return request<{ favorites: Favorite[]; capacity: number }>("/__local__/favorites");
}

export async function saveFavorite(url: string, index?: number): Promise<{ index: number }> {
  return request<{ index: number }>("/__local__/favorites/save", { url, index });
}

export async function removeFavorite(index: number): Promise<void> {
  await request("/__local__/favorites/remove", { index });
}

export interface Settings {
  language: string;
  languages: string[];
  autoDeleteTmpPkg: boolean;
  showHiddenFiles: boolean;
  tempFolder: string;
  enableBackgroundDownload: boolean;
  showActivityDialog: boolean;
  minimumBackgroundFileSize: number;
  webServerEnabled: boolean;
  httpServerPort: number;
  compressedFilePath: string;
  hasAllDebridKey: boolean;
  hasWebPassword: boolean;
  webSessionTimeout: number;
  hasRealDebridKey: boolean;
}

export type SettingsPatch = Partial<
  Omit<Settings, "languages" | "hasAllDebridKey" | "hasRealDebridKey" | "hasWebPassword">
> & {
  alldebridApiKey?: string;
  realdebridApiKey?: string;
  /** Empty string removes the password; omit to keep the stored one. */
  webPassword?: string;
};

export interface AuthStatus {
  required: boolean;
  authenticated: boolean;
  /** Console language, so the sign-in screen can be translated too. */
  language?: string;
}

/** Console language; this route is public so the sign-in screen can use it. */
export async function currentLanguage(): Promise<string> {
  try {
    const data = await request<{ language?: string }>("/__local__/lang");
    return typeof data.language === "string" ? data.language : "";
  } catch {
    return "";
  }
}

export async function authStatus(): Promise<AuthStatus> {
  return request<AuthStatus>("/__local__/auth");
}

export async function login(password: string): Promise<void> {
  await request("/__local__/login", { password });
}

export async function logout(): Promise<void> {
  await request("/__local__/logout", {});
}

export async function getSettings(): Promise<Settings> {
  return request<Settings>("/__local__/settings");
}

export async function saveSettings(patch: SettingsPatch): Promise<void> {
  await request("/__local__/settings", patch);
}

export interface PackageInfo {
  params: Record<string, string>;
  hasIcon: boolean;
}

/** Extracts SFO/param.json metadata from a local package. */
export async function pkgInfo(item: string): Promise<PackageInfo> {
  return request<PackageInfo>("/__local__/pkg_info", { item });
}

export interface TransferState {
  id: string;
  name: string;
  loaded: number;
  total: number;
  /** 0 running, 1 done, 2 failed, 3 cancelled */
  state: number;
  error: string;
}

export interface CopyItem {
  path: string;
  is_dir: boolean;
}

export interface CopyStart {
  /** Non-empty when the copy was not started. */
  id: string;
  /** Names that already exist at the destination. */
  conflicts: string[];
}

async function startCopy(
  url: string,
  site: string,
  items: CopyItem[],
  dest: string,
  overwrite: boolean,
  skip: string[],
): Promise<CopyStart> {
  const data = await request<{ id?: string; conflicts?: string[] }>(url, { site, items, dest, overwrite, skip });
  return { id: data.id ?? "", conflicts: data.conflicts ?? [] };
}

/** Starts a console-side copy to the remote server; returns the transfer id. */
export async function remoteCopyFromLocal(site: string, items: CopyItem[], dest: string, overwrite: boolean, skip: string[] = []): Promise<CopyStart> {
  return startCopy("/__remote__/copy_from_local", site, items, dest, overwrite, skip);
}

/** Starts a console-side copy from the remote server; returns the transfer id. */
export async function copyFromRemote(site: string, items: CopyItem[], dest: string, overwrite: boolean, skip: string[] = []): Promise<CopyStart> {
  return startCopy("/__local__/copy_from_remote", site, items, dest, overwrite, skip);
}

export interface BgDownload {
  path: string;
  loaded: number;
  size: number;
  /** 0 Pending, 1 Downloading, 2 Resumed, 3 Failed, 4 Success */
  state: number;
}

export async function bgDownloads(): Promise<BgDownload[]> {
  const data = await request<{ result: BgDownload[] }>("/__local__/bg_downloads");
  return Array.isArray(data.result) ? data.result : [];
}

/** Empty string when the ezRemote server is not reachable. */
export interface ServerStatus {
  version: string;
  /** Console language; the poll doubles as the language signal. */
  language: string;
}

export async function ezremoteStatus(): Promise<ServerStatus> {
  const data = await request<{ version?: string; language?: string }>("/__local__/ezremote_server/version");
  return {
    version: typeof data.version === "string" ? data.version : "",
    language: typeof data.language === "string" ? data.language : "",
  };
}

export async function ezremoteStop(): Promise<void> {
  await request("/__local__/ezremote_server/stop", {});
}

export async function ezremoteRestart(): Promise<void> {
  await request("/__local__/ezremote_server/restart", {});
}

export interface RemotePkgInfo {
  params: Record<string, string>;
  hasIcon: boolean;
}

export async function remotePkgInfo(site: string, path: string): Promise<RemotePkgInfo> {
  return request<RemotePkgInfo>("/__remote__/pkg_info", { site, path });
}

export async function installRemotePackage(site: string, path: string, size: number): Promise<void> {
  await request("/__remote__/install_pkg", { site, path, size });
}

export interface Activity {
  id: number;
  /** ACTIVITY_* from activity.h: 0 copy, 1 move, 2 delete, 3 compress, 4 extract, 5 upload, 6 download, 7 install. */
  kind: number;
  kindName: string;
  /** Running | Done | Failed | Cancelled */
  /** ACTIVITY_* state: 0 running, 1 done, 2 failed, 3 cancelled, 4 partial. */
  state: number;
  stateName: string;
  /** False while a phase cannot be interrupted (install hand-off). */
  cancellable: boolean;
  /** True once the user asked to stop; the worker has not noticed yet. */
  cancelRequested: boolean;
  /** Entries finished; itemsTotal is 0 when the worker does not track them. */
  itemsDone: number;
  itemsTotal: number;
  label: string;
  item: string;
  done: number;
  total: number;
  elapsed: number;
  /** "Native" or "Web" */
  /** ACTIVITY_NATIVE = 0, ACTIVITY_WEB = 1. */
  origin: number;
  originName: string;
}

/** Removes finished entries; returns how many were dropped. */
export async function clearActivities(): Promise<number> {
  const data = await request<{ removed?: number }>("/__local__/activities/clear", {});
  return data.removed ?? 0;
}

/** Asks the console to stop one running activity. */
export async function cancelActivity(id: number): Promise<void> {
  await request("/__local__/activities/cancel", { id });
}

export async function activities(): Promise<Activity[]> {
  const data = await request<Activity[]>("/__local__/activities");
  return Array.isArray(data) ? data : [];
}

export async function clearBgDownloads(): Promise<void> {
  await request("/__local__/bg_downloads/clear", {});
}

export async function remoteCopy(site: string, items: string[], dest: string): Promise<void> {
  await request("/__remote__/copy", { site, items, dest });
}

export async function remoteMove(site: string, items: string[], dest: string): Promise<void> {
  await request("/__remote__/move", { site, items, dest });
}

export async function remoteNewFile(site: string, path: string): Promise<void> {
  await request("/__remote__/newfile", { site, path });
}

export async function remoteGetContent(site: string, item: string): Promise<string> {
  const data = await request<{ result: string }>("/__remote__/getContent", { site, item });
  return typeof data.result === "string" ? data.result : "";
}

export async function remoteEdit(site: string, item: string, content: string): Promise<void> {
  await request("/__remote__/edit", { site, item, content });
}

export async function transferStatus(id: string): Promise<TransferState> {
  return request<TransferState>(`/__local__/transfer?id=${encodeURIComponent(id)}`);
}

export function downloadFileUrl(path: string): string {
  return `/__local__/downloadFile?path=${encodeURIComponent(path)}`;
}

export function downloadMultipleUrl(items: string[], toFilename: string): string {
  const query = items.map((item) => `items=${encodeURIComponent(item)}`).join("&");
  return `/__local__/downloadMultiple?${query}&toFilename=${encodeURIComponent(toFilename)}`;
}

export function triggerDownload(url: string): void {
  const anchor = document.createElement("a");
  anchor.href = url;
  anchor.rel = "noopener";
  document.body.appendChild(anchor);
  anchor.click();
  anchor.remove();
}
