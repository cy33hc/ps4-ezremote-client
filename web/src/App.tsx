import { useCallback, useEffect, useMemo, useReducer, useRef, useState } from "react";
import type { MouseEvent as ReactMouseEvent } from "react";

import * as api from "./api/client";
import type { DirEntry } from "./api/client";
import { EditorDialog } from "./components/EditorDialog";
import { FileTable } from "./components/FileTable";
import type { Sort, SortKey } from "./components/FileTable";
import { Icon } from "./components/Icon";
import type { IconName } from "./components/Icon";
import { ImageViewer } from "./components/ImageViewer";
import { Modal } from "./components/Modal";
import { PackageInfoDialog } from "./components/PackageInfoDialog";
import { PropertiesDialog } from "./components/PropertiesDialog";
import { ServersDialog } from "./components/ServersDialog";
import { SettingsDialog } from "./components/SettingsDialog";
import { cachedLanguage, getLanguage, loadLanguage, observeTranslations, t, tp } from "./i18n";
import { Toasts } from "./components/Toasts";
import type { Toast, ToastTone } from "./components/Toasts";
import { breadcrumbs, formatSize, isArchive, isPackage, joinPath } from "./lib/format";

const DEFAULT_PATH = "/";
const LAST_PATH_KEY = "ezrc:lastPath";
const TEXT_FILE = /\.(txt|log|ini|json|xml|html|conf|config|md|js|css|yml|yaml)$/i;
const IMAGE_FILE = /\.(bmp|jpg|jpeg|png|webp)$/i;

type Pane = "local" | "remote";

interface MenuItem {
  label?: string;
  icon?: IconName;
  onSelect?: () => void;
  disabled?: boolean;
  danger?: boolean;
  separator?: boolean;
}

interface MenuState {
  pane: Pane;
  entry: DirEntry | null;
  x: number;
  y: number;
}

interface PromptState {
  title: string;
  label: string;
  initial: string;
  confirmLabel: string;
  onSubmit: (value: string) => void;
}

interface ConfirmState {
  title: string;
  message: string;
  confirmLabel: string;
  onConfirm: () => void;
}

type UrlMode = "install" | "download";

function sortEntries(entries: DirEntry[], filter: string, sort: Sort): DirEntry[] {
  const needle = filter.trim().toLowerCase();
  const filtered = needle ? entries.filter((entry) => entry.name.toLowerCase().includes(needle)) : entries.slice();
  const direction = sort.dir === "asc" ? 1 : -1;
  filtered.sort((a, b) => {
    if (a.type !== b.type) return a.type === "dir" ? -1 : 1;
    if (sort.key === "size") return (Number(a.size || 0) - Number(b.size || 0)) * direction;
    if (sort.key === "date") return a.date.localeCompare(b.date) * direction;
    return a.name.localeCompare(b.name, undefined, { sensitivity: "base" }) * direction;
  });
  return filtered;
}

