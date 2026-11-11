import { useCallback, useEffect, useMemo, useState } from "react";
import { t } from "../i18n";
import type { ReactNode } from "react";

import * as api from "../api/client";
import type { ServerList, ServerProfile } from "../api/client";
import { Icon } from "./Icon";
import { Modal } from "./Modal";

interface ServersDialogProps {
  onClose: () => void;
  onChanged: () => void;
}

interface FormState {
  server: string;
  username: string;
  password: string;
  httpServerType: string;
  defaultDirectory: string;
  enableRpi: boolean;
  enableDiskCache: boolean;
}

function toForm(profile: ServerProfile): FormState {
  return {
    server: profile.server,
    username: profile.username,
    password: "",
    httpServerType: profile.httpServerType,
    defaultDirectory: profile.defaultDirectory || "/",
    enableRpi: profile.enableRpi,
    // A stored profile can predate the dependency; never show it enabled.
    enableDiskCache: profile.enableRpi && profile.enableDiskCache,
  };
}

export function ServersDialog({ onClose, onChanged }: ServersDialogProps) {
  const [list, setList] = useState<ServerList | null>(null);
  const [selected, setSelected] = useState<string>("");
  const [form, setForm] = useState<FormState | null>(null);
  const [loading, setLoading] = useState(true);
  const [busy, setBusy] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [showInfo, setShowInfo] = useState(false);

  const load = useCallback(async (keep?: string) => {
    const data = await api.listServers();
    setList(data);
    const next = keep && data.sites.some((site) => site.name === keep) ? keep : data.active || data.sites[0]?.name || "";
    setSelected(next);
    const profile = data.sites.find((site) => site.name === next);
    setForm(profile ? toForm(profile) : null);
  }, []);

  useEffect(() => {
    let cancelled = false;
    api
      .listServers()
      .then((data) => {
        if (cancelled) return;
        setList(data);
        const next = data.active || data.sites[0]?.name || "";
        setSelected(next);
        const profile = data.sites.find((site) => site.name === next);
        setForm(profile ? toForm(profile) : null);
      })
      .catch((cause: unknown) => {
        if (!cancelled) setError(cause instanceof Error ? cause.message : "Cannot load server profiles");
      })
      .finally(() => {
        if (!cancelled) setLoading(false);
      });
    return () => {
      cancelled = true;
    };
  }, []);

  const profile = useMemo(() => list?.sites.find((site) => site.name === selected), [list, selected]);
  const isHttp = profile?.type === 4;

  const pick = (site: ServerProfile) => {
    setSelected(site.name);
    setForm(toForm(site));
    setError(null);
  };

  const save = async () => {
    if (!form || !profile) return;
    setBusy("Saving…");
    setError(null);
    try {
      await api.saveServer({
        name: profile.name,
        server: form.server.trim(),
        username: form.username,
        password: form.password ? form.password : undefined,
        httpServerType: form.httpServerType,
        defaultDirectory: form.defaultDirectory.trim() || "/",
        enableRpi: form.enableRpi,
        enableDiskCache: form.enableRpi && form.enableDiskCache,
      });
      await load(profile.name);
      onChanged();
    } catch (cause) {
      setError(cause instanceof Error ? cause.message : "Cannot save this profile");
    } finally {
      setBusy(null);
    }
  };

  const activate = async () => {
    if (!profile) return;
    setBusy("Activating…");
    setError(null);
    try {
      await api.selectServer(profile.name);
      await load(profile.name);
      onChanged();
      // Selecting is a commit: drop the dialog so the panes are visible again
      // while the new site connects and lists its default directory.
      onClose();
    } catch (cause) {
      setError(cause instanceof Error ? cause.message : "Cannot select this profile");
    } finally {
      setBusy(null);
    }
  };

  return (
    <Modal
      title={t("Servers")}
      width="max-w-4xl"
      panelClassName="min-h-[22rem] sm:min-h-0 [@media(max-height:520px)]:min-h-[30rem]"
      onClose={onClose}
      footer={
        <>
          {busy ? <span className="mr-auto text-[11px] text-accent-300">{busy}</span> : null}
          <button type="button" onClick={onClose} className="rounded-lg px-3 py-1.5 text-xs font-medium text-mist-300 hover:bg-white/5">
            Close
          </button>
          <button
            type="button"
            onClick={() => void activate()}
            disabled={!profile || busy !== null || list?.active === profile.name}
            className="rounded-lg border border-abyss-600 px-3 py-1.5 text-xs font-medium text-mist-200 hover:bg-white/5 disabled:opacity-40"
          >
            Use this site
          </button>
          <button
            type="button"
            onClick={() => void save()}
            disabled={!form || busy !== null}
            className="rounded-lg bg-accent-500 px-3 py-1.5 text-xs font-semibold text-white transition hover:bg-accent-600 disabled:opacity-40"
          >
            Save
          </button>
        </>
      }
    >
      {loading ? (
        <p className="py-10 text-center text-sm text-mist-500">Loading…</p>
      ) : !list ? (
        <p className="py-10 text-center text-sm text-rose-400">{error ?? "No server profiles"}</p>
      ) : (
        <>
        <div className="mb-3 flex items-start gap-2 rounded-lg border border-abyss-600 bg-abyss-800 px-3 py-2">
          <p className={`min-w-0 flex-1 text-sm text-mist-300 ${showInfo ? "" : "truncate"}`}>
            {t("Profiles stored on the console. The active site is used for remote connections and installs.")}
          </p>
          <button
            type="button"
            onClick={() => setShowInfo((value) => !value)}
            aria-label={showInfo ? "Collapse description" : "Expand description"}
            title={showInfo ? "Collapse" : "Expand"}
            className="shrink-0 rounded-md p-0.5 text-mist-500 transition hover:bg-white/10 hover:text-mist-100"
          >
            <Icon name="chevronRight" size={16} className={showInfo ? "-rotate-90" : "rotate-90"} />
          </button>
        </div>

        <div className="grid gap-4 sm:grid-cols-[15rem_1fr]">
          <div className="max-h-20 space-y-1 overflow-y-auto pr-1 sm:max-h-[52vh] [@media(max-height:520px)]:max-h-none">
            {list.sites.map((site) => {
              const active = list.active === site.name;
              const isCurrent = site.name === selected;
              return (
                <button
                  key={site.name}
                  type="button"
                  onClick={() => pick(site)}
                  className={`w-full rounded-lg border px-2.5 py-2 text-left transition ${
                    isCurrent ? "border-accent-500/50 bg-accent-500/10" : "border-abyss-600 hover:border-white/15 hover:bg-abyss-800"
                  }`}
                >
                  <span className="flex items-center gap-1.5 text-xs font-semibold text-mist-100">
                    {site.name}
                    {active ? <span className="rounded bg-mint-400/15 px-1.5 py-0.5 text-[10px] font-medium text-mint-400">active</span> : null}
                  </span>
                  <span className="mt-0.5 block truncate font-mono text-[11px] text-mist-500">{site.server || "not configured"}</span>
                  <span className="mt-0.5 block text-[10px] uppercase tracking-wide text-mist-500/80">
                    {list.clientTypes[site.type] ?? "unknown"}
                  </span>
                </button>
              );
            })}
          </div>

          {/* Mobile: the two blocks stack, so mark the split clearly. */}
          <div className="my-1 h-px bg-mist-500/40 sm:hidden" />

          <div className="space-y-3">
            {form && profile ? (
              <>
                <Field label="Server">
                  <input
                    value={form.server}
                    onChange={(event) => setForm({ ...form, server: event.target.value })}
                    placeholder="ftp://192.168.1.10:21"
                    className="input"
                  />
                </Field>

                <div className="grid gap-3 sm:grid-cols-2">
                  <Field label="Username">
                    <input value={form.username} onChange={(event) => setForm({ ...form, username: event.target.value })} className="input" />
                  </Field>
                  <Field label={`${t("Password")}${profile.hasPassword ? ` ${t("(stored)")}` : ""}`}>
                    <input
                      type="password"
                      value={form.password}
                      onChange={(event) => setForm({ ...form, password: event.target.value })}
                      placeholder={profile.hasPassword ? "unchanged" : "empty"}
                      className="input"
                    />
                  </Field>
                </div>

                {isHttp ? (
                  <Field label="HTTP server type">
                    <select
                      value={form.httpServerType}
                      onChange={(event) => setForm({ ...form, httpServerType: event.target.value })}
                      className="input"
                    >
                      {list.httpServerTypes.map((type) => (
                        <option key={type} value={type}>
                          {type}
                        </option>
                      ))}
                    </select>
                  </Field>
                ) : null}

                <Field label="Default directory">
                  <input
                    value={form.defaultDirectory}
                    onChange={(event) => setForm({ ...form, defaultDirectory: event.target.value })}
                    className="input font-mono"
                  />
                </Field>

                <div className="flex flex-wrap gap-x-5 gap-y-2 pt-1">
                  <Check
                    label="Enable RPI"
                    checked={form.enableRpi}
                    onChange={(value) =>
                      // The disk cache only applies to the streaming path RPI
                      // enables, so it cannot outlive it.
                      setForm({ ...form, enableRpi: value, enableDiskCache: value ? form.enableDiskCache : false })
                    }
                  />
                  <Check
                    label="Disk cache"
                    checked={form.enableDiskCache && form.enableRpi}
                    disabled={!form.enableRpi}
                    onChange={(value) => setForm({ ...form, enableDiskCache: value })}
                  />
                </div>

                {error ? (
                  <p className="flex items-start gap-2 rounded-lg border border-rose-400/30 bg-rose-400/10 px-3 py-2 text-xs text-rose-400">
                    <Icon name="alert" size={14} className="mt-px shrink-0" />
                    {error}
                  </p>
                ) : null}
              </>
            ) : (
              <p className="text-sm text-mist-500">Select a site.</p>
            )}
          </div>
        </div>
        </>
      )}
    </Modal>
  );
}

function Field({ label, children }: { label: string; children: ReactNode }) {
  return (
    <label className="block">
      <span className="block text-xs font-medium text-mist-500">{label}</span>
      <span className="mt-1 block">{children}</span>
    </label>
  );
}

function Check({
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
