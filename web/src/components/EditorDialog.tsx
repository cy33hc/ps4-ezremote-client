import { useEffect, useState } from "react";

import * as api from "../api/client";
import { Modal } from "./Modal";

interface EditorDialogProps {
  item: string;
  name: string;
  onClose: () => void;
  onSaved: () => void;
  /** Overrides for remote files; default to the local endpoints. */
  load?: (item: string) => Promise<string>;
  save?: (item: string, content: string) => Promise<void>;
}

export function EditorDialog({ item, name, onClose, onSaved, load, save: saveFile }: EditorDialogProps) {
  const [content, setContent] = useState("");
  const [original, setOriginal] = useState("");
  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    let cancelled = false;
    (load ?? api.getContent)(item)
      .then((text) => {
        if (cancelled) return;
        setContent(text);
        setOriginal(text);
      })
      .catch((cause: unknown) => {
        if (!cancelled) setError(cause instanceof Error ? cause.message : "Cannot open this file");
      })
      .finally(() => {
        if (!cancelled) setLoading(false);
      });
    return () => {
      cancelled = true;
    };
  }, [item, load]);

  const dirty = content !== original;

  const save = async () => {
    if (loading || saving || !dirty) return;
    setSaving(true);
    setError(null);
    try {
      await (saveFile ?? api.edit)(item, content);
      setOriginal(content);
      onSaved();
      onClose();
    } catch (cause) {
      setError(cause instanceof Error ? cause.message : "Cannot save this file");
    } finally {
      setSaving(false);
    }
  };

  return (
    <Modal
      title={name}
      subtitle={item}
      width="max-w-4xl"
      onClose={onClose}
      footer={
        <>
          <span className="mr-auto text-[11px] text-mist-500">
            {loading ? "Loading…" : dirty ? "Unsaved changes" : "No changes"}
          </span>
          <button type="button" onClick={onClose} className="rounded-lg px-3 py-1.5 text-xs font-medium text-mist-300 hover:bg-white/5">
            Cancel
          </button>
          <button
            type="button"
            onClick={() => void save()}
            disabled={!dirty || saving || loading}
            className="rounded-lg bg-accent-500 px-3 py-1.5 text-xs font-semibold text-white transition hover:bg-accent-600 disabled:opacity-40"
          >
            {saving ? "Saving…" : "Save"}
          </button>
        </>
      }
    >
      {error ? (
        <p className="mb-3 rounded-lg border border-rose-400/30 bg-rose-400/10 px-3 py-2 text-xs text-rose-400">{error}</p>
      ) : null}
      <textarea
        value={content}
        readOnly={loading}
        spellCheck={false}
        onChange={(event) => setContent(event.target.value)}
        onKeyDown={(event) => {
          if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "s") {
            event.preventDefault();
            void save();
          }
        }}
        className="h-[58vh] w-full resize-none rounded-lg border border-abyss-600 bg-abyss-900 p-3 font-mono text-xs leading-relaxed text-mist-100 focus:border-accent-500/60 focus:outline-none"
        placeholder={loading ? "Loading…" : ""}
      />
    </Modal>
  );
}
