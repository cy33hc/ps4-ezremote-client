import { useEffect, useState } from "react";
import { t } from "../i18n";
import type { ReactNode } from "react";

import * as api from "../api/client";
import type { Settings } from "../api/client";
import { Icon } from "./Icon";
import { Modal } from "./Modal";

interface SettingsDialogProps {
  onClose: () => void;
  onSaved: () => void;
}

const MB = 1024 * 1024;

export function SettingsDialog({ onClose, onSaved }: SettingsDialogProps) {
  const [settings, setSettings] = useState<Settings | null>(null);
  const [allDebrid, setAllDebrid] = useState("");
  const [realDebrid, setRealDebrid] = useState("");
  const [minSizeMb, setMinSizeMb] = useState("0");
  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);
  const [webPassword, setWebPassword] = useState("");
  const [removeWebPassword, setRemoveWebPassword] = useState(false);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    let cancelled = false;
    api
      .getSettings()
      .then((data) => {
        if (cancelled) return;
        setSettings(data);
        setMinSizeMb(String(Math.round(data.minimumBackgroundFileSize / MB)));
      })
      .catch((cause: unknown) => {
        if (!cancelled) setError(cause instanceof Error ? cause.message : "Cannot load settings");
      })
      .finally(() => {
        if (!cancelled) setLoading(false);
      });
    return () => {
      cancelled = true;
    };
  }, []);

  const update = <K extends keyof Settings>(key: K, value: Settings[K]) => {
    setSettings((current) => (current ? { ...current, [key]: value } : current));
  };

  const save = async () => {
    if (!settings || saving) return;
    setSaving(true);
    setError(null);
    try {
      const changedPassword = removeWebPassword || webPassword.length > 0;

      await api.saveSettings({
        language: settings.language,
        autoDeleteTmpPkg: settings.autoDeleteTmpPkg,
        showHiddenFiles: settings.showHiddenFiles,
        tempFolder: settings.tempFolder,
        enableBackgroundDownload: settings.enableBackgroundDownload,
        showActivityDialog: settings.showActivityDialog,
        minimumBackgroundFileSize: Math.max(0, Number(minSizeMb) || 0) * MB,
        webServerEnabled: settings.webServerEnabled,
        httpServerPort: settings.httpServerPort,
        compressedFilePath: settings.compressedFilePath,
        webSessionTimeout: Math.max(1, Number(settings.webSessionTimeout) || 60),
        alldebridApiKey: allDebrid ? allDebrid : undefined,
        realdebridApiKey: realDebrid ? realDebrid : undefined,
        webPassword: removeWebPassword ? "" : webPassword ? webPassword : undefined,
      });
      onSaved();
      onClose();
      if (changedPassword) {
        // The change invalidates every session, including this one.
        window.location.reload();
      }
    } catch (cause) {
      setError(cause instanceof Error ? cause.message : "Cannot save settings");
    } finally {
      setSaving(false);
    }
  };

  return (
    <Modal
      title="Settings"
      subtitle="Stored on the console. Server port and web server changes apply after a restart."
      width="max-w-3xl"
      panelClassName="min-h-[22rem] sm:min-h-0 [@media(max-height:520px)]:min-h-[30rem]"
      onClose={onClose}
      footer={
        <>
          <button type="button" onClick={onClose} className="rounded-lg px-3 py-1.5 text-xs font-medium text-mist-300 hover:bg-white/5">
            Cancel
          </button>
          <button
            type="button"
            onClick={() => void save()}
            disabled={!settings || saving}
            className="rounded-lg bg-accent-500 px-3 py-1.5 text-xs font-semibold text-white transition hover:bg-accent-600 disabled:opacity-40"
          >
            {saving ? "Saving…" : "Save"}
          </button>
        </>
      }
    >
      {loading ? (
        <p className="py-10 text-center text-sm text-mist-500">Loading…</p>
      ) : !settings ? (
        <p className="py-10 text-center text-sm text-rose-400">{error ?? "No settings"}</p>
      ) : (
        <div className="max-h-[58vh] space-y-5 overflow-y-auto pr-1 [@media(max-height:520px)]:max-h-none">
          <Section title="General">
            <Field label="Language">
              <select value={settings.language} onChange={(event) => update("language", event.target.value)} className="input">
                {settings.languages.map((language) => (
                  <option key={language} value={language}>
                    {language}
                  </option>
                ))}
              </select>
            </Field>
            <Field label="Temp folder">
              <input value={settings.tempFolder} onChange={(event) => update("tempFolder", event.target.value)} className="input font-mono" />
            </Field>
            <Check label="Delete temporary package after install" checked={settings.autoDeleteTmpPkg} onChange={(value) => update("autoDeleteTmpPkg", value)} />
            <Check label="Show hidden files" checked={settings.showHiddenFiles} onChange={(value) => update("showHiddenFiles", value)} />
            <Check
              label="Open the activities dialog when a new activity starts"
              checked={settings.showActivityDialog}
              onChange={(value) => update("showActivityDialog", value)}
            />
          </Section>

          <Section title="Downloads">
            <Check
              label="Download in the background"
              checked={settings.enableBackgroundDownload}
              onChange={(value) => update("enableBackgroundDownload", value)}
            />
            <Field label="Minimum size for background download (MB)">
              <input
                value={minSizeMb}
                onChange={(event) => setMinSizeMb(event.target.value)}
                inputMode="numeric"
                className="input w-32 font-mono"
              />
            </Field>
            <div className="grid gap-3 sm:grid-cols-2">
              <Field label={`${t("AllDebrid API key")}${settings.hasAllDebridKey ? ` ${t("(stored)")}` : ""}`}>
                <input
                  type="password"
                  value={allDebrid}
                  onChange={(event) => setAllDebrid(event.target.value)}
                  placeholder={settings.hasAllDebridKey ? "unchanged" : "empty"}
                  className="input"
                />
              </Field>
              <Field label={`${t("RealDebrid API key")}${settings.hasRealDebridKey ? ` ${t("(stored)")}` : ""}`}>
                <input
                  type="password"
                  value={realDebrid}
                  onChange={(event) => setRealDebrid(event.target.value)}
                  placeholder={settings.hasRealDebridKey ? "unchanged" : "empty"}
                  className="input"
                />
              </Field>
            </div>
          </Section>

          <Section title="Web server">
            <Check label="Enable web server" checked={settings.webServerEnabled} onChange={(value) => update("webServerEnabled", value)} />
            <div className="grid gap-3 sm:grid-cols-2">
              <Field label="Port">
                <input
                  value={String(settings.httpServerPort)}
                  onChange={(event) => update("httpServerPort", Number(event.target.value.replace(/\D/g, "")) || 0)}
                  inputMode="numeric"
                  className="input font-mono"
                />
              </Field>
              <Field label="Session timeout (minutes)">
                <input
                  value={String(settings.webSessionTimeout)}
                  onChange={(event) => update("webSessionTimeout", Number(event.target.value.replace(/\D/g, "")) || 0)}
                  inputMode="numeric"
                  className="input font-mono"
                />
              </Field>
              <Field label={`${t("Web UI password")}${settings.hasWebPassword ? ` ${t("(stored)")}` : ""}`}>
                <input
                  type="password"
                  value={webPassword}
                  onChange={(event) => setWebPassword(event.target.value)}
                  placeholder={settings.hasWebPassword ? "Leave blank to keep" : "No password set"}
                  className="input"
                />
              </Field>
              <Field label="Compressed files folder">
                <input
                  value={settings.compressedFilePath}
                  onChange={(event) => update("compressedFilePath", event.target.value)}
                  className="input font-mono"
                />
              </Field>
            </div>
            {settings.hasWebPassword ? (
              <Check
                label="Remove the web UI password"
                checked={removeWebPassword}
                onChange={setRemoveWebPassword}
              />
            ) : null}
          </Section>

          {error ? (
            <p className="flex items-start gap-2 rounded-lg border border-rose-400/30 bg-rose-400/10 px-3 py-2 text-xs text-rose-400">
              <Icon name="alert" size={14} className="mt-px shrink-0" />
              {error}
            </p>
          ) : null}
        </div>
      )}
    </Modal>
  );
}

function Section({ title, children }: { title: string; children: ReactNode }) {
  return (
    <section className="space-y-3">
      <h3 className="text-[11px] font-semibold uppercase tracking-wider text-mist-500">{title}</h3>
      {children}
    </section>
  );
}

function Field({ label, children }: { label: string; children: ReactNode }) {
  return (
    <label className="block">
      <span className="block text-xs font-medium text-mist-500">{t(label)}</span>
      <span className="mt-1 block">{children}</span>
    </label>
  );
}

function Check({ label, checked, onChange }: { label: string; checked: boolean; onChange: (value: boolean) => void }) {
  return (
    <label className="flex cursor-pointer items-center gap-2 text-xs text-mist-300">
      <input type="checkbox" checked={checked} onChange={(event) => onChange(event.target.checked)} className="size-3.5 accent-accent-500" />
      {t(label)}
    </label>
  );
}
