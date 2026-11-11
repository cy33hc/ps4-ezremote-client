# ezRemote Client — web UI

The console UI is a static single-page app served by the app's built-in HTTP
server. Vite builds straight into `../data/assets`, which is what ships in the
package as `/data/ezremote-client` and is served from `/app0/assets`.

## Requirements

- Node 20+ and npm

## Develop

```bash
npm install
npm run dev
```

The dev server only serves the UI; `/__local__/*` calls need a running console.
Point the browser at the console (`http://<ps4-ip>:8080/index.html`) to exercise
the real API, or proxy `/__local__` to the console while iterating.

## Build

```bash
npm run build
```

This runs `tsc -b` and then Vite, writing:

- `../data/assets/index.html`
- `../data/assets/assets/index-<hash>.{js,css}`

The build is minified and content-hashed. The base URL is pinned to
`/app0/assets/` because the console serves the whole
filesystem with `/` mounted at the HTTP root.

The base URL is pinned to `/app0/assets/` because the
console serves the whole filesystem with `/` mounted at the HTTP root.

## Theme

Colors mirror the native ImGui style in `source/main.cpp` (`InitImgui`):

| Token | Value | ImGui role |
| --- | --- | --- |
| window | `#252526` | `WindowBg` / `PopupBg` |
| panel | `#333337` | `ChildBg` / `FrameBg` / `Button` |
| border | `#4E4E4E` | `Border` / `Separator` |
| accent-500 | `#0077C8` | `HeaderActive` / `CheckMark` / selection |
| accent-400 | `#1D97EC` | `ButtonHovered` / `FrameBgHovered` |
| title | `#0A648E` | `TitleBgActive` (active pane label) |
| text | `#FFFFFF` / `#979797` | `Text` / `TextDisabled` |

## Features

Commander-style two-pane layout: local files on the left, the active remote site on the right.
Actions live in right-click context menus (per item or empty area), not a toolbar.

- Cross-pane transfers: copy/move local -> remote and remote -> local (`/__remote__/copy_from_local`,
  `/__local__/copy_from_remote`)
- Double-click a folder to enter it, a file to download it, an image to preview it

- Local file manager: breadcrumb navigation, filter, sortable columns, multi-select
- New folder, rename, delete, copy/cut/paste, properties
- Download single files, or several as one zip
- Upload with 4 MB chunked multipart requests
- Install selected `.pkg` files, install from URL, download from URL
- Extract `.zip`/`.rar`/`.7z`, compress selection to a zip
- Built-in text editor (`getContent`/`edit`)
- Image preview for local and remote images (click to toggle actual size)
- Package info (`/__local__/pkg_info`): SFO/param.json attributes and icon for a local `.pkg`
- Server profiles (`/__local__/servers`): edit URL/credentials/options and choose the active site
- Remote browsing (`/__remote__/list`): switch Local/Remote to navigate FTP/SFTP/SMB/NFS/WebDAV/HTTP
  servers through a per-site connection held by the console
- Global settings (`/__local__/settings`): language, temp folder, hidden files, background
  download threshold, debrid keys, web server port/paths
- Favourites (`/__local__/favorites`): save package URLs and install/download them with the URL dialogs
- Remote upload (`/__remote__/upload`): chunked, staged in the temp folder, Put() on the last chunk
- Remote download to the browser (`/__remote__/download`, streamed with Range support), plus
  remote new folder / rename / delete (`/__remote__/mkdir|rename|remove`)

### Keyboard shortcuts

| Keys | Action |
| --- | --- |
| `Enter` / double-click | Open folder, or download a file |
| `Backspace` | Parent folder |
| `F5` | Refresh |
| `F2` | Rename selection |
| `Ctrl/Cmd+C` / `X` / `V` | Copy / cut / paste |
| `Delete` | Delete selection |
| `Ctrl/Cmd+S` | Save (inside the editor) |

## Layout

- `src/api/client.ts` — typed wrapper over the `/__local__` endpoints
- `src/components/` — icon set, modal, toasts, file table
- `src/lib/format.ts` — path and size/date helpers
- `src/App.tsx` — app shell, file manager state and actions