export default function App() {
  const [path, setPath] = useState(() => localStorage.getItem(LAST_PATH_KEY) || DEFAULT_PATH);
  const [entries, setEntries] = useState<DirEntry[]>([]);
  const [loading, setLoading] = useState(true);
  const [selection, setSelection] = useState<Set<string>>(new Set());
  const [sort, setSort] = useState<Sort>({ key: "name", dir: "asc" });
  const [filter, setFilter] = useState("");
  const [reloadKey, setReloadKey] = useState(0);

  const [remotePath, setRemotePath] = useState("");
  const [remoteSite, setRemoteSite] = useState<string | null>(null);
  const [remoteEntries, setRemoteEntries] = useState<DirEntry[]>([]);
  const [remoteSelection, setRemoteSelection] = useState<Set<string>>(new Set());
  const [remoteLoading, setRemoteLoading] = useState(false);
  const [remoteError, setRemoteError] = useState<string | null>(null);
  const [remoteSort, setRemoteSort] = useState<Sort>({ key: "name", dir: "asc" });
  const [remoteFilter, setRemoteFilter] = useState("");

  const [activePane, setActivePane] = useState<Pane>("local");
  const [localSelectMode, setLocalSelectMode] = useState(false);
  const [remoteSelectMode, setRemoteSelectMode] = useState(false);
  const [remoteEnabled, setRemoteEnabled] = useState(true);
  const [remoteClipboard, setRemoteClipboard] = useState<{ mode: "copy" | "cut"; items: string[] } | null>(null);
  const [busy, setBusy] = useState<string | null>(null);
  const [bgOpen, setBgOpen] = useState(false);
  const [activityList, setActivityList] = useState<api.Activity[]>([]);
  const [showActivityDialog, setShowActivityDialog] = useState(true);
  const [, refreshTranslations] = useReducer((value: number) => value + 1, 0);
  const lastActivityIdRef = useRef(0);
  const loadPreferences = useCallback(async () => {
    try {
      const loaded = await api.getSettings();
      setShowActivityDialog(loaded.showActivityDialog);
      loadLanguage(loaded.language);
      // Translations are module state; re-render the tree once they are in.
      refreshTranslations();
    } catch {
      // Keep the current value.
    }
  }, []);

  const [activityOpen, setActivityOpen] = useState(false);
  const [exitDialog, setExitDialog] = useState<"stop" | "restart" | null>(null);
  const [remotePkgView, setRemotePkgView] = useState<DirEntry | null>(null);
  const [auth, setAuth] = useState<api.AuthStatus | null>(null);
  // Touch devices use long-press for the context menu; a draggable row
  // swallows that gesture, so only enable dragging for precise pointers.
  const [dragEnabled, setDragEnabled] = useState(false);
  const authed = auth !== null && auth.authenticated;

  // Settings are gated: reading them before login fails, which would leave the
  // defaults in place for the whole session.
  useEffect(() => {
    if (!auth) return;
    let cancelled = false;

    // The language route is public, so the sign-in screen does not have to wait
    // for a session (or trust a cached value from a previous visit).
    void (async () => {
      const fetched = await api.currentLanguage();
      if (cancelled) return;

      const language = fetched || auth.language || cachedLanguage();
      if (language) {
        loadLanguage(language);
        refreshTranslations();
      }
      if (authed) void loadPreferences();
    })();

    return () => {
      cancelled = true;
    };
  }, [auth, authed, loadPreferences]);

  // React keeps English in its virtual tree, so the rendered DOM is translated
  // after each commit and re-translated when a dialog opens.
  useEffect(() => {
    if (!auth) return;
    return observeTranslations();
  }, [auth, refreshTranslations]);
  const [bgList, setBgList] = useState<api.BgDownload[]>([]);
  const [ezremoteVersion, setEzremoteVersion] = useState<string | null>(null);
  const [ezMenu, setEzMenu] = useState<{ x: number; y: number } | null>(null);
  const [powerMenu, setPowerMenu] = useState<{ x: number; y: number } | null>(null);
  const [queuedNotice, setQueuedNotice] = useState<string | null>(null);
  const knownBg = useRef<Set<string> | null>(null);
  const noticeTimer = useRef<number | null>(null);
  const [localAsk, setLocalAsk] = useState<{ name: string; index: number; total: number } | null>(null);
  const localAnswer = useRef<((choice: "skip" | "overwrite" | "always") => void) | null>(null);
  const [conflict, setConflict] = useState<{
    toRemote: boolean;
    items: api.CopyItem[];
    conflicts: string[];
    index: number;
    skipped: string[];
  } | null>(null);
  const activeBg = bgList.filter((item) => item.state !== 3 && item.state !== 4).length;
  const [menu, setMenu] = useState<MenuState | null>(null);
  const [clipboard, setClipboard] = useState<{ mode: "copy" | "cut"; items: string[] } | null>(null);
  const [toasts, setToasts] = useState<Toast[]>([]);
  const [promptState, setPromptState] = useState<PromptState | null>(null);
  const [confirmState, setConfirmState] = useState<ConfirmState | null>(null);
  const [urlMode, setUrlMode] = useState<UrlMode | null>(null);
  const [urlPrefill, setUrlPrefill] = useState("");
  const [serversOpen, setServersOpen] = useState(false);
  const [settingsOpen, setSettingsOpen] = useState(false);
  const [editor, setEditor] = useState<{ path: string; name: string; remote: boolean } | null>(null);
  const [properties, setProperties] = useState<DirEntry | null>(null);
  const [pkgView, setPkgView] = useState<{ path: string; name: string } | null>(null);
  const [viewer, setViewer] = useState<{ src: string; name: string } | null>(null);
  const [serverInfo, setServerInfo] = useState<api.ServerList | null>(null);

  const uploadRef = useRef<HTMLInputElement>(null);
  const toastId = useRef(0);

  const notify = useCallback((message: string, tone: ToastTone = "info") => {
    const id = ++toastId.current;
    setToasts((current) => [...current, { id, tone, message }]);
    window.setTimeout(() => setToasts((current) => current.filter((toast) => toast.id !== id)), 4500);
  }, []);

  const dismissToast = useCallback((id: number) => setToasts((current) => current.filter((toast) => toast.id !== id)), []);
  const refresh = useCallback(() => setReloadKey((key) => key + 1), []);

  const loadServerInfo = useCallback(async (): Promise<api.ServerList | null> => {
    try {
      const data = await api.listServers();
      setServerInfo(data);
      return data;
    } catch {
      return null;
    }
  }, []);

  const loadRemote = useCallback(
    async (target: string, siteOverride?: string) => {
      const activeSite = siteOverride ?? serverInfo?.active;
      if (!activeSite) return false;
      setRemoteLoading(true);
      setRemoteError(null);
      try {
        const data = await api.listRemote(activeSite, target);
        setRemoteEntries(data.entries);
        setRemotePath(data.path);
        setRemoteSite(activeSite);
        setRemoteSelection(new Set());
        return true;
      } catch (error) {
        setRemoteEntries([]);
        setRemoteError(error instanceof Error ? error.message : "Cannot reach this server");
        return false;
      } finally {
        setRemoteLoading(false);
      }
    },
    [serverInfo],
  );

  const resetRemotePane = useCallback(() => {
    setRemoteSite(null);
    setRemotePath("");
    setRemoteEntries([]);
    setRemoteSelection(new Set());
    setRemoteError(null);
  }, []);

  useEffect(() => {
    if (!authed) return;
    void loadServerInfo();
  }, [loadServerInfo, authed]);

  // Password protection: ask before any other API call is made.
  useEffect(() => {
    const query = window.matchMedia("(pointer: fine)");
    setDragEnabled(query.matches);

    const onChange = (event: MediaQueryListEvent) => setDragEnabled(event.matches);
    if (query.addEventListener) query.addEventListener("change", onChange);
    return () => {
      if (query.removeEventListener) query.removeEventListener("change", onChange);
    };
  }, []);

  useEffect(() => {
    void api
      .authStatus()
      .then(setAuth)
      .catch(() => setAuth({ required: false, authenticated: true }));
  }, []);

  useEffect(() => {
    const onUnauthorized = () => setAuth({ required: true, authenticated: false });
    window.addEventListener("ezremote:unauthorized", onUnauthorized);
    return () => window.removeEventListener("ezremote:unauthorized", onUnauthorized);
  }, []);

  // Background downloads live in the ezRemote server; poll while the panel is
  // open, and slowly otherwise so the header badge stays current.
  useEffect(() => {
    if (!authed) return;
    let cancelled = false;
    const load = async () => {
      // The status button follows /version alone.
      try {
        const status = await api.ezremoteStatus();
        if (!cancelled) {
          setEzremoteVersion(status.version);

          // The same poll carries the language, so a change made on the
          // console is applied without a reload.
          if (status.language && status.language !== getLanguage()) {
            loadLanguage(status.language);
            refreshTranslations();
          }
        }
      } catch {
        if (!cancelled) setEzremoteVersion("");
      }

      // Background workers (copy/move/delete/compress/…) report separately.
      try {
        const running = await api.activities();
        if (!cancelled) {
          setActivityList(running);
          const newest = running[running.length - 1];
          if (newest && newest.id !== lastActivityIdRef.current) {
            // Seed the baseline on first poll: existing history is not news.
            const firstPoll = lastActivityIdRef.current === 0;
            lastActivityIdRef.current = newest.id;

            // Native-initiated work is surfaced on the console only.
            if (!firstPoll && showActivityDialog && newest.origin !== 0) setActivityOpen(true);
          }
        }
      } catch {
        // Nothing to show.
      }

      // The queue listing is independent of the status probe.
      try {
        const list = await api.bgDownloads();
        if (cancelled) return;
        setBgList(list);

        // A restart reports an empty queue; ignoring it keeps the baseline so
        // the repopulated list is not mistaken for newly queued files.
        if (list.length === 0) return;

        const paths = new Set(list.map((item) => item.path));
        if (knownBg.current === null) {
          knownBg.current = paths;
        } else {
          const added = list.filter((item) => !knownBg.current!.has(item.path));
          knownBg.current = paths;

          if (added.length > 0) {
            const name = added[0].path.substring(added[0].path.lastIndexOf("/") + 1);
            setQueuedNotice(added.length === 1 ? `${name} queued` : `${added.length} files queued`);
            if (noticeTimer.current !== null) window.clearTimeout(noticeTimer.current);
            noticeTimer.current = window.setTimeout(() => setQueuedNotice(null), 6000);
          }
        }
      } catch {
        // Keep the last known list.
      }
    };

    void load();
    const timer = window.setInterval(load, activityOpen ? 1000 : bgOpen ? 1500 : 3000);
    return () => {
      cancelled = true;
      window.clearInterval(timer);
    };
  }, [bgOpen, activityOpen, authed, showActivityDialog]);

  // Connect the remote pane once a profile is known (and only once per failure).
  useEffect(() => {
    if (remoteEnabled && serverInfo && !remotePath && !remoteError && !remoteLoading) void loadRemote("");
  }, [remoteEnabled, serverInfo, remotePath, remoteError, remoteLoading, loadRemote]);

  // A different active site invalidates the remote pane: clear the old listing
  // and list the new site's default directory (empty path = server default).
  useEffect(() => {
    if (!serverInfo || remoteSite === null || remoteSite === serverInfo.active) return;
    setRemoteEntries([]);
    setRemoteSelection(new Set());
    void loadRemote("", serverInfo.active);
  }, [serverInfo, remoteSite, loadRemote]);

  useEffect(() => {
    // No session, no request: the sign-in screen is shown before this can
    // succeed anyway.
    if (!authed) return;

    let cancelled = false;
    setLoading(true);
    api
      .list(path)
      .then((data) => {
        if (cancelled) return;
        setEntries(data);
        setSelection(new Set());
        localStorage.setItem(LAST_PATH_KEY, path);
      })
      .catch((error: unknown) => {
        if (cancelled) return;
        setEntries([]);
        notify(error instanceof Error ? error.message : "Cannot read this folder", "error");
      })
      .finally(() => {
        if (!cancelled) setLoading(false);
      });
    return () => {
      cancelled = true;
    };
  }, [path, reloadKey, notify, authed]);

  const activeProfile = useMemo(
    () => serverInfo?.sites.find((site) => site.name === serverInfo.active),
    [serverInfo],
  );

  const localVisible = useMemo(() => sortEntries(entries, filter, sort), [entries, filter, sort]);
  const remoteVisible = useMemo(() => sortEntries(remoteEntries, remoteFilter, remoteSort), [remoteEntries, remoteFilter, remoteSort]);
  const localSelected = useMemo(() => entries.filter((entry) => selection.has(entry.name)), [entries, selection]);
  const remoteSelected = useMemo(() => remoteEntries.filter((entry) => remoteSelection.has(entry.name)), [remoteEntries, remoteSelection]);
  const localBytes = useMemo(() => localSelected.reduce((total, entry) => total + Number(entry.size || 0), 0), [localSelected]);
  const remoteBytes = useMemo(() => remoteSelected.reduce((total, entry) => total + Number(entry.size || 0), 0), [remoteSelected]);

  const toggle = (setter: (fn: (prev: Set<string>) => Set<string>) => void) => (name: string, additive: boolean) => {
    setter((current) => {
      if (additive) {
        const next = new Set(current);
        if (next.has(name)) next.delete(name);
        else next.add(name);
        return next;
      }
      return current.size === 1 && current.has(name) ? new Set() : new Set([name]);
    });
  };

  const toggleAll = (setter: (fn: (prev: Set<string>) => Set<string>) => void, visible: DirEntry[]) => () => {
    setter((current) =>
      visible.length > 0 && visible.every((entry) => current.has(entry.name)) ? new Set() : new Set(visible.map((entry) => entry.name)),
    );
  };

  const changeSort = (setter: (fn: (prev: Sort) => Sort) => void) => (key: SortKey) => {
    setter((current) => ({ key, dir: current.key === key && current.dir === "asc" ? "desc" : "asc" }));
  };

  const run = useCallback(
    async (label: string, action: () => Promise<void>) => {
      setBusy(label);
      try {
        await action();
        refresh();
      } catch (error) {
        notify(error instanceof Error ? error.message : "Operation failed", "error");
      } finally {
        setBusy(null);
      }
    },
    [notify, refresh],
  );

  const runRemote = useCallback(
    async (label: string, action: () => Promise<void>) => {
      setBusy(label);
      try {
        await action();
        await loadRemote(remotePath);
      } catch (error) {
        notify(error instanceof Error ? error.message : "Operation failed", "error");
      } finally {
        setBusy(null);
      }
    },
    [loadRemote, notify, remotePath],
  );

  /* ---------- local actions ---------- */

  const newFolder = () =>
    setPromptState({
      title: "New folder",
      label: "Folder name",
      initial: "",
      confirmLabel: "Create",
      onSubmit: (value) => {
        const raw = value.trim();
        if (!raw) return;

        // Never collide: "New Folder" -> "New Folder (2)".
        const taken = new Set(entries.map((entry) => entry.name.toLowerCase()));
        const name = taken.has(raw.toLowerCase()) ? uniqueCopyName(raw, taken) : raw;

        void run("Creating folder…", async () => {
          await api.createFolder(joinPath(path, name));
          notify(tp("Created {name}", { name }), "success");
          refresh();
        });
      },
    });

  const renameSelection = () => {
    if (localSelected.length !== 1) {
      notify("Select exactly one item to rename", "error");
      return;
    }
    const entry = localSelected[0];
    setPromptState({
      title: "Rename",
      label: "New name",
      initial: entry.name,
      confirmLabel: "Rename",
      onSubmit: (value) => {
        const name = value.trim();
        if (!name || name === entry.name) return;
        void run("Renaming…", async () => {
          await api.rename(joinPath(path, entry.name), joinPath(path, name));
          notify(tp("Renamed to {name}", { name }), "success");
        });
      },
    });
  };

  const deleteSelection = () => {
    if (localSelected.length === 0) return;
    setConfirmState({
      title: "Delete",
      message: `Permanently delete ${localSelected.length} item${localSelected.length > 1 ? "s" : ""}? This cannot be undone.`,
      confirmLabel: "Delete",
      onConfirm: () =>
        run("Deleting…", async () => {
          await api.remove(localSelected.map((entry) => joinPath(path, entry.name)));
          notify("Deleted", "success");
        }),
    });
  };

  const downloadSelection = () => {
    if (localSelected.length === 0) {
      notify("Select something to download", "error");
      return;
    }
    if (localSelected.length === 1) {
      api.triggerDownload(api.downloadFileUrl(joinPath(path, localSelected[0].name)));
      return;
    }
    api.triggerDownload(api.downloadMultipleUrl(localSelected.map((entry) => joinPath(path, entry.name)), "ezremote-client.zip"));
  };

  const installSelection = () => {
    const packages = localSelected
      .filter((entry) => entry.type === "dir" || isPackage(entry.name) || isArchive(entry.name))
      .map((entry) => joinPath(path, entry.name));
    if (packages.length === 0) {
      notify("Select a package, archive, or folder containing packages to install", "error");
      return;
    }
    void run(`Installing ${packages.length} package${packages.length > 1 ? "s" : ""}…`, async () => {
      await api.install(packages);
      notify("Install finished", "success");
    });
  };

  const extractSelection = () => {
    if (localSelected.length !== 1 || !isArchive(localSelected[0].name)) {
      notify("Select one .zip, .rar or .7z archive to extract", "error");
      return;
    }
    const entry = localSelected[0];
    setPromptState({
      title: "Extract archive",
      label: "Destination folder",
      initial: entry.name.replace(/\.[^.]+$/, ""),
      confirmLabel: "Extract",
      onSubmit: (value) => {
        const folder = value.trim();
        if (!folder) return;
        void run("Extracting…", async () => {
          await api.extract(joinPath(path, entry.name), path, folder);
          notify(tp("Extracted to {folder}", { folder }), "success");
        });
      },
    });
  };

  const newLocalFile = () => {
    setPromptState({
      title: "New file",
      label: "File name",
      initial: "",
      confirmLabel: "Create",
      onSubmit: (value) => {
        const raw = value.trim();
        if (!raw) return;

        // Never overwrite: "notes.txt" -> "notes (2).txt".
        const taken = new Set(entries.map((entry) => entry.name.toLowerCase()));
        const name = taken.has(raw.toLowerCase()) ? uniqueCopyName(raw, taken) : raw;

        void run("Creating file…", async () => {
          await api.createLocalFile(joinPath(path, name));
          notify(tp("Created {name}", { name }), "success");
          refresh();
        });
      },
    });
  };

  const compressSelection = () => {
    if (localSelected.length === 0) {
      notify("Select something to compress", "error");
      return;
    }

    // One file -> "name.zip", one folder -> "folder.zip", several -> "archive.zip".
    const archiveName =
      localSelected.length > 1
        ? "archive.zip"
        : localSelected[0].type === "dir"
          ? `${localSelected[0].name}.zip`
          : (() => {
              const dot = localSelected[0].name.lastIndexOf(".");
              const base = dot > 0 ? localSelected[0].name.slice(0, dot) : localSelected[0].name;
              return `${base}.zip`;
            })();

    // /data and the USB mounts keep the archive beside the source.
    const inData =
      path === "/data" || path.startsWith("/data/") || /^\/mnt\/usb\d+(?:\/|$)/.test(path);

    void (async () => {
      let folder = path;
      if (!inData) {
        try {
          folder = (await api.getSettings()).compressedFilePath || path;
        } catch {
          folder = path;
        }
      }

      // Never propose a name that already exists in the destination folder.
      const taken = new Set<string>();
      if (inData) {
        entries.forEach((entry) => taken.add(entry.name.toLowerCase()));
      } else {
        try {
          (await api.list(folder)).forEach((entry) => taken.add(entry.name.toLowerCase()));
        } catch {
          // Folder may not exist yet; the backend creates it.
        }
      }
      const uniqueName = taken.has(archiveName.toLowerCase()) ? uniqueCopyName(archiveName, taken) : archiveName;

      setPromptState({
        title: "Create archive",
        label: "Save as (full path)",
        initial: joinPath(folder, uniqueName),
        confirmLabel: "Compress",
        onSubmit: (value) => {
          const target = value.trim();
          const slash = target.lastIndexOf("/");
          const dir = slash > 0 ? target.slice(0, slash) : slash === 0 ? "/" : "";
          const file = slash >= 0 ? target.slice(slash + 1) : target;
          if (!file) {
            notify("Enter a file name", "error");
            return;
          }
          void run("Compressing…", async () => {
            await api.compress(localSelected.map((entry) => joinPath(path, entry.name)), dir, file);
            notify(tp("Created {name}", { name: file }), "success");
            refresh();
          });
        },
      });
    })();
  };

  const editSelection = () => {
    if (localSelected.length !== 1) {
      notify("Select one file to edit", "error");
      return;
    }
    const entry = localSelected[0];
    if (entry.type === "dir") {
      notify("A folder cannot be opened in the text editor", "error");
      return;
    }
    if (!TEXT_FILE.test(entry.name)) {
      notify("Only text files can be opened in the editor", "error");
      return;
    }
    setEditor({ path: joinPath(path, entry.name), name: entry.name, remote: false });
  };

  const openPackageInfo = () => {
    if (localSelected.length !== 1 || !isPackage(localSelected[0].name)) {
      notify("Select a .pkg file", "error");
      return;
    }
    setPkgView({ path: joinPath(path, localSelected[0].name), name: localSelected[0].name });
  };

  const copySelection = () => {
    if (localSelected.length === 0) return;
    setClipboard({ mode: "copy", items: localSelected.map((entry) => joinPath(path, entry.name)) });
    notify(tp(localSelected.length > 1 ? "Copied {n} items" : "Copied {n} item", { n: localSelected.length }), "info");
  };

  const cutSelection = () => {
    if (localSelected.length === 0) return;
    setClipboard({ mode: "cut", items: localSelected.map((entry) => joinPath(path, entry.name)) });
    notify(tp(localSelected.length > 1 ? "Cut {n} items" : "Cut {n} item", { n: localSelected.length }), "info");
  };

  const pasteClipboard = () => {
    if (!clipboard) return;
    const { mode, items } = clipboard;
    // Trailing slashes differ between listings, so compare normalised paths.
    const trimSlash = (value: string) => (value.length > 1 ? value.replace(/\/+$/, "") : value);
    const here = trimSlash(path);
    const sameDirectory = items.every(
      (item) => trimSlash(item.substring(0, item.lastIndexOf("/"))) === here,
    );

    // Cutting into the folder the files came from changes nothing.
    if (mode === "cut" && sameDirectory) {
      setClipboard(null);
      return;
    }

    let conflicted = false;
    void run(mode === "copy" ? "Copying…" : "Moving…", async () => {
      if (mode === "copy" && sameDirectory) {
        // Copying beside the originals always collides; mimic Explorer and
        // create "name (2)" rather than overwriting.
        const taken = new Set(entries.map((entry) => entry.name.toLowerCase()));
        for (const source of items) {
          const name = source.substring(source.lastIndexOf("/") + 1);
          // The original is always present here, so a copy must be renamed.
          taken.add(name.toLowerCase());
          const target = uniqueCopyName(name, taken);
          taken.add(target.toLowerCase());
          await api.copy([source], path, { singleFilename: target });
        }
      } else if (mode === "copy") {
        // Ask per file, then copy once with the skipped ones left out.
        const conflicts = await api.copy(items, path, { probe: true });
        const skipped: string[] = [];
        let always = false;

        if (conflicts.length > 0) {
          conflicted = true;
          for (let index = 0; index < conflicts.length && !always; index += 1) {
            const conflict = conflicts[index];
            const choice = await new Promise<"skip" | "overwrite" | "always">((resolve) => {
              localAnswer.current = resolve;
              setLocalAsk({ name: conflict.dest, index, total: conflicts.length });
            });
            // "always" keeps the rest of this transfer, not the next one.
            if (choice === "always") always = true;
            else if (choice === "skip") skipped.push(conflict.src);
          }
        }

        await api.copy(items, path, { overwrite: true, skip: skipped });
      } else {
        // Cut follows the same per-file questions as copy.
        const conflicts = await api.move(items, path, { probe: true });
        const skipped: string[] = [];
        let always = false;

        if (conflicts.length > 0) {
          conflicted = true;
          for (let index = 0; index < conflicts.length && !always; index += 1) {
            const conflict = conflicts[index];
            const choice = await new Promise<"skip" | "overwrite" | "always">((resolve) => {
              localAnswer.current = resolve;
              setLocalAsk({ name: conflict.dest, index, total: conflicts.length });
            });
            if (choice === "always") always = true;
            else if (choice === "skip") skipped.push(conflict.src);
          }
        }

        await api.move(items, path, { overwrite: true, skip: skipped });
      }
      if (mode === "cut") setClipboard(null);
      if (!conflicted)
        notify(tp(items.length > 1 ? "Pasted {n} items" : "Pasted {n} item", { n: items.length }), "success");
      refresh();
    });
  };

  const uploadFiles = async (files: FileList) => {
    const chunkSize = 4 * 1024 * 1024;
    let failed = false;
    let cancelledUpload = false;
    for (const file of Array.from(files)) {
      setBusy(`Uploading ${file.name}…`);
      try {
        const chunks = Math.max(1, Math.ceil(file.size / chunkSize));
        let cancelled = false;
        for (let index = 0; index < chunks; index += 1) {
          const start = index * chunkSize;
          const form = new FormData();
          form.append("destination", path);
          form.append("_chunkSize", String(chunkSize));
          form.append("_chunkNumber", String(index));
          form.append("_totalSize", String(file.size));
          form.append("file", file.slice(start, Math.min(start + chunkSize, file.size)), file.name);
          const response = await fetch("/__local__/upload", { method: "POST", body: form });
          if (!response.ok) throw new Error(`Upload failed (HTTP ${response.status})`);

          // The console drops the chunk once the activity is cancelled, so stop
          // sending the rest of this file (and the ones after it).
          const body = (await response.json().catch(() => null)) as { cancelled?: boolean } | null;
          if (body?.cancelled) {
            cancelled = true;
            break;
          }
        }
        if (cancelled) {
          cancelledUpload = true;
          break;
        }
        notify(tp("Uploaded {name}", { name: file.name }), "success");
      } catch (error) {
        failed = true;
        notify(error instanceof Error ? error.message : "Upload failed", "error");
      } finally {
        setBusy(null);
      }
    }
    if (!failed || cancelledUpload) refresh();
  };

  /* ---------- remote actions ---------- */

  const remoteNewFolder = () => {
    if (!serverInfo) return;
    setPromptState({
      title: "New remote folder",
      label: "Folder name",
      initial: "",
      confirmLabel: "Create",
      onSubmit: (value) => {
        const name = value.trim();
        if (!name) return;
        void runRemote("Creating folder…", () => api.remoteMkdir(serverInfo.active, joinPath(remotePath, name)));
      },
    });
  };

  const remoteRename = () => {
    if (!serverInfo) return;
    if (remoteSelected.length !== 1) {
      notify("Select exactly one item to rename", "error");
      return;
    }
    const entry = remoteSelected[0];
    setPromptState({
      title: "Rename on server",
      label: "New name",
      initial: entry.name,
      confirmLabel: "Rename",
      onSubmit: (value) => {
        const name = value.trim();
        if (!name || name === entry.name) return;
        void runRemote("Renaming…", () =>
          api.remoteRename(serverInfo.active, joinPath(remotePath, entry.name), joinPath(remotePath, name)),
        );
      },
    });
  };

  const remoteDelete = () => {
    if (!serverInfo) return;
    if (remoteSelected.length === 0) {
      notify("Select something to delete", "error");
      return;
    }
    setConfirmState({
      title: "Delete on server",
      message: `Permanently delete ${remoteSelected.length} remote item${remoteSelected.length > 1 ? "s" : ""}? This cannot be undone.`,
      confirmLabel: "Delete",
      onConfirm: () => {
        const files = remoteSelected.filter((entry) => entry.type !== "dir").map((entry) => joinPath(remotePath, entry.name));
        const dirs = remoteSelected.filter((entry) => entry.type === "dir").map((entry) => joinPath(remotePath, entry.name));
        void runRemote("Deleting…", () => api.remoteRemove(serverInfo.active, files, dirs));
      },
    });
  };

  const remoteNewFile = () => {
    if (!serverInfo) return;
    setPromptState({
      title: "New remote file",
      label: "File name",
      initial: "",
      confirmLabel: "Create",
      onSubmit: (value) => {
        const name = value.trim();
        if (!name) return;
        void runRemote("Creating file…", () => api.remoteNewFile(serverInfo.active, joinPath(remotePath, name)));
      },
    });
  };

  const remoteEditFile = () => {
    if (remoteSelected.length !== 1 || remoteSelected[0].type === "dir") {
      notify("Select one text file to edit", "error");
      return;
    }
    const entry = remoteSelected[0];
    if (!TEXT_FILE.test(entry.name)) {
      notify("Only text files can be opened in the editor", "error");
      return;
    }
    setEditor({ path: joinPath(remotePath, entry.name), name: entry.name, remote: true });
  };

  const copyRemoteItems = (mode: "copy" | "cut") => {
    if (remoteSelected.length === 0) return;
    setRemoteClipboard({ mode, items: remoteSelected.map((entry) => joinPath(remotePath, entry.name)) });
    notify(
      tp(
        `${mode === "copy" ? "Copied" : "Cut"} {n} item${remoteSelected.length > 1 ? "s" : ""}`,
        { n: remoteSelected.length },
      ),
      "info",
    );
  };

  const pasteRemoteItems = () => {
    if (!remoteClipboard || !serverInfo) return;
    const { mode, items } = remoteClipboard;
    void runRemote(mode === "copy" ? "Copying…" : "Moving…", async () => {
      if (mode === "copy") await api.remoteCopy(serverInfo.active, items, remotePath);
      else await api.remoteMove(serverInfo.active, items, remotePath);
      setRemoteClipboard(null);
      notify("Pasted", "success");
    });
  };



  const executeRestart = () => {
    void (async () => {
      try {
        await fetch("/stop");
      } catch {
        // Expected: the listener is torn down mid-response.
      }

      // The launcher lives on the ezRemote server; it relaunches the client and
      // redirects back to its web UI.
      const host = window.location.hostname;
      window.setTimeout(() => {
        window.location.href = `http://${host}:6701/start_client`;
      }, 2500);
    })();
  };

  const executeStop = () => {
    void (async () => {
      try {
        const response = await fetch("/stop");
        if (!response.ok) {
          notify("Could not stop the client (HTTP " + response.status + ")", "error");
          return;
        }
      } catch {
        // The listener is torn down mid-response; a network error means it worked.
      }
      notify("ezRemote Client stopped", "info");
    })();
  };

  const logout = () => {
    void (async () => {
      try {
        await api.logout();
      } catch {
        // Reloading below clears the dead session either way.
      }
      window.location.reload();
    })();
  };

  const disconnectRemote = async () => {
    if (!serverInfo) return;
    try {
      await api.disconnectRemote(serverInfo.active);
      setRemoteEnabled(false);
      resetRemotePane();
      notify("Disconnected", "info");
    } catch (error) {
      notify(error instanceof Error ? error.message : "Disconnect failed", "error");
    }
  };

  const followTransfer = useCallback(
    async (id: string) => {
      for (;;) {
        await new Promise((resolve) => setTimeout(resolve, 400));
        try {
          const state = await api.transferStatus(id);

          if (state.state === 0) continue;

          if (state.state === 1) {
            notify("Copied", "success");
          } else if (state.state === 3) {
            notify("Cancelled", "info");
          } else {
            notify(state.error || "Transfer failed", "error");
          }

          refresh();
          await loadRemote(remotePath);
          return;
        } catch (error) {
          notify(error instanceof Error ? error.message : "Transfer failed", "error");
          return;
        }
      }
    },
    [loadRemote, notify, refresh, remotePath],
  );

  const startCopy = useCallback(
    async (toRemote: boolean, items: api.CopyItem[], overwrite: boolean, skip: string[] = []) => {
      if (!serverInfo) return;

      const result = toRemote
        ? await api.remoteCopyFromLocal(serverInfo.active, items, remotePath, overwrite, skip)
        : await api.copyFromRemote(serverInfo.active, items, path, overwrite, skip);

      if (!result.id) {
        notify("Could not start the transfer", "error");
        return;
      }

      // Progress lives in the activities dialog; still follow for completion.
      void followTransfer(result.id);
    },
    [followTransfer, notify, path, remotePath, serverInfo],
  );

  const startCross = useCallback(
    async (toRemote: boolean, source?: DirEntry[]) => {
      if (!serverInfo) {
        notify("Select a server profile first", "error");
        return;
      }
      const chosen = source ?? (toRemote ? localSelected : remoteSelected);
      const items: api.CopyItem[] = toRemote
        ? chosen.map((entry) => ({ path: joinPath(path, entry.name), is_dir: entry.type === "dir" }))
        : chosen.map((entry) => ({ path: joinPath(remotePath, entry.name), is_dir: entry.type === "dir" }));

      if (items.length === 0) {
        notify("Select something to transfer", "error");
        return;
      }

      try {
        // Probe first: the server reports which destinations already exist.
        const probe = toRemote
          ? await api.remoteCopyFromLocal(serverInfo.active, items, remotePath, false)
          : await api.copyFromRemote(serverInfo.active, items, path, false);

        if (probe.conflicts.length > 0) {
          setConflict({ toRemote, items, conflicts: probe.conflicts, index: 0, skipped: [] });
          return;
        }

        if (!probe.id) {
          notify("Could not start the transfer", "error");
          return;
        }

        void followTransfer(probe.id);
      } catch (error) {
        notify(error instanceof Error ? error.message : "Transfer failed", "error");
      }
    },
    [followTransfer, localSelected, notify, path, remotePath, remoteSelected, serverInfo],
  );

  // Resolve one conflict; "always" covers the remaining files of this transfer only.
  const resolveConflict = useCallback(
    (choice: "skip" | "overwrite" | "always") => {
      if (!conflict) return;

      const { toRemote, items, conflicts, index, skipped } = conflict;
      const nextSkipped = choice === "skip" ? [...skipped, conflicts[index]] : skipped;
      const nextIndex = choice === "always" ? conflicts.length : index + 1;

      if (nextIndex < conflicts.length) {
        setConflict({ ...conflict, index: nextIndex, skipped: nextSkipped });
        return;
      }

      setConflict(null);

      void startCopy(toRemote, items, true, nextSkipped).catch((error: unknown) =>
        notify(error instanceof Error ? error.message : "Transfer failed", "error"),
      );
    },
    [conflict, notify, startCopy],
  );

  const localToRemote = () => void startCross(true);
  const remoteToLocal = (entries?: DirEntry[]) => void startCross(false, entries);

  /* ---------- menus ---------- */

  const openMenu = (pane: Pane, entry: DirEntry | null, event: ReactMouseEvent) => {
    event.preventDefault();
    setActivePane(pane);
    if (entry) {
      if (pane === "local" && !selection.has(entry.name)) setSelection(new Set([entry.name]));
      if (pane === "remote" && !remoteSelection.has(entry.name)) setRemoteSelection(new Set([entry.name]));
    }
    setMenu({ pane, entry, x: event.clientX, y: event.clientY });
  };

  const localMenu = (entry: DirEntry | null): MenuItem[] => {
    const items: MenuItem[] = [];
    if (entry) {
      if (entry.type === "dir") {
        items.push({ label: "Open", icon: "folder", onSelect: () => setPath(joinPath(path, entry.name)) });
        items.push({ label: "Install package", icon: "install", onSelect: installSelection });
        items.push({ label: "Upload to PS4", icon: "upload", onSelect: () => uploadRef.current?.click() });
        items.push({ label: "Install from URL", icon: "link", onSelect: () => { setUrlPrefill(""); setUrlMode("install"); } });
        items.push({ label: "Download from URL", icon: "link", onSelect: () => { setUrlPrefill(""); setUrlMode("download"); } });
      } else {
        items.push({ label: "Download from PS4", icon: "download", onSelect: downloadSelection });
        items.push({ label: "Upload to PS4", icon: "upload", onSelect: () => uploadRef.current?.click() });
        items.push({ label: "Install from URL", icon: "link", onSelect: () => { setUrlPrefill(""); setUrlMode("install"); } });
        items.push({ label: "Download from URL", icon: "link", onSelect: () => { setUrlPrefill(""); setUrlMode("download"); } });
        items.push({ separator: true });
        if (IMAGE_FILE.test(entry.name))
          items.push({ label: "Preview image", icon: "image", onSelect: () => setViewer({ src: api.downloadFileUrl(joinPath(path, entry.name)), name: entry.name }) });
        if (isPackage(entry.name) || isArchive(entry.name)) {
          items.push({ label: "Install Package", icon: "install", onSelect: installSelection });
          items.push({ label: "Package info", icon: "package", onSelect: openPackageInfo });
        }
        if (isArchive(entry.name)) items.push({ label: "Extract", icon: "archive", onSelect: extractSelection });
        if (TEXT_FILE.test(entry.name)) items.push({ label: "Edit", icon: "text", onSelect: editSelection });
      }
      items.push({ label: "Compress to zip", icon: "package", onSelect: compressSelection });
      items.push({ separator: true });
      items.push({ label: "Copy to remote", icon: "copy", onSelect: localToRemote, disabled: !serverInfo || !remotePath || !canRemote(RA.UPLOAD) });
      items.push({ separator: true });
      items.push({ label: "Cut", icon: "cut", onSelect: cutSelection });
      items.push({ label: "Copy", icon: "copy", onSelect: copySelection });
      if (clipboard) items.push({ label: "Paste", icon: "paste", onSelect: pasteClipboard });
      items.push({ separator: true });
      items.push({ label: "Rename", icon: "rename", onSelect: renameSelection });
      items.push({ label: "Delete", icon: "trash", danger: true, onSelect: deleteSelection });
      items.push({ label: "New folder", icon: "folderPlus", onSelect: newFolder });
      items.push({ label: "New file", icon: "file", onSelect: newLocalFile });
      items.push({ separator: true });
      items.push({ label: "Properties", icon: "info", onSelect: () => setProperties(localSelected[0] ?? entry) });
      return items;
    }

    items.push({ label: "New folder", icon: "folderPlus", onSelect: newFolder });
    items.push({ label: "New file", icon: "file", onSelect: newLocalFile });
    items.push({ label: "Upload to PS4", icon: "upload", onSelect: () => uploadRef.current?.click() });
    if (clipboard) items.push({ label: "Paste", icon: "paste", onSelect: pasteClipboard });
    items.push({ label: "Refresh", icon: "refresh", onSelect: refresh });
    items.push({ label: "Select all", icon: "selectAll", onSelect: toggleAll(setSelection, localVisible) });
    items.push({ separator: true });
    items.push({ label: "Install from URL", icon: "link", onSelect: () => { setUrlPrefill(""); setUrlMode("install"); } });
    items.push({ label: "Download from URL", icon: "link", onSelect: () => { setUrlPrefill(""); setUrlMode("download"); } });
    return items;
  };

  const RA = {
    CUT: 1,
    COPY: 2,
    PASTE: 4,
    DELETE: 8,
    RENAME: 16,
    NEW_FOLDER: 32,
    DOWNLOAD: 64,
    UPLOAD: 128,
    INSTALL: 256,
    EDIT: 512,
    NEW_FILE: 1024,
    EXTRACT: 2048,
  } as const;

  // Unknown capabilities (older backend) fall back to "all allowed".
  const canRemote = (flag: number) => ((activeProfile?.supportedActions ?? 0x1fff) & flag) !== 0;

  const openRemotePkgInfo = (entry: DirEntry) => {
    setRemotePkgView(entry);
  };

  const installRemotePackages = (entries: DirEntry[]) => {
    if (!serverInfo) return;
    const packageCount = entries.length;
    void runRemote(`Installing ${packageCount} package${packageCount > 1 ? "s" : ""}…`, async () => {
      let failed = 0;
      for (const entry of entries) {
        try {
          await api.installRemotePackage(serverInfo.active, joinPath(remotePath, entry.name), Number(entry.size) || 0);
        } catch (error) {
          failed += 1;
          notify(error instanceof Error ? `${entry.name}: ${error.message}` : `${entry.name} failed`, "error");
        }
      }
      if (failed === 0) {
        setRemotePkgView(null);
        notify(tp(packageCount > 1 ? "{n} packages installing" : "{n} package installing", { n: packageCount }), "success");
      }
      else if (failed < packageCount)
        notify(tp("{done} of {total} packages installing", { done: packageCount - failed, total: packageCount }), "info");
    });
  };

  const remoteExtract = (entry: DirEntry) => {
    if (!serverInfo) return;
    setPromptState({
      title: "Extract remote archive",
      label: "Destination folder (local)",
      initial: entry.name.replace(/\.[^.]+$/, ""),
      confirmLabel: "Extract",
      onSubmit: (value) => {
        const folder = value.trim();
        if (!folder) return;
        void run("Extracting…", async () => {
          await api.extractRemote(serverInfo.active, joinPath(remotePath, entry.name), path, folder);
          notify(tp("Extracted to {folder}", { folder }), "success");
        });
      },
    });
  };

  const remoteMenu = (entry: DirEntry | null): MenuItem[] => {
    const items: MenuItem[] = [];
    if (entry) {
      if (entry.type === "dir") {
        items.push({ label: "Open", icon: "folder", onSelect: () => void loadRemote(joinPath(remotePath, entry.name)) });
        items.push({
          label: "Install package",
          icon: "install",
          onSelect: () => installRemotePackages([entry]),
          disabled: !canRemote(RA.INSTALL),
        });
      } else {
        if (TEXT_FILE.test(entry.name))
          items.push({ label: "Edit", icon: "text", onSelect: remoteEditFile, disabled: !canRemote(RA.EDIT) });
        const targets =
          remoteSelected.length > 1 && remoteSelected.some((item) => item.name === entry.name) ? remoteSelected : [entry];
        const packages = targets.filter((item) => item.type !== "dir" && (item.name.toLowerCase().endsWith(".pkg") || isArchive(item.name)));
        if (packages.length > 0)
          items.push({
            label: packages.length > 1 ? `Install ${packages.length} packages` : "Install package",
            icon: "package",
            onSelect: () => installRemotePackages(packages),
            disabled: !canRemote(RA.INSTALL),
          });
        if (entry.name.toLowerCase().endsWith(".pkg"))
          items.push({ label: "Package info", icon: "info", onSelect: () => openRemotePkgInfo(entry), disabled: !serverInfo });
        if (isArchive(entry.name))
          items.push({
            label: "Extract to local folder",
            icon: "archive",
            onSelect: () => remoteExtract(entry),
            disabled: !serverInfo || !canRemote(RA.EXTRACT),
          });
        if (IMAGE_FILE.test(entry.name) && serverInfo)
          items.push({
            label: "Preview image",
            icon: "image",
            onSelect: () => setViewer({ src: api.remoteDownloadUrl(serverInfo.active, joinPath(remotePath, entry.name)), name: entry.name }),
          });
      }
      items.push({ separator: true });
      items.push({ label: "Copy to local", icon: "copy", onSelect: remoteToLocal, disabled: !serverInfo || !canRemote(RA.DOWNLOAD) });
      items.push({ separator: true });
      items.push({ label: "Cut", icon: "cut", onSelect: () => copyRemoteItems("cut"), disabled: !canRemote(RA.CUT) });
      items.push({ label: "Copy", icon: "copy", onSelect: () => copyRemoteItems("copy"), disabled: !canRemote(RA.COPY) });
      if (remoteClipboard)
        items.push({ label: "Paste", icon: "paste", onSelect: pasteRemoteItems, disabled: !canRemote(RA.PASTE) });
      items.push({ separator: true });
      items.push({ label: "Rename", icon: "rename", onSelect: remoteRename, disabled: !canRemote(RA.RENAME) });
      items.push({ label: "Delete", icon: "trash", danger: true, onSelect: remoteDelete, disabled: !canRemote(RA.DELETE) });
      items.push({ label: "Properties", icon: "info", onSelect: () => setProperties(remoteSelected[0] ?? entry) });
      return items;
    }

    items.push({ label: "New folder", icon: "folderPlus", onSelect: remoteNewFolder, disabled: !serverInfo || !canRemote(RA.NEW_FOLDER) });
    items.push({ label: "New file", icon: "file", onSelect: remoteNewFile, disabled: !serverInfo || !canRemote(RA.NEW_FILE) });
    if (remoteClipboard)
      items.push({ label: "Paste", icon: "paste", onSelect: pasteRemoteItems, disabled: !canRemote(RA.PASTE) });
    items.push({ label: "Refresh", icon: "refresh", onSelect: () => void loadRemote(remotePath), disabled: !serverInfo });
    items.push({ label: "Select all", icon: "selectAll", onSelect: toggleAll(setRemoteSelection, remoteVisible) });
    items.push({ separator: true });
    items.push({ label: "Disconnect", icon: "server", onSelect: () => void disconnectRemote(), disabled: !serverInfo });
    return items;
  };

  if (auth === null) {
    return <div className="grid h-full place-items-center bg-abyss-950 text-sm text-mist-500">Loading…</div>;
  }

  if (auth.required && !auth.authenticated) {
    return (
      <LoginScreen
        onSuccess={() => {
          setAuth({ required: true, authenticated: true });
          // The first loads ran before authentication and failed; redo them.
          void loadServerInfo();
          refresh();
        }}
      />
    );
  }

  return (
    <div className="flex h-full flex-col">
      <header className="border-b border-abyss-600 bg-abyss-900">
        <div className="flex flex-wrap items-center gap-x-3 gap-y-2 px-4 py-2.5 sm:flex-nowrap">
          <div className="flex items-center gap-2">
            <img
              src="/app0/assets/images/icon0.png"
              alt="ezRemote Client"
              className="size-10 shrink-0 rounded-xl shadow-lg shadow-black/30"
            />
            <div className="min-w-0 leading-tight">
              <p className="text-2xl font-semibold tracking-tight">ezRemote Client</p>
              <div className="flex min-w-0 items-center gap-2">
                <HeaderButton icon="server" label="Servers" onClick={() => setServersOpen(true)} />
                <p className="min-w-0 max-w-[60vw] truncate text-[13px] font-medium text-mist-300" title={activeProfile?.server ?? ""}>
                  {serverInfo
                    ? `${serverInfo.active} · ${serverInfo.clientTypes[activeProfile?.type ?? -1] ?? "unset"}${
                        activeProfile?.server ? ` · ${activeProfile.server}` : ""
                      }`
                    : "no profile"}
                </p>
              </div>
            </div>
          </div>
          <div className="ml-auto flex w-full flex-wrap items-center justify-end gap-1.5 sm:w-auto">
            <button
              type="button"
              onClick={(event) => setEzMenu({ x: event.clientX, y: event.clientY })}
              className={`flex items-center gap-1.5 rounded-full border px-2.5 py-1.5 text-xs font-medium transition hover:brightness-125 ${
                ezremoteVersion === null
                  ? "border-abyss-600 bg-abyss-800 text-mist-500"
                  : ezremoteVersion
                    ? "border-mint-400/25 bg-mint-400/10 text-mint-400"
                    : "border-rose-400/25 bg-rose-400/10 text-rose-400"
              }`}
              title="ezRemote server state"
            >
              <span className={`size-1.5 rounded-full ${ezremoteVersion ? "bg-mint-400" : ezremoteVersion === "" ? "bg-rose-400" : "bg-mist-500"}`} />
              <span className="hidden lg:inline">{ezremoteVersion ? `ezRemote Server v${ezremoteVersion}` : ezremoteVersion === "" ? "ezRemote Server offline" : "ezRemote Server …"}</span>
              <Icon name="chevronRight" size={13} className="rotate-90 opacity-70" />
            </button>
            {queuedNotice ? (
              <span className="max-w-[18rem] truncate rounded-full border border-accent-500/40 bg-accent-500/10 px-2.5 py-1 text-xs font-medium text-accent-200" title={queuedNotice}>
                {queuedNotice}
              </span>
            ) : null}
            <HeaderButton
              icon="activity"
              label="Activity"
              onClick={() => setActivityOpen(true)}
              badge={activityList.filter((item) => item.state === 0).length}
              busy={activityList.some((item) => item.state === 0)}
            />
            <HeaderButton
              icon="download"
              label="Downloads"
              onClick={() => setBgOpen(true)}
              badge={activeBg}
              busy={bgList.some((item) => item.state === 1 || item.state === 2)}
            />
            <HeaderButton icon="settings" label="Settings" onClick={() => setSettingsOpen(true)} />
            <HeaderButton
              icon="logout"
              onClick={(event) => setPowerMenu({ x: event.clientX, y: event.clientY })}
            />
          </div>
        </div>
      </header>

      <main className="grid min-h-0 flex-1 gap-2.5 overflow-y-auto p-2.5 lg:grid-cols-2 lg:overflow-visible">
        <Panel
          label="Local"
          path={path}
          entries={localVisible}
          selected={selection}
          loading={loading}
          sort={sort}
          filter={filter}
          active={activePane === "local"}
          onActivate={() => setActivePane("local")}
          onFilter={setFilter}
          onSort={changeSort(setSort)}
          onToggle={(name, additive) => toggle(setSelection)(name, additive || localSelectMode)}
          onToggleAll={toggleAll(setSelection, localVisible)}
          selectMode={localSelectMode}
          onToggleSelectMode={() => setLocalSelectMode((value) => !value)}
          onOpen={(entry) => (entry.type === "dir" ? setPath(joinPath(path, entry.name)) : undefined)}
          onContext={(entry, event) => openMenu("local", entry, event)}
          onRowDragStart={dragEnabled ? (entry, event) => {
            const dragged =
              selection.has(entry.name) && selection.size > 1
                ? localVisible.filter((item) => selection.has(item.name))
                : [entry];
            event.dataTransfer.effectAllowed = "copy";
            event.dataTransfer.setData(
              "application/x-ezremote",
              JSON.stringify({ pane: "local", names: dragged.map((item) => item.name) }),
            );
          } : undefined}
          onNavigate={setPath}
          onRefresh={refresh}
          onDropItems={(event) => {
            event.preventDefault();
            const raw = event.dataTransfer.getData("application/x-ezremote");
            if (!raw) return;
            try {
              const payload = JSON.parse(raw) as { pane?: string; names?: string[] };
              if (payload.pane !== "remote" || !payload.names?.length) return;
              const dropped = remoteEntries.filter((entry) => payload.names!.includes(entry.name));
              if (dropped.length > 0) void startCross(false, dropped);
            } catch {
              // Ignore foreign drag payloads.
            }
          }}
        />
        <Panel
          label="Remote"
          path={remotePath}
          entries={remoteVisible}
          selected={remoteSelection}
          loading={remoteLoading}
          error={remoteError}
          sort={remoteSort}
          filter={remoteFilter}
          active={activePane === "remote"}
          onActivate={() => setActivePane("remote")}
          onFilter={setRemoteFilter}
          onSort={changeSort(setRemoteSort)}
          onToggle={(name, additive) => toggle(setRemoteSelection)(name, additive || remoteSelectMode)}
          onToggleAll={toggleAll(setRemoteSelection, remoteVisible)}
          selectMode={remoteSelectMode}
          onToggleSelectMode={() => setRemoteSelectMode((value) => !value)}
          onOpen={(entry) => (entry.type === "dir" ? void loadRemote(joinPath(remotePath, entry.name)) : undefined)}
          onContext={(entry, event) => openMenu("remote", entry, event)}
          onRowDragStart={dragEnabled ? (entry, event) => {
            // Drag the selection when the grabbed row is part of it.
            const dragged =
              remoteSelection.has(entry.name) && remoteSelection.size > 1
                ? remoteEntries.filter((item) => remoteSelection.has(item.name))
                : [entry];
            event.dataTransfer.effectAllowed = "copy";
            event.dataTransfer.setData(
              "application/x-ezremote",
              JSON.stringify({ pane: "remote", names: dragged.map((item) => item.name) }),
            );
          } : undefined}
          onNavigate={(target) => void loadRemote(target)}
          onDropItems={(event) => {
            event.preventDefault();
            const raw = event.dataTransfer.getData("application/x-ezremote");
            if (!raw) return;
            try {
              const payload = JSON.parse(raw) as { pane?: string; names?: string[] };
              if (payload.pane !== "local" || !payload.names?.length) return;
              if (!serverInfo || !remotePath) {
                notify("Connect to a remote server first", "error");
                return;
              }
              const dropped = localVisible.filter((entry) => payload.names!.includes(entry.name));
              if (dropped.length > 0) void startCross(true, dropped);
            } catch {
              // Ignore foreign drag payloads.
            }
          }}
          onRefresh={() => { setRemoteEnabled(true); void loadRemote(remotePath); }}
          onRetry={() => { setRemoteEnabled(true); void loadRemote(""); }}
        />
      </main>



      <footer className="flex flex-wrap items-center gap-x-4 gap-y-1 border-t border-abyss-600 px-4 py-2 text-xs text-mist-500">
        <span className="font-medium text-mist-300">{activePane === "local" ? "Local" : "Remote"}</span>
        <span>
          {(() => {
            const count = activePane === "local" ? localSelected.length : remoteSelected.length;
            return tp(count === 1 ? "{n} selected" : "{n} selected", { n: count });
          })()}
        </span>
        <span>{formatSize(String(activePane === "local" ? localBytes : remoteBytes))}</span>
        <span className="truncate font-mono text-mist-500/70">{activePane === "local" ? path : remotePath || "not connected"}</span>
        {busy ? (
          <span className="ml-auto flex items-center gap-2 text-accent-300">
            <span className="size-3 animate-spin rounded-full border-2 border-accent-500/30 border-t-accent-400" />
            {busy}
          </span>
        ) : null}
      </footer>

      <input ref={uploadRef} type="file" multiple hidden onChange={(event) => { if (event.target.files?.length) void uploadFiles(event.target.files); event.target.value = ""; }} />

      {menu ? (
        <ContextMenu
          x={menu.x}
          y={menu.y}
          items={menu.pane === "local" ? localMenu(menu.entry) : remoteMenu(menu.entry)}
          onClose={() => setMenu(null)}
        />
      ) : null}

      {promptState ? <PromptDialog state={promptState} onClose={() => setPromptState(null)} /> : null}
      {confirmState ? <ConfirmDialog state={confirmState} onClose={() => setConfirmState(null)} /> : null}
      {urlMode ? (
        <UrlDialog
          mode={urlMode}
          initialUrl={urlPrefill}
          defaultDest={path}
          onClose={() => setUrlMode(null)}
          onSubmit={(values) => {
            // Fire and forget: the work runs as an activity, so the dialog can
            // close as soon as it is submitted.
            if (urlMode === "install") {
              void run("Installing…", async () => {
                await api.installFromUrl({
                  url: values.url,
                  use_alldebrid: values.alldebrid,
                  use_realdebrid: values.realdebrid,
                  use_disk_cache: values.diskCache,
                  enable_rpi: values.rpi,
                });
                notify("Install queued", "success");
              });
            } else {
              void run("Downloading…", async () => {
                await api.downloadFromUrl({
                  url: values.url,
                  dest: values.dest,
                  use_alldebrid: values.alldebrid,
                  use_realdebrid: values.realdebrid,
                });
                notify("Download queued", "success");
              });
            }
            return Promise.resolve();
          }}
        />
      ) : null}
      {editor ? (
        <EditorDialog
          item={editor.path}
          name={editor.name}
          onClose={() => setEditor(null)}
          onSaved={() => {
            refresh();
            if (editor.remote) void loadRemote(remotePath);
          }}
          load={editor.remote && serverInfo ? (item) => api.remoteGetContent(serverInfo.active, item) : undefined}
          save={editor.remote && serverInfo ? (item, content) => api.remoteEdit(serverInfo.active, item, content) : undefined}
        />
      ) : null}
      {properties ? <PropertiesDialog
        entry={properties}
        path={activePane === "local" ? path : remotePath}
        remote={activePane !== "local"}
        onClose={() => setProperties(null)}
        onRefresh={refresh}
      /> : null}
      {powerMenu ? (
        <>
          <div className="fixed inset-0 z-40" onMouseDown={() => setPowerMenu(null)} />
          <div
            className="fixed z-50 w-56 rounded-xl border border-abyss-600 bg-abyss-900/95 p-1 shadow-2xl backdrop-blur"
            style={{ left: Math.max(8, powerMenu.x - 190), top: powerMenu.y + 10 }}
          >
            <button
              type="button"
              onClick={() => {
                setPowerMenu(null);
                logout();
              }}
              className="flex w-full items-center gap-2 rounded-md px-2 py-1.5 text-left text-xs text-mist-300 hover:bg-white/5 hover:text-mist-100"
            >
              <Icon name="logout" size={14} />
              Logout
            </button>
            <button
              type="button"
              onClick={() => {
                setPowerMenu(null);
                setExitDialog("restart");
              }}
              className="flex w-full items-center gap-2 rounded-md px-2 py-1.5 text-left text-xs text-mist-300 hover:bg-white/5 hover:text-mist-100"
            >
              <Icon name="refresh" size={14} />
              Restart ezRemote Client
            </button>
            <button
              type="button"
              onClick={() => {
                setPowerMenu(null);
                setExitDialog("stop");
              }}
              className="flex w-full items-center gap-2 rounded-md px-2 py-1.5 text-left text-xs text-rose-400 hover:bg-rose-400/10"
            >
              <Icon name="power" size={14} />
              Stop ezRemote Client
            </button>
          </div>
        </>
      ) : null}
      {ezMenu ? (
        <>
          <div className="fixed inset-0 z-40" onMouseDown={() => setEzMenu(null)} />
          <div
            className="fixed z-50 w-56 rounded-xl border border-abyss-600 bg-abyss-900/95 p-1 shadow-2xl backdrop-blur"
            style={{ left: Math.max(8, ezMenu.x - 190), top: ezMenu.y + 10 }}
          >
            <button
              type="button"
              onClick={() => {
                setEzMenu(null);
                void api
                  .ezremoteRestart()
                  .then(() => notify("Restarting the ezRemote server", "info"))
                  .catch((error: unknown) => notify(error instanceof Error ? error.message : "Restart failed", "error"));
              }}
              className="flex w-full items-center gap-2 rounded-md px-2 py-1.5 text-left text-sm text-mist-300 hover:bg-white/5 hover:text-mist-100"
            >
              <Icon name="refresh" size={14} />
              Restart ezRemote server
            </button>
            <button
              type="button"
              onClick={() => {
                setEzMenu(null);
                void api
                  .ezremoteStop()
                  .then(() => notify("ezRemote server stopped", "info"))
                  .catch((error: unknown) => notify(error instanceof Error ? error.message : "Stop failed", "error"));
              }}
              className="flex w-full items-center gap-2 rounded-md px-2 py-1.5 text-left text-sm text-rose-400 hover:bg-rose-400/10"
            >
              <Icon name="close" size={14} />
              Stop ezRemote server
            </button>
          </div>
        </>
      ) : null}
      {activityOpen ? (
        <ActivitiesDialog
          activities={activityList}
          onCancel={(id) => {
            void (async () => {
              try {
                await api.cancelActivity(id);
                refresh();
              } catch (cause) {
                notify(cause instanceof Error ? cause.message : "Cannot cancel this activity", "error");
              }
            })();
          }}
          onClose={() => setActivityOpen(false)}
          onClear={() => {
            void (async () => {
              try {
                await api.clearActivities();
                setActivityList(await api.activities());
              } catch (error) {
                notify(error instanceof Error ? error.message : "Could not clear activities", "error");
              }
            })();
          }}
        />
      ) : null}
      {exitDialog ? (
        <ExitConfirmDialog
          mode={exitDialog}
          running={activityList.filter((item) => item.state === 0)}
          onCancel={() => setExitDialog(null)}
          onConfirm={() => {
            const mode = exitDialog;
            setExitDialog(null);
            if (mode === "stop") executeStop();
            else executeRestart();
          }}
        />
      ) : null}
      {remotePkgView && serverInfo ? (
        <PackageInfoDialog
          item={joinPath(remotePath, remotePkgView.name)}
          name={remotePkgView.name}
          remote={{ site: serverInfo.active }}
          onInstall={() => {
            const target = remotePkgView;
            setRemotePkgView(null);
            installRemotePackages([target]);
          }}
          onClose={() => setRemotePkgView(null)}
        />
      ) : null}
      {bgOpen ? (
        <BackgroundDownloadsDialog
          downloads={bgList}
          onClose={() => setBgOpen(false)}
          onClear={() =>
            void (async () => {
              try {
                await api.clearBgDownloads();
                setBgList(await api.bgDownloads());
                notify("Download history cleared", "success");
              } catch (error) {
                notify(error instanceof Error ? error.message : "Could not clear history", "error");
              }
            })()
          }
        />
      ) : null}
      {conflict ? (
        <ConflictDialog
          name={conflict.conflicts[conflict.index]}
          index={conflict.index}
          total={conflict.conflicts.length}
          onClose={() => setConflict(null)}
          onSkip={() => resolveConflict("skip")}
          onOverwrite={() => resolveConflict("overwrite")}
          onAlways={() => resolveConflict("always")}
        />
      ) : null}
      {localAsk ? (
        <ConflictDialog
          name={localAsk.name}
          index={localAsk.index}
          total={localAsk.total}
          onClose={() => {
            localAnswer.current?.("skip");
            localAnswer.current = null;
            setLocalAsk(null);
          }}
          onSkip={() => {
            localAnswer.current?.("skip");
            localAnswer.current = null;
            setLocalAsk(null);
          }}
          onOverwrite={() => {
            localAnswer.current?.("overwrite");
            localAnswer.current = null;
            setLocalAsk(null);
          }}
          onAlways={() => {
            localAnswer.current?.("always");
            localAnswer.current = null;
            setLocalAsk(null);
          }}
        />
      ) : null}
      {pkgView ? (
        <PackageInfoDialog
          item={pkgView.path}
          name={pkgView.name}
          onClose={() => setPkgView(null)}
          onInstall={() => {
            const target = pkgView.path;
            setPkgView(null);
            void run("Installing package…", async () => {
              await api.install([target]);
              notify("Install finished", "success");
            });
          }}
        />
      ) : null}
      {viewer ? <ImageViewer src={viewer.src} name={viewer.name} onClose={() => setViewer(null)} /> : null}
      {settingsOpen ? <SettingsDialog
          onClose={() => setSettingsOpen(false)}
          onSaved={() => {
            void loadServerInfo();
            void loadPreferences();
          }}
        /> : null}
      {serversOpen ? (
        <ServersDialog
          onClose={() => setServersOpen(false)}
          onChanged={() =>
            void (async () => {
              const updated = await loadServerInfo();
              if (!updated) return;
              setRemoteEnabled(true);

              // Editing the site in use invalidates its session: drop it so the
              // next request connects with the values that were just saved.
              try {
                await api.disconnectRemote(updated.active);
              } catch {
                // Not connected yet: nothing to drop.
              }

              setRemoteEntries([]);
              setRemoteSelection(new Set());
              setRemoteError(null);

              // Always start at the site's defaultDirectory: probing the old
              // folder can crash some clients when the path is not valid.
              await loadRemote("", updated.active);
              notify(tp("Reconnected to {site}", { site: updated.active }), "info");
            })()
          }
        />
      ) : null}
      <Toasts toasts={toasts} onDismiss={dismissToast} />
    </div>
  );
}

