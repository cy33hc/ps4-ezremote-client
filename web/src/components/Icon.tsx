import type { SVGProps } from "react";

export type IconName =
  | "folder"
  | "folderPlus"
  | "file"
  | "archive"
  | "package"
  | "refresh"
  | "upload"
  | "download"
  | "install"
  | "trash"
  | "rename"
  | "chevronRight"
  | "close"
  | "check"
  | "search"
  | "link"
  | "home"
  | "alert"
  | "cut"
  | "copy"
  | "paste"
  | "info"
  | "text"
  | "server"
  | "star"
  | "settings"
  | "power"
  | "logout"
  | "activity"
  | "image"
  | "selectAll"
  | "sortAsc"
  | "sortDesc";

const PATHS: Record<IconName, string[]> = {
  folder: ["M3 7.5A2.5 2.5 0 0 1 5.5 5h3.2l1.8 2H18.5A2.5 2.5 0 0 1 21 9.5v8A2.5 2.5 0 0 1 18.5 20h-13A2.5 2.5 0 0 1 3 17.5z"],
  folderPlus: [
    "M3 7.5A2.5 2.5 0 0 1 5.5 5h3.2l1.8 2H18.5A2.5 2.5 0 0 1 21 9.5v8A2.5 2.5 0 0 1 18.5 20h-13A2.5 2.5 0 0 1 3 17.5z",
    "M12 11v6M9 14h6",
  ],
  file: ["M7 3.5h6.5L18 8v12.5H7z", "M13.5 3.5V8H18"],
  archive: ["M4 6.5h16v13H4z", "M4 6.5V4h16v2.5", "M10 10.5h4"],
  package: ["M12 3 20 7v10l-8 4-8-4V7z", "M4 7l8 4 8-4", "M12 11v10"],
  refresh: ["M20 12a8 8 0 1 1-2.3-5.6", "M20 4v4.5h-4.5"],
  upload: ["M12 16V4", "M7.5 8.5 12 4l4.5 4.5", "M4 16v3.5h16V16"],
  download: ["M12 4v12", "M7.5 11.5 12 16l4.5-4.5", "M4 16v3.5h16V16"],
  install: ["M12 3.5v9", "M8.5 9 12 12.5 15.5 9", "M4.5 15v5h15v-5", "M4.5 17.5h15"],
  trash: ["M4 7h16", "M9.5 7V4.5h5V7", "M6.5 7l1 13h9l1-13", "M10 11v5.5M14 11v5.5"],
  rename: ["M4 20h4l10.5-10.5a2.1 2.1 0 0 0-3-3L5 17z", "M4 20v-3.5"],
  chevronRight: ["M9.5 6 15.5 12l-6 6"],
  close: ["M6 6l12 12M18 6 6 18"],
  check: ["M4.5 12.5 9.5 17.5 19.5 6.5"],
  search: ["M10.5 17a6.5 6.5 0 1 0 0-13 6.5 6.5 0 0 0 0 13z", "M15.5 15.5 20 20"],
  link: ["M10 13.5a4 4 0 0 0 5.7 0l2.8-2.8a4 4 0 0 0-5.7-5.7L11.5 6.3", "M14 10.5a4 4 0 0 0-5.7 0l-2.8 2.8a4 4 0 0 0 5.7 5.7l1.3-1.3"],
  home: ["M4 11 12 4l8 7", "M6 9.5V20h12V9.5"],
  alert: ["M12 4 2.8 20h18.4z", "M12 10v4.5M12 17.5v.5"],
  cut: ["M6.5 10a3 3 0 1 0 0-6 3 3 0 0 0 0 6z", "M6.5 20a3 3 0 1 0 0-6 3 3 0 0 0 0 6z", "M8.7 8.7 19 19", "M19 5 8.7 15.3"],
  copy: ["M9 9h11v11H9z", "M5 15V4h11"],
  paste: ["M9 4h6v3H9z", "M7 5H5v15h14V5h-2"],
  info: ["M12 21a9 9 0 1 0 0-18 9 9 0 0 0 0 18z", "M12 11v6M12 7.5v.5"],
  text: ["M4.5 6.5h15", "M4.5 12h15", "M4.5 17.5h9"],
  server: ["M4 5.5h16v5H4z", "M4 13.5h16v5H4z", "M7.5 8h.5M7.5 16h.5"],
  star: ["M12 4.5l2.4 4.9 5.4.8-3.9 3.8.9 5.4-4.8-2.5-4.8 2.5.9-5.4L4.2 10.2l5.4-.8z"],
  selectAll: ["M4 6.5h4M4 12h4M4 17.5h4", "M11 6.5h9M11 12h9M11 17.5h9"],
  image: ["M4 5h16v14H4z", "M8.5 11a1.5 1.5 0 1 0 0-3 1.5 1.5 0 0 0 0 3z", "M5 17l4.5-4.5 3.5 3.5 3-3L20 17"],
  settings: ["M12 15.2a3.2 3.2 0 1 0 0-6.4 3.2 3.2 0 0 0 0 6.4z", "M19.4 13.5a7.6 7.6 0 0 0 0-3l2-1.5-2-3.4-2.3 1a7.6 7.6 0 0 0-2.6-1.5L14.1 2h-4.2l-.4 2.6a7.6 7.6 0 0 0-2.6 1.5l-2.3-1-2 3.4 2 1.5a7.6 7.6 0 0 0 0 3l-2 1.5 2 3.4 2.3-1a7.6 7.6 0 0 0 2.6 1.5l.4 2.6h4.2l.4-2.6a7.6 7.6 0 0 0 2.6-1.5l2.3 1 2-3.4z"],
  activity: ["M3 12h3.5l2.5-7 4 14 2.5-7H21"],
  logout: ["M15 3h3a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2h-3", "M10 17l5-5-5-5", "M15 12H3"],
  power: ["M12 3v9", "M18.4 6.6a9 9 0 1 1-12.8 0"],
  sortAsc: ["M8 6v12", "M4.5 14.5 8 18l3.5-3.5", "M14 8h6M14 12h4.5M14 16h3"],
  sortDesc: ["M8 18V6", "M4.5 9.5 8 6l3.5 3.5", "M14 8h3M14 12h4.5M14 16h6"],
};

interface IconProps extends Omit<SVGProps<SVGSVGElement>, "name"> {
  name: IconName;
  size?: number;
}

export function Icon({ name, size = 18, ...rest }: IconProps) {
  return (
    <svg
      width={size}
      height={size}
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth={1.7}
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
      {...rest}
    >
      {PATHS[name].map((d) => (
        <path key={d} d={d} />
      ))}
    </svg>
  );
}
