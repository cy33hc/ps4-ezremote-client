import { Icon } from "./Icon";

export type ToastTone = "success" | "error" | "info";

export interface Toast {
  id: number;
  tone: ToastTone;
  message: string;
}

const TONE_STYLES: Record<ToastTone, string> = {
  success: "border-mint-400/30 bg-mint-400/10 text-mint-400",
  error: "border-rose-400/30 bg-rose-400/10 text-rose-400",
  info: "border-accent-400/30 bg-accent-400/10 text-accent-300",
};

const TONE_ICON = { success: "check", error: "alert", info: "alert" } as const;

interface ToastsProps {
  toasts: Toast[];
  onDismiss: (id: number) => void;
}

export function Toasts({ toasts, onDismiss }: ToastsProps) {
  if (toasts.length === 0) return null;

  return (
    <div className="pointer-events-none fixed inset-x-0 bottom-0 z-50 flex flex-col items-center gap-2 p-4 sm:items-end">
      {toasts.map((toast) => (
        <div
          key={toast.id}
          className="pointer-events-auto flex w-full max-w-md items-start gap-3 rounded-xl border border-abyss-600 bg-abyss-850/95 px-4 py-3 shadow-xl shadow-black/40 backdrop-blur"
          role="status"
        >
          <span className={`mt-0.5 rounded-full border p-1 ${TONE_STYLES[toast.tone]}`}>
            <Icon name={TONE_ICON[toast.tone]} size={14} />
          </span>
          <p className="flex-1 text-sm leading-snug text-mist-100">{toast.message}</p>
          <button
            type="button"
            className="rounded-md p-1 text-mist-500 transition hover:bg-white/5 hover:text-mist-100"
            onClick={() => onDismiss(toast.id)}
            aria-label="Dismiss"
          >
            <Icon name="close" size={14} />
          </button>
        </div>
      ))}
    </div>
  );
}