function HeaderButton({
  icon,
  label,
  onClick,
  badge,
  busy,
}: {
  icon: IconName;
  label?: string;
  onClick: (event: ReactMouseEvent<HTMLButtonElement>) => void;
  badge?: number;
  busy?: boolean;
}) {
  return (
    <button
      type="button"
      onClick={onClick}
      className="relative flex items-center gap-1.5 rounded-lg border border-abyss-600 px-2.5 py-1.5 text-xs font-medium text-mist-300 transition hover:border-white/20 hover:bg-white/5 hover:text-mist-100"
    >
      <Icon name={icon} size={18} className={busy ? "animate-pulse text-accent-200" : undefined} />
      {label ? <span className="hidden lg:inline">{t(label)}</span> : null}
      {badge ? (
        <span className="absolute -top-1.5 -right-1.5 min-w-4 rounded-full bg-accent-500 px-1 text-[10px] font-semibold leading-4 text-white">
          {badge}
        </span>
      ) : null}
    </button>
  );
}

function Panel({
  label,
  path,
  entries,
  selected,
  loading,
  error,
  sort,
  filter,
  active,
  onActivate,
  onFilter,
  onSort,
  onToggle,
  onToggleAll,
  onOpen,
  onContext,
  onRowDragStart,
  onDropItems,
  onNavigate,
  onRefresh,
  selectMode,
  onToggleSelectMode,
  onRetry,
}: {
  label: string;
  path: string;
  entries: DirEntry[];
  selected: Set<string>;
  loading: boolean;
  error?: string | null;
  sort: Sort;
  filter: string;
  active: boolean;
  onActivate: () => void;
  onFilter: (value: string) => void;
  onSort: (key: SortKey) => void;
  onToggle: (name: string, additive: boolean) => void;
  onRowDragStart?: (entry: DirEntry, event: React.DragEvent) => void;
  onDropItems?: (event: React.DragEvent) => void;
  onToggleAll: () => void;
  onOpen: (entry: DirEntry) => void;
  onContext: (entry: DirEntry | null, event: ReactMouseEvent) => void;
  onNavigate: (path: string) => void;
  onRefresh: () => void;
  selectMode?: boolean;
  onToggleSelectMode?: () => void;
  onRetry?: () => void;
}) {
  const crumbs = breadcrumbs(path);

  return (
    <section
      onMouseDown={onActivate}
      onDragOver={(event) => {
        if (onDropItems) event.preventDefault();
      }}
      onDrop={(event) => onDropItems?.(event)}
      className={`flex min-h-[18rem] flex-col gap-2 rounded-2xl border p-2.5 transition lg:min-h-0 ${
        active ? "border-accent-500 bg-abyss-900" : "border-abyss-600 bg-abyss-900"
      }`}
    >
      <div className="flex items-center gap-2">
        <span
          className={`rounded-md px-2 py-0.5 text-[10px] font-semibold uppercase tracking-wider ${
            active ? "bg-title text-white" : "bg-abyss-800 text-mist-500"
          }`}
        >
          {t(label)}
        </span>
        <nav className="flex min-w-0 flex-1 flex-wrap items-center gap-0.5 text-xs">
          {crumbs.length === 0 ? <span className="truncate text-mist-500">not connected</span> : null}
          {crumbs.map((crumb, index) => (
            <span key={crumb.path} className="flex items-center gap-0.5">
              {index > 0 ? <Icon name="chevronRight" size={12} className="text-mist-500/60" /> : null}
              <button
                type="button"
                onClick={() => onNavigate(crumb.path)}
                className={`rounded px-1 py-0.5 transition hover:bg-white/5 hover:text-mist-100 ${
                  index === crumbs.length - 1 ? "font-medium text-mist-100" : "text-mist-500"
                }`}
              >
                {index === 0 ? <Icon name="home" size={13} /> : crumb.label}
              </button>
            </span>
          ))}
        </nav>
        <span className="text-[11px] text-mist-500">{entries.length}</span>
        {onToggleSelectMode ? (
          <button
            type="button"
            onClick={onToggleSelectMode}
            title={selectMode ? "Stop selecting" : "Select multiple items"}
            aria-pressed={selectMode}
            className={`rounded-md p-1 transition ${
              selectMode ? "bg-accent-500/20 text-accent-200" : "text-mist-500 hover:bg-white/10 hover:text-mist-100"
            }`}
          >
            <Icon name="selectAll" size={14} />
          </button>
        ) : null}
        <button
          type="button"
          onClick={onRefresh}
          title={`${t("Refresh")} ${t(label)}`}
          aria-label={`${t("Refresh")} ${t(label)}`}
          className="rounded-md p-1 text-mist-500 transition hover:bg-white/10 hover:text-mist-100"
        >
          <Icon name="refresh" size={14} />
        </button>
      </div>

      <input
        value={filter}
        onChange={(event) => onFilter(event.target.value)}
        placeholder={t("Filter…")}
        className="w-full rounded-lg border border-abyss-600 bg-abyss-800 px-2.5 py-1 text-xs text-mist-100 placeholder:text-mist-500/70 focus:border-accent-500/60 focus:outline-none"
      />

      {error ? (
        <div className="flex items-center gap-2 rounded-lg border border-rose-400/30 bg-rose-400/10 px-2.5 py-1.5 text-[11px] text-rose-400">
          <Icon name="alert" size={14} className="shrink-0" />
          <span className="flex-1 truncate">{error}</span>
          {onRetry ? (
            <button type="button" onClick={onRetry} className="rounded border border-rose-400/40 px-2 py-0.5 font-medium hover:bg-rose-400/10">
              Connect
            </button>
          ) : null}
        </div>
      ) : null}

      <FileTable
        entries={entries}
        selected={selected}
        loading={loading}
        sort={sort}
        onSort={onSort}
        onToggle={onToggle}
        onToggleAll={onToggleAll}
        onOpen={onOpen}
        onContext={onContext}
        onRowDragStart={onRowDragStart}
      />
    </section>
  );
}

