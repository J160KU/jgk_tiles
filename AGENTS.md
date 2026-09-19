# jgk_tiles

GNOME Wayland **16×8** overlay tiler. **Ctrl+Esc** shows a work-area grid; two clicks resize the last focused window. **Esc** dismisses.

## Architecture

- **C daemon** (`jgk_tiles`) — Wayland ARGB overlay, pointer/keyboard, `SIGUSR1` toggle from GNOME custom shortcut.
- **Shell extension** (`jgk-tiles@local`) — Unix socket `$XDG_RUNTIME_DIR/jgk_tiles.sock`: `STORE_FOCUS`, `RESIZE`, work area, overlay toggle.
- **Resize** — `Meta.Window.move_resize_frame` in-process. X11 EWMH cannot move native Wayland windows.

Grid math: `grid.c` (`GRID_COLS=16`, `GRID_ROWS=8`). Overlay draw uses surface-local coords; apply uses monitor work area (bar/dock excluded).

## Install / build

```bash
./install.sh          # deps + make + make install
make && make install
```

After first extension install on Wayland: log out/in, then `gnome-extensions enable jgk-tiles@local`.

## Layout

| Path | Role |
|------|------|
| `wayland.c` | Overlay, pointer, keyboard |
| `grid.c` | Hit-test and rectangle math |
| `bridge.c` | Socket client |
| `extension/` | Socket server + Meta resize |
| `install-keys.sh` | GNOME Ctrl+Esc binding |
| `install.sh` | Kit entry (deps + make install) |

## Docs

- [PROGRESS.md](PROGRESS.md) — status
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) — fixes
- [SECURITY.md](SECURITY.md) — trust surface
- [README.md](README.md) — human overview
