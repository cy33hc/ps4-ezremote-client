import { useCallback, useEffect, useState } from "react";

import * as api from "../api/client";
import type { Favorite } from "../api/client";
import { Icon } from "./Icon";
import { Modal } from "./Modal";

interface FavoritesDialogProps {
  onClose: () => void;
  onInstall: (url: string) => void;
  onDownload: (url: string) => void;
}

export function FavoritesDialog({ onClose, onInstall, onDownload }: FavoritesDialogProps) {
  const [favorites, setFavorites] = useState<Favorite[]>([]);
  const [capacity, setCapacity] = useState(30);
  const [url, setUrl] = useState("");
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);

  const load = useCallback(async () => {
    const data = await api.listFavorites();
    setFavorites(data.favorites);
    setCapacity(data.capacity);
  }, []);

  useEffect(() => {
    let cancelled = false;
    api
      .listFavorites()
      .then((data) => {
        if (cancelled) return;
        setFavorites(data.favorites);
        setCapacity(data.capacity);
      })
      .catch((cause: unknown) => {
        if (!cancelled) setError(cause instanceof Error ? cause.message : "Cannot load favourites");
      })
      .finally(() => {
        if (!cancelled) setLoading(false);
      });
    return () => {
      cancelled = true;
    };
  }, []);

  const add = async () => {
    const value = url.trim();
    if (!value || busy) return;
    setBusy(true);
    setError(null);
    try {
      await api.saveFavorite(value);
      setUrl("");
      await load();
    } catch (cause) {
      setError(cause instanceof Error ? cause.message : "Cannot save this URL");
    } finally {
      setBusy(false);
    }
  };

  const remove = async (index: number) => {
    setBusy(true);
    setError(null);
    try {
      await api.removeFavorite(index);
      await load();
    } catch (cause) {
      setError(cause instanceof Error ? cause.message : "Cannot remove this favourite");
    } finally {
      setBusy(false);
    }
  };

  return (
    <Modal
      title="Favourites"
      subtitle={`${favorites.length} of ${capacity} saved package URLs`}
      width="max-w-3xl"
      onClose={onClose}
      footer={
        <button type="button" onClick={onClose} className="rounded-lg bg-white/5 px-3 py-1.5 text-xs font-medium text-mist-200 hover:bg-white/10">
          Close
        </button>
      }
    >
      <div className="mb-3 flex gap-2">
        <input
          value={url}
          onChange={(event) => setUrl(event.target.value)}
          onKeyDown={(event) => {
            if (event.key === "Enter") void add();
          }}
          placeholder="https://example.com/game.pkg"
          className="input flex-1 font-mono text-xs"
        />
        <button
          type="button"
          onClick={() => void add()}
          disabled={!url.trim() || busy}
          className="rounded-lg bg-accent-500 px-3 py-1.5 text-xs font-semibold text-white transition hover:bg-accent-600 disabled:opacity-40"
        >
          Add
        </button>
      </div>

      {error ? (
        <p className="mb-3 flex items-start gap-2 rounded-lg border border-rose-400/30 bg-rose-400/10 px-3 py-2 text-xs text-rose-400">
          <Icon name="alert" size={14} className="mt-px shrink-0" />
          {error}
        </p>
      ) : null}

      {loading ? (
        <p className="py-8 text-center text-sm text-mist-500">Loading…</p>
      ) : favorites.length === 0 ? (
        <p className="py-8 text-center text-sm text-mist-500">No favourites yet. Add a package URL above.</p>
      ) : (
        <ul className="max-h-[46vh] space-y-1 overflow-y-auto">
          {favorites.map((favorite) => (
            <li
              key={favorite.index}
              className="flex items-center gap-2 rounded-lg border border-abyss-600 px-2.5 py-2 hover:border-abyss-600 hover:bg-abyss-800"
            >
              <Icon name="star" size={14} className="shrink-0 text-amber-400" />
              <span className="min-w-0 flex-1 truncate font-mono text-[11px] text-mist-300" title={favorite.url}>
                {favorite.url}
              </span>
              <button
                type="button"
                onClick={() => onInstall(favorite.url)}
                className="rounded-md border border-abyss-600 px-2 py-1 text-[11px] font-medium text-accent-300 transition hover:bg-white/5"
              >
                Install
              </button>
              <button
                type="button"
                onClick={() => onDownload(favorite.url)}
                className="rounded-md border border-abyss-600 px-2 py-1 text-[11px] font-medium text-mist-300 transition hover:bg-white/5"
              >
                Download
              </button>
              <button
                type="button"
                onClick={() => void remove(favorite.index)}
                disabled={busy}
                className="rounded-md p-1 text-mist-500 transition hover:bg-white/5 hover:text-rose-400 disabled:opacity-40"
                aria-label="Remove favourite"
              >
                <Icon name="trash" size={14} />
              </button>
            </li>
          ))}
        </ul>
      )}
    </Modal>
  );
}