function ExitConfirmDialog({
  mode,
  running,
  onCancel,
  onConfirm,
}: {
  mode: "stop" | "restart";
  running: api.Activity[];
  onCancel: () => void;
  onConfirm: () => void;
}) {
  const stopping = mode === "stop";

  return (
    <Modal
      title={stopping ? "Stop ezRemote Client" : "Restart ezRemote Client"}
      subtitle={stopping ? "The web UI will disconnect and any transfer stops." : "The client restarts and this page reopens once it is back."}
      width="max-w-xl"
      onClose={onCancel}
      footer={
        <>
          <button type="button" onClick={onCancel} className="rounded-lg px-3 py-1.5 text-sm font-medium text-mist-300 hover:bg-white/5">
            Cancel
          </button>
          <button
            type="button"
            onClick={onConfirm}
            className={`rounded-lg px-3 py-1.5 text-sm font-semibold text-white transition ${
              stopping || running.length > 0 ? "bg-rose-500 hover:bg-rose-400" : "bg-accent-500 hover:bg-accent-600"
            }`}
          >
            {stopping ? "Stop client" : "Restart client"}
          </button>
        </>
      }
    >
      {running.length > 0 ? (
        <div className="rounded-lg border border-rose-400/40 bg-rose-400/10 p-3">
          <p className="flex items-center gap-2 text-sm font-semibold text-rose-400">
            <Icon name="alert" size={16} className="shrink-0" />
            {running.length} background {running.length === 1 ? "activity is" : "activities are"} still running
          </p>
          <p className="mt-1 text-sm text-rose-300/90">
            Interrupting {running.length === 1 ? "it" : "them"} may leave a partial file behind.
          </p>
          <ul className="mt-2 space-y-1">
            {running.slice(0, 5).map((item) => (
              <li key={item.id} className="truncate text-xs text-rose-200/80">
                {t(ACTIVITY_KINDS[item.kind] ?? item.kindName)} · {item.label}
                {item.item ? <span className="text-rose-200/50"> — {item.item}</span> : null}
              </li>
            ))}
            {running.length > 5 ? <li className="text-xs text-rose-200/60">…and {running.length - 5} more</li> : null}
          </ul>
        </div>
      ) : (
        <p className="text-sm text-mist-300">No background activity is running.</p>
      )}
    </Modal>
  );
}

