import { useEffect, useRef } from "react";
import { t } from "../i18n";
import type { ReactNode } from "react";

import { Icon } from "./Icon";

interface ModalProps {
  title: string;
  subtitle?: string;
  onClose: () => void;
  children: ReactNode;
  footer?: ReactNode;
  width?: string;
  /** When false the dialog cannot be closed by Escape, backdrop or the X. */
  dismissible?: boolean;
  /** Extra classes for the panel, e.g. a minimum height on phones. */
  panelClassName?: string;
}

export function Modal({ title, subtitle, onClose, children, footer, width = "max-w-lg", dismissible = true, panelClassName = "" }: ModalProps) {
  const panelRef = useRef<HTMLDivElement>(null);

  // Focus the panel once on mount. Deliberately not tied to onClose: callers
  // pass a fresh arrow each render, and re-running this would steal focus from
  // whatever field the user is typing in (e.g. while the app polls in the
  // background).
  useEffect(() => {
    const previous = document.activeElement as HTMLElement | null;
    // Prefer a text field so prompts open with the caret in the box; fall back
    // to the first button for action-only dialogs (the X comes first otherwise).
    const panel = panelRef.current;
    const field = panel?.querySelector<HTMLElement>("input, select, textarea");
    (field ?? panel?.querySelector<HTMLElement>("button"))?.focus();
    return () => {
      previous?.focus?.();
    };
  }, []);

  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key === "Escape" && dismissible) {
        event.stopPropagation();
        onClose();
      }
    };
    document.addEventListener("keydown", onKeyDown);
    return () => {
      document.removeEventListener("keydown", onKeyDown);
    };
  }, [onClose, dismissible]);

  return (
    <div
      className="fixed inset-0 z-50 overflow-y-auto bg-black/60 p-4 backdrop-blur-sm"
      onMouseDown={(event) => {
        if (dismissible && event.target === event.currentTarget) onClose();
      }}
      role="dialog"
      aria-modal="true"
      aria-label={t(title)}
    >
      <div className="flex min-h-full items-center justify-center">
      <div
        ref={panelRef}
        className={`my-auto flex max-h-[calc(100dvh-2rem)] w-full min-w-0 ${width} flex-col overflow-hidden rounded-2xl border border-abyss-600 bg-abyss-850 shadow-2xl shadow-black/50 ${panelClassName}`}
      >
        <header className="flex items-start justify-between gap-4 border-b border-abyss-600 px-5 py-4">
          <div className="min-w-0">
            <h2 className="text-lg font-semibold tracking-tight text-mist-100">{title}</h2>
            {/* Wraps instead of truncating: nowrap text widened the whole panel on phones. */}
            {subtitle ? <p className="mt-0.5 text-sm break-words text-mist-500">{typeof subtitle === "string" ? t(subtitle) : subtitle}</p> : null}
          </div>
          {dismissible ? (
            <button
              type="button"
              onClick={onClose}
              className="rounded-lg p-1.5 text-mist-500 transition hover:bg-white/5 hover:text-mist-100"
              aria-label="Close"
            >
              <Icon name="close" size={18} />
            </button>
          ) : null}
        </header>
        <div className="min-h-0 flex-1 overflow-x-hidden overflow-y-auto px-5 py-4">{children}</div>
        {footer ? <footer className="flex justify-end gap-2 border-t border-abyss-600 px-5 py-3.5">{footer}</footer> : null}
      </div>
      </div>
    </div>
  );
}
