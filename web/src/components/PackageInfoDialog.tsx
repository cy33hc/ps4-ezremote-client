import { useEffect, useMemo, useState } from "react";
import { t } from "../i18n";

import * as api from "../api/client";
import { Icon } from "./Icon";
import { Modal } from "./Modal";

interface PackageInfoDialogProps {
  item: string;
  name: string;
  onClose: () => void;
  onInstall: () => void;
  /** When set, the package is read over the remote session instead of locally. */
  remote?: { site: string };
}

export function PackageInfoDialog({ item, name, onClose, onInstall, remote }: PackageInfoDialogProps) {
  const [params, setParams] = useState<Record<string, string> | null>(null);
  const [hasIcon, setHasIcon] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(true);
  const iconUrl = useMemo(
    () => (remote ? `/__remote__/pkg_icon?t=${Date.now()}` : `/__local__/pkg_icon?t=${Date.now()}`),
    [remote],
  );

  useEffect(() => {
    let cancelled = false;
    (remote ? api.remotePkgInfo(remote.site, item) : api.pkgInfo(item))
      .then((info) => {
        if (cancelled) return;
        setParams(info.params);
        setHasIcon(info.hasIcon);
      })
      .catch((cause: unknown) => {
        if (!cancelled) setError(cause instanceof Error ? cause.message : "Cannot read this package");
      })
      .finally(() => {
        if (!cancelled) setLoading(false);
      });
    return () => {
      cancelled = true;
    };
  }, [item, remote]);

  return (
    <Modal
      title="Package info"
      subtitle={name}
      width="max-w-4xl"
      onClose={onClose}
      footer={
        <>
          <button type="button" onClick={onClose} className="rounded-lg px-3 py-1.5 text-xs font-medium text-mist-300 hover:bg-white/5">
            {t("Close")}
          </button>
          <button
            type="button"
            onClick={onInstall}
            className="rounded-lg bg-accent-500 px-3 py-1.5 text-xs font-semibold text-white transition hover:bg-accent-600"
          >
            {t("Install")}
          </button>
        </>
      }
    >
      {loading ? (
        <p className="py-10 text-center text-sm text-mist-500">Reading package…</p>
      ) : error ? (
        <p className="flex items-start gap-2 rounded-lg border border-rose-400/30 bg-rose-400/10 px-3 py-2 text-xs text-rose-400">
          <Icon name="alert" size={14} className="mt-px shrink-0" />
          {error}
        </p>
      ) : (
        <div className="grid gap-4 sm:grid-cols-[12rem_1fr]">
          <div className="grid aspect-square place-items-center overflow-hidden rounded-xl border border-abyss-600 bg-abyss-800">
            {hasIcon ? (
              <img src={iconUrl} alt="" className="size-full object-contain" />
            ) : (
              <Icon name="package" size={48} className="text-mist-500/50" />
            )}
          </div>
          <div className="max-h-[52vh] overflow-y-auto pr-1">
            {params && Object.keys(params).length > 0 ? (
              <dl className="divide-y divide-abyss-600">
                {Object.entries(params)
                  .filter(([, value]) => value !== "")
                  .map(([key, value]) => (
                    <div key={key} className="grid grid-cols-[10rem_1fr] gap-3 py-1.5">
                      <dt className="text-xs text-mist-500">{key}</dt>
                      <dd className="min-w-0 break-words text-xs text-mist-100">{value}</dd>
                    </div>
                  ))}
              </dl>
            ) : (
              <p className="text-sm text-mist-500">No attributes found in this package.</p>
            )}
          </div>
        </div>
      )}
    </Modal>
  );
}