// ACTIVITY_NATIVE = 0, ACTIVITY_WEB = 1 (source/activity.h).
const ACTIVITY_ORIGINS = ["Native", "Web"];

// Order must match ACTIVITY_* state enum in source/activity.h.
const ACTIVITY_STATES = ["Running", "Done", "Failed", "Cancelled", "Partial"];

// Order must match ACTIVITY_* in source/activity.h.
const ACTIVITY_KINDS = ["Copy", "Move", "Delete", "Compress", "Extract", "Upload", "Download", "Install"];

function ActivitiesDialog({
  activities,
  onClose,
  onClear,
  onCancel,
}: {
  activities: api.Activity[];
  onClose: () => void;
  onClear: () => void;
  onCancel: (id: number) => void;
}) {
  const rows = [...activities].reverse();
  const label: Record<string, string> = {
    Running: "text-amber-400",
    Done: "text-mint-400",
    Failed: "text-rose-400",
    Partial: "text-orange-400",
    Cancelled: "text-mist-500",
  };

  return (
    <Modal
      title="Background activity"
      subtitle={rows.length > 0 ? `${rows.length} item${rows.length > 1 ? "s" : ""}` : undefined}
      width="max-w-3xl"
      onClose={onClose}
      footer={
        <>
          <button
            type="button"
            onClick={onClear}
            className="mr-auto rounded-lg border border-white/10 px-3 py-1.5 text-sm font-medium text-mist-200 transition hover:bg-white/5"
          >
            Clear completed
          </button>
          <button type="button" onClick={onClose} className="rounded-lg bg-accent-500 px-3 py-1.5 text-sm font-semibold text-white transition hover:bg-accent-600">
            Close
          </button>
        </>
      }
    >
      {rows.length === 0 ? (
        <p className="py-6 text-center text-sm text-mist-500">Nothing has run yet.</p>
      ) : (
        <ul className="max-h-[60vh] space-y-2 overflow-y-auto">
          {rows.map((item) => (
            <li key={item.id} className="rounded-lg border border-abyss-600 bg-abyss-800 px-3 py-2">
              <div className="flex items-center gap-3">
                <span className="min-w-0 flex-1 truncate text-sm text-mist-100">
                  <span className="mr-1.5 rounded border border-abyss-600 px-1 py-px text-[10px] uppercase tracking-wide text-mist-500">
                    {t(ACTIVITY_ORIGINS[item.origin] ?? item.originName)}
                  </span>
                  <span className="text-mist-500">{t(ACTIVITY_KINDS[item.kind] ?? item.kindName)} · </span>
                  {item.label}
                </span>
                <span className={`shrink-0 text-xs ${label[ACTIVITY_STATES[item.state]] ?? "text-mist-500"}`}>
                  {t(ACTIVITY_STATES[item.state] ?? item.stateName)}
                  {item.state === 3 && item.itemsTotal > 0 ? ` ${item.itemsDone}/${item.itemsTotal}` : ""}
                </span>
                <span className="shrink-0 font-mono text-xs text-mist-500">{item.elapsed}s</span>
                {item.state === 0 && item.cancellable ? (
                  item.cancelRequested ? (
                    <span className="shrink-0 text-[11px] text-mist-500">{t("Cancelling…")}</span>
                  ) : (
                    <button
                      type="button"
                      onClick={() => onCancel(item.id)}
                      className="shrink-0 rounded-md border border-rose-400/40 px-2 py-0.5 text-[11px] font-medium text-rose-300 transition hover:bg-rose-400/10"
                    >
                      {t("Cancel")}
                    </button>
                  )
                ) : null}
              </div>
              {item.item ? (
                <p className="mt-1 truncate font-mono text-[11px] text-mist-500" title={item.item}>
                  {item.item}
                </p>
              ) : null}
              {item.state === 0 && item.total > 0 ? (
                <div className="mt-2 flex items-center gap-2">
                  <div className="h-1.5 flex-1 overflow-hidden rounded-full bg-abyss-700">
                    <div
                      className="h-full rounded-full bg-accent-500 transition-[width] duration-300"
                      style={{ width: `${Math.min(100, Math.round((item.done / item.total) * 100))}%` }}
                    />
                  </div>
                  <span className="shrink-0 font-mono text-[10px] text-mist-500">
                    {Math.min(100, Math.round((item.done / item.total) * 100))}%
                  </span>
                </div>
              ) : null}
            </li>
          ))}
        </ul>
      )}
    </Modal>
  );
}

