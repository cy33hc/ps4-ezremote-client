import { useState } from "react";

import * as api from "../api/client";
import type { DirEntry } from "../api/client";
import { formatSize, joinPath } from "../lib/format";
import { Modal } from "./Modal";

interface PropertiesDialogProps {
  entry: DirEntry;
  path: string;
  /** Remote entries do not carry meaningful permission bits. */
  remote?: boolean;
  onClose: () => void;
  /** Called after a successful permission change so the pane can refresh. */
  onRefresh?: () => void;
}

function Row({ label, value, mono }: { label: string; value: string; mono?: boolean }) {
  return (
    <div className="grid grid-cols-[7.5rem_1fr] gap-3 border-b border-abyss-600 py-2 text-sm last:border-0">
      <span className="text-xs text-mist-500">{label}</span>
      <span className={`min-w-0 break-all text-mist-100 ${mono ? "font-mono text-xs" : ""}`}>{value || "—"}</span>
    </div>
  );
}

export function PropertiesDialog({ entry, path, remote, onClose, onRefresh }: PropertiesDialogProps) {
  const parse = (value: string): boolean[] => {
    const text = value.length === 10 ? value.slice(1) : value;
    return Array.from({ length: 9 }, (_, i) => "rwxst".includes(text[i] ?? ""));
  };

  const initial = parse(entry.rights);
  const [bits, setBits] = useState<boolean[]>(initial);
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const rights = bits.map((on, i) => (on ? "rwx"[i % 3] : "-")).join("");
  const changed = rights !== initial.map((on, i) => (on ? "rwx"[i % 3] : "-")).join("");

  const applyRights = async () => {
    if (remote || saving || !changed) return;
    setSaving(true);
    setError(null);
    try {
      await api.chmod(joinPath(path, entry.name), rights);
      onRefresh?.();
    } catch (cause) {
      setError(cause instanceof Error ? cause.message : "Could not change permissions");
    } finally {
      setSaving(false);
    }
  };

  return (
    <Modal
      title="Properties"
      subtitle={entry.name}
      onClose={onClose}
      footer={
        <button type="button" onClick={onClose} className="rounded-lg bg-white/5 px-3 py-1.5 text-xs font-medium text-mist-200 hover:bg-white/10">
          Close
        </button>
      }
    >
      <div className="-my-1">
        <Row label="Name" value={entry.name} />
        <Row label="Location" value={joinPath(path, entry.name)} mono />
        <Row label="Type" value={entry.type === "dir" ? "Folder" : "File"} />
        <Row label="Size" value={entry.type === "dir" ? "" : `${formatSize(entry.size)} (${Number(entry.size || 0).toLocaleString()} bytes)`} />
        <Row label="Modified" value={entry.date} />
        {remote ? null : (
          <div className="grid grid-cols-[7.5rem_1fr] gap-3 border-b border-abyss-600 py-2 text-sm">
            <span className="text-xs text-mist-500">Rights</span>
            <div className="min-w-0 space-y-1">
              {["User", "Group", "Others"].map((who, row) => (
                <div key={who} className="flex items-center gap-3">
                  <span className="w-14 shrink-0 text-[11px] text-mist-500">{who}</span>
                  {["r", "w", "x"].map((action, col) => {
                    const index = row * 3 + col;
                    return (
                      <label key={action} className="flex items-center gap-1 text-[11px] text-mist-300">
                        <input
                          type="checkbox"
                          checked={bits[index]}
                          onChange={() => setBits((current) => current.map((on, i) => (i === index ? !on : on)))}
                          className="size-3.5 accent-accent-500"
                        />
                        {action}
                      </label>
                    );
                  })}
                </div>
              ))}
              <div className="flex items-center gap-2 pt-1">
                <span className="font-mono text-[11px] text-mist-500">{rights}</span>
                <button
                  type="button"
                  onClick={() => void applyRights()}
                  disabled={saving || !changed}
                  className="rounded-lg bg-accent-500 px-2.5 py-1 text-xs font-semibold text-white transition hover:bg-accent-600 disabled:cursor-default disabled:opacity-40 disabled:hover:bg-accent-500"
                >
                  {saving ? "…" : "Apply"}
                </button>
              </div>
            </div>
          </div>
        )}
        {error ? <p className="pt-2 text-xs text-rose-400">{error}</p> : null}
      </div>
    </Modal>
  );
}
