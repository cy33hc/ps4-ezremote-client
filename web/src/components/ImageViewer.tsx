import { useState } from "react";

import { Modal } from "./Modal";

interface ImageViewerProps {
  src: string;
  name: string;
  onClose: () => void;
}

export function ImageViewer({ src, name, onClose }: ImageViewerProps) {
  const [actualSize, setActualSize] = useState(false);
  const [failed, setFailed] = useState(false);

  return (
    <Modal
      title={name}
      subtitle={`Image preview${actualSize ? " · click to fit" : " · click for actual size"}`}
      width="max-w-5xl"
      onClose={onClose}
      footer={
        <button type="button" onClick={onClose} className="rounded-lg bg-white/5 px-3 py-1.5 text-xs font-medium text-mist-200 hover:bg-white/10">
          Close
        </button>
      }
    >
      <div
        className="grid max-h-[70vh] min-h-[16rem] place-items-center overflow-auto rounded-lg border border-abyss-600 bg-abyss-800 p-2"
        onClick={() => setActualSize((value) => !value)}
      >
        {failed ? (
          <p className="text-sm text-rose-400">This image could not be displayed.</p>
        ) : (
          <img
            src={src}
            alt={name}
            onError={() => setFailed(true)}
            className={actualSize ? "max-w-none cursor-zoom-out" : "max-h-[66vh] max-w-full cursor-zoom-in object-contain"}
          />
        )}
      </div>
    </Modal>
  );
}