function BackgroundDownloadsDialog({
  downloads,
  onClose,
  onClear,
}: {
  downloads: api.BgDownload[];
  onClose: () => void;
  onClear: () => void;
}) {
  const labels = ["Pending", "Downloading", "Resumed", "Failed", "Success"];

  return (
    <Modal
      title="Background downloads"
      subtitle={downloads.length > 0 ? `${downloads.length} item${downloads.length > 1 ? "s" : ""}` : undefined}
      width="max-w-3xl"
      panelClassName="min-h-[20rem] sm:min-h-0 [@media(max-height:520px)]:min-h-[30rem]"
      onClose={onClose}
      footer={
        <>
          <button
            type="button"
            onClick={onClear}
            className="mr-auto rounded-lg border border-white/10 px-3 py-1.5 text-sm font-medium text-mist-200 transition hover:bg-white/5"
          >
            Clear history
          </button>
          <button type="button" onClick={onClose} className="rounded-lg bg-accent-500 px-3 py-1.5 text-sm font-semibold text-white transition hover:bg-accent-600">
            Close
          </button>
        </>
      }
    >
      {downloads.length === 0 ? (
        <p className="py-6 text-center text-sm text-mist-500">No background downloads.</p>
      ) : (
        <ul className="max-h-[60vh] space-y-3 overflow-y-auto [@media(max-height:520px)]:max-h-none">
          {downloads.map((item, index) => {
            const pct = item.size > 0 ? Math.min(100, Math.round((item.loaded / item.size) * 100)) : 0;
            const failed = item.state === 3;
            const done = item.state === 4;
            return (
              <li key={`${item.path}-${index}`} className="rounded-lg border border-abyss-600 bg-abyss-800 p-3">
                <div className="flex items-center gap-3">
                  <span className="min-w-0 flex-1 break-all font-mono text-sm text-mist-100">
                    {item.path.substring(item.path.lastIndexOf("/") + 1)}
                  </span>
                  <span className={`shrink-0 text-xs ${failed ? "text-rose-400" : done ? "text-mint-400" : "text-mist-500"}`}>
                    {labels[item.state] ?? "Unknown"}
                  </span>
                </div>
                <div className="mt-2 flex items-center gap-3">
                  <span className="h-2.5 min-w-0 flex-1 overflow-hidden rounded-full bg-abyss-700">
                    <span
                      className={`block h-full rounded-full ${failed ? "bg-rose-400" : done ? "bg-mint-400" : "bg-accent-500"}`}
                      style={{ width: `${pct}%` }}
                    />
                  </span>
                  <span className="shrink-0 font-mono text-xs text-mist-300">
                    {pct}% · {formatSize(String(item.loaded))}
                    {item.size > 0 ? ` / ${formatSize(String(item.size))}` : ""}
                  </span>
                </div>
              </li>
            );
          })}
        </ul>
      )}
    </Modal>
  );
}

function ConflictDialog({
  name,
  index,
  total,
  onClose,
  onSkip,
  onOverwrite,
  onAlways,
}: {
  name: string;
  index: number;
  total: number;
  onClose: () => void;
  onSkip: () => void;
  onOverwrite: () => void;
  onAlways: () => void;
}) {
  return (
    <Modal
      title="File already exists"
      subtitle={total > 1 ? `File ${index + 1} of ${total} conflicts` : undefined}
      onClose={onClose}
      footer={
        <>
          <button type="button" onClick={onClose} className="rounded-lg px-3 py-1.5 text-sm font-medium text-mist-300 hover:bg-white/5">
            Cancel
          </button>
          <button
            type="button"
            onClick={onSkip}
            className="rounded-lg border border-white/10 px-3 py-1.5 text-sm font-medium text-mist-200 hover:bg-white/5"
          >
            Skip
          </button>
          <button
            type="button"
            onClick={onOverwrite}
            className="rounded-lg border border-white/10 px-3 py-1.5 text-sm font-medium text-mist-200 hover:bg-white/5"
          >
            Overwrite
          </button>
          <button
            type="button"
            onClick={onAlways}
            className="rounded-lg bg-accent-500 px-3 py-1.5 text-sm font-semibold text-white transition hover:bg-accent-600"
          >
            Always overwrite
          </button>
        </>
      }
    >
      <p className="mb-2 text-sm text-mist-500">This file already exists at the destination:</p>
      <p className="break-all rounded-lg border border-abyss-600 bg-abyss-800 p-2.5 font-mono text-sm text-mist-100">{name}</p>
      <p className="mt-2.5 text-xs leading-relaxed text-mist-500">
        “Always overwrite” applies to the remaining files of this transfer only — the next transfer asks again.
      </p>
    </Modal>
  );
}

function ContextMenu({ x, y, items, onClose }: { x: number; y: number; items: MenuItem[]; onClose: () => void }) {
  useEffect(() => {
    const close = () => onClose();
    const onKey = (event: KeyboardEvent) => {
      if (event.key === "Escape") onClose();
    };
    window.addEventListener("mousedown", close);
    window.addEventListener("keydown", onKey);
    return () => {
      window.removeEventListener("mousedown", close);
      window.removeEventListener("keydown", onKey);
    };
  }, [onClose]);

  const left = Math.min(x, window.innerWidth - 240);
  const top = Math.min(y, Math.max(8, window.innerHeight - 40 - items.length * 30));

  return (
    <div
      className="fixed z-50 w-56 overflow-y-auto rounded-xl border border-abyss-600 bg-abyss-900/95 p-1 shadow-2xl shadow-black/60 backdrop-blur"
      style={{ left, top, maxHeight: "70vh" }}
      onMouseDown={(event) => event.stopPropagation()}
      onContextMenu={(event) => event.preventDefault()}
    >
      {items.map((item, index) =>
        item.separator ? (
          <div key={index} className="my-1 h-px bg-white/5" />
        ) : (
          <button
            key={index}
            type="button"
            disabled={item.disabled}
            onClick={() => {
              onClose();
              item.onSelect?.();
            }}
            className={`flex w-full items-center gap-2 rounded-md px-2 py-1.5 text-left text-xs transition disabled:cursor-not-allowed disabled:opacity-40 ${
              item.danger ? "text-rose-400 hover:bg-rose-400/10" : "text-mist-300 hover:bg-white/5 hover:text-mist-100"
            }`}
          >
            {item.icon ? <Icon name={item.icon} size={14} /> : <span className="size-3.5" />}
            {t(item.label)}
          </button>
        ),
      )}
    </div>
  );
}

function LoginScreen({ onSuccess }: { onSuccess: () => void }) {
  const [password, setPassword] = useState("");
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);

  const submit = async () => {
    if (!password || busy) return;
    setBusy(true);
    setError(null);
    try {
      await api.login(password);
      onSuccess();
    } catch (cause) {
      setError(cause instanceof Error ? cause.message : "Cannot sign in");
      setPassword("");
    } finally {
      setBusy(false);
    }
  };

  return (
    <div className="grid h-full place-items-center bg-abyss-950 p-6">
      <div className="w-full max-w-sm rounded-2xl border border-abyss-600 bg-abyss-900 p-6 shadow-2xl shadow-black/40">
        <h1 className="text-xl font-semibold tracking-tight text-mist-100">ezRemote Client</h1>
        <p className="mt-1 text-sm text-mist-500">{t("This console is password protected.")}</p>
        <input
          autoFocus
          type="password"
          value={password}
          onChange={(event) => setPassword(event.target.value)}
          onKeyDown={(event) => {
            if (event.key === "Enter") void submit();
          }}
          placeholder={t("Password")}
          className="input mt-4"
        />
        {error ? <p className="mt-2 text-xs text-rose-400">{error}</p> : null}
        <button
          type="button"
          onClick={() => void submit()}
          disabled={!password || busy}
          className="mt-4 w-full rounded-lg bg-accent-500 px-3 py-2 text-sm font-semibold text-white transition hover:bg-accent-600 disabled:cursor-default disabled:opacity-40 disabled:hover:bg-accent-500"
        >
          {busy ? t("Signing in…") : t("Sign in")}
        </button>
      </div>
    </div>
  );
}

function PromptDialog({ state, onClose }: { state: PromptState; onClose: () => void }) {
  const [value, setValue] = useState(state.initial);
  const canSubmit = value.trim().length > 0;
  const submit = () => {
    if (!canSubmit) return;
    onClose();
    state.onSubmit(value);
  };

  return (
    <Modal
      title={state.title}
      onClose={onClose}
      footer={
        <>
          <button type="button" onClick={onClose} className="rounded-lg px-3 py-1.5 text-xs font-medium text-mist-300 hover:bg-white/5">
            Cancel
          </button>
          <button
            type="button"
            onClick={submit}
            disabled={!canSubmit}
            className="rounded-lg bg-accent-500 px-3 py-1.5 text-xs font-semibold text-white transition hover:bg-accent-600 disabled:cursor-default disabled:opacity-40 disabled:hover:bg-accent-500"
          >
            {state.confirmLabel}
          </button>
        </>
      }
    >
      <label className="block text-xs font-medium text-mist-500">{state.label}</label>
      <input
        autoFocus
        value={value}
        onChange={(event) => setValue(event.target.value)}
        onKeyDown={(event) => {
          if (event.key === "Enter") submit();
        }}
        className="input mt-1.5"
      />
    </Modal>
  );
}

function ConfirmDialog({ state, onClose }: { state: ConfirmState; onClose: () => void }) {
  return (
    <Modal
      title={state.title}
      onClose={onClose}
      footer={
        <>
          <button type="button" onClick={onClose} className="rounded-lg px-3 py-1.5 text-xs font-medium text-mist-300 hover:bg-white/5">
            Cancel
          </button>
          <button
            type="button"
            onClick={() => {
              onClose();
              state.onConfirm();
            }}
            className="rounded-lg bg-rose-500/90 px-3 py-1.5 text-xs font-semibold text-white transition hover:bg-rose-500"
          >
            {state.confirmLabel}
          </button>
        </>
      }
    >
      <p className="text-sm leading-relaxed text-mist-300">{state.message}</p>
    </Modal>
  );
}

interface UrlValues {
  url: string;
  dest: string;
  alldebrid: boolean;
  realdebrid: boolean;
  diskCache: boolean;
  rpi: boolean;
}

// Windows-style copy name: "file.txt" -> "file (2).txt", "folder" -> "folder (2)".
function uniqueCopyName(name: string, taken: Set<string>): string {
  const dot = name.lastIndexOf(".");
  const base = dot > 0 ? name.slice(0, dot) : name;
  const ext = dot > 0 ? name.slice(dot) : "";
  for (let index = 2; ; index += 1) {
    const candidate = `${base} (${index})${ext}`;
    // PS4 filesystems are case-insensitive, so compare lowercased.
    if (!taken.has(candidate.toLowerCase())) return candidate;
  }
}

// Best-effort filename from a URL: last path segment, query/hash stripped.
function urlFileName(url: string): string {
  try {
    const parsed = new URL(url);
    const segment = parsed.pathname.split("/").filter(Boolean).pop() ?? "";
    let name = segment;
    try {
      name = decodeURIComponent(segment);
    } catch {
      // Keep the raw segment when it is not valid percent-encoding.
    }
    if (name && name !== "." && name !== ".." && !name.includes("/")) return name;
  } catch {
    // Not a parseable URL; fall through to a generated name.
  }
  return `download-${Date.now()}`;
}

const URL_OPTIONS_KEY = "ezremote.urlOptions";

type DebridChoice = "none" | "alldebrid" | "realdebrid";

function loadUrlOptions(): { debrid: DebridChoice; rpi: boolean; diskCache: boolean } {
  try {
    const raw = window.localStorage.getItem(URL_OPTIONS_KEY);
    if (raw) {
      const parsed = JSON.parse(raw) as Partial<{
        alldebrid: boolean;
        realdebrid: boolean;
        rpi: boolean;
        diskCache: boolean;
      }>;
      const debrid: DebridChoice =
        parsed.alldebrid === true ? "alldebrid" : parsed.realdebrid === true ? "realdebrid" : "none";
      const rpi = parsed.rpi === true;
      // The disk cache is only meaningful with RPI, so a stale pair cannot
      // bring it back on its own.
      return { debrid, rpi, diskCache: rpi && parsed.diskCache === true };
    }
  } catch {
    // Ignore unreadable or malformed values.
  }

  // Nothing stored yet: the console keeps these separately from any site, in
  // install_pkg_url, where RPI starts on and everything else off. A site's
  // own RPI / disk-cache switches only affect installing from that site.
  return { debrid: "none", rpi: true, diskCache: false };
}

function UrlDialog({
  mode,
  initialUrl,
  defaultDest,
  onClose,
  onSubmit,
}: {
  mode: UrlMode;
  initialUrl: string;
  defaultDest: string;
  onClose: () => void;
  onSubmit: (values: UrlValues) => Promise<void>;
}) {
  const [url, setUrl] = useState(initialUrl);
  const [dest, setDest] = useState(defaultDest);
  const [fileName, setFileName] = useState(() => urlFileName(initialUrl));
  const [fileNameEdited, setFileNameEdited] = useState(false);
  // Only one file host can be used at a time; "none" keeps it a plain link.
  const [debrid, setDebrid] = useState<DebridChoice>(() => loadUrlOptions().debrid);
  const [rpi, setRpi] = useState(() => loadUrlOptions().rpi);
  const [diskCache, setDiskCache] = useState(() => loadUrlOptions().diskCache);
  const [working, setWorking] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const isInstall = mode === "install";

  // Keep the suggested name in step with the URL until the user edits it.
  useEffect(() => {
    if (!fileNameEdited) setFileName(urlFileName(url));
  }, [url, fileNameEdited]);

  useEffect(() => {
    try {
      window.localStorage.setItem(
        URL_OPTIONS_KEY,
        JSON.stringify({
          alldebrid: debrid === "alldebrid",
          realdebrid: debrid === "realdebrid",
          rpi,
          diskCache,
        }),
      );
    } catch {
      // Storage can be unavailable in private modes.
    }
  }, [debrid, rpi, diskCache]);

  const canSubmit = Boolean(url.trim()) && (isInstall || Boolean(fileName.trim()));

  const submit = async () => {
    if (!canSubmit || working) return;
    setWorking(true);
    setError(null);
    try {
      let target = dest.trim();
      if (!isInstall) {
        const folder = dest.trim() || defaultDest;
        let name = fileName.trim();
        try {
          // Rename rather than overwrite when the folder already has the name.
          const listing = await api.list(folder);
          const taken = new Set(listing.map((entry) => entry.name.toLowerCase()));
          if (taken.has(name.toLowerCase())) name = uniqueCopyName(name, taken);
        } catch {
          // Folder may not exist yet; nothing to collide with.
        }
        target = joinPath(folder, name);
      }

      await onSubmit({
        url: url.trim(),
        dest: target,
        alldebrid: debrid === "alldebrid",
        realdebrid: debrid === "realdebrid",
        diskCache,
        rpi,
      });
      onClose();
    } catch (cause) {
      setError(cause instanceof Error ? cause.message : "Request failed");
    } finally {
      setWorking(false);
    }
  };

  return (
    <Modal
      title={isInstall ? "Install from URL" : "Download from URL"}
      subtitle={isInstall ? "Queue a package hosted on a direct link or file host" : "Download a file to the current folder"}
      onClose={onClose}
      footer={
        <>
          <button type="button" onClick={onClose} className="rounded-lg px-3 py-1.5 text-xs font-medium text-mist-300 hover:bg-white/5">
            Cancel
          </button>
          <button
            type="button"
            onClick={() => void submit()}
            disabled={!canSubmit || working}
            className="rounded-lg bg-accent-500 px-3 py-1.5 text-xs font-semibold text-white transition hover:bg-accent-600 disabled:opacity-40"
          >
            {working ? "Working…" : isInstall ? "Install" : "Download"}
          </button>
        </>
      }
    >
      <div className="space-y-3">
        <div>
          <label className="block text-xs font-medium text-mist-500">URL</label>
          <input autoFocus value={url} onChange={(event) => setUrl(event.target.value)} placeholder="https://…" className="input mt-1.5" />
        </div>
        {!isInstall ? (
          <div className="grid gap-3 sm:grid-cols-2">
            <div>
              <label className="block text-xs font-medium text-mist-500">Destination folder</label>
              <input value={dest} onChange={(event) => setDest(event.target.value)} className="input mt-1.5 font-mono text-xs" />
            </div>
            <div>
              <label className="block text-xs font-medium text-mist-500">File name</label>
              <input
                value={fileName}
                onChange={(event) => {
                  setFileName(event.target.value);
                  setFileNameEdited(true);
                }}
                className="input mt-1.5 font-mono text-xs"
              />
            </div>
          </div>
        ) : null}
        <div className="flex flex-wrap gap-x-4 gap-y-2 pt-1">
          {/* One host at a time; clicking the active one clears it. */}
          <Radio
            label="AllDebrid"
            checked={debrid === "alldebrid"}
            onSelect={() => setDebrid(debrid === "alldebrid" ? "none" : "alldebrid")}
          />
          <Radio
            label="RealDebrid"
            checked={debrid === "realdebrid"}
            onSelect={() => setDebrid(debrid === "realdebrid" ? "none" : "realdebrid")}
          />
          {isInstall ? (
            <Checkbox
              label="RPI"
              checked={rpi}
              onChange={(value) => {
                setRpi(value);
                // Disk cache only applies to the streaming path RPI selects.
                if (!value) setDiskCache(false);
              }}
            />
          ) : null}
          {isInstall ? (
            // Only meaningful with RPI: the streaming path is chosen there.
            <Checkbox label="Disk cache" checked={diskCache} onChange={setDiskCache} disabled={!rpi} />
          ) : null}
        </div>
        {error ? (
          <p className="flex items-start gap-2 rounded-lg border border-rose-400/30 bg-rose-400/10 px-3 py-2 text-xs text-rose-400">
            <Icon name="alert" size={14} className="mt-px shrink-0" />
            {error}
          </p>
        ) : null}
      </div>
    </Modal>
  );
}

function Radio({ label, checked, onSelect }: { label: string; checked: boolean; onSelect: () => void }) {
  return (
    <label className="flex cursor-pointer items-center gap-2 text-xs text-mist-300">
      <input
        type="radio"
        checked={checked}
        // A click on the active radio clears it, so "none" stays reachable.
        onClick={onSelect}
        onChange={() => {}}
        className="size-3.5 accent-accent-500"
      />
      {label}
    </label>
  );
}

function Checkbox({
  label,
  checked,
  onChange,
  disabled = false,
}: {
  label: string;
  checked: boolean;
  onChange: (value: boolean) => void;
  disabled?: boolean;
}) {
  return (
    <label
      className={`flex items-center gap-2 text-xs ${
        disabled ? "cursor-default text-mist-500/60" : "cursor-pointer text-mist-300"
      }`}
    >
      <input
        type="checkbox"
        checked={checked}
        disabled={disabled}
        onChange={(event) => onChange(event.target.checked)}
        className="size-3.5 accent-accent-500 disabled:opacity-40"
      />
      {label}
    </label>
  );
}

