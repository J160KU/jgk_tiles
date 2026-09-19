# jgk_tiles

Fast 16×8 window tiler for GNOME on Wayland. Press **Ctrl+Esc**, click two tiles, and the last focused window is resized to that rectangle. **Esc** dismisses the overlay.

The overlay stays in the monitor work area (not under the top bar or dock). Hover is magenta; the selected range is turquoise.

## Install

```bash
git clone https://github.com/J160KU/jgk_tiles.git
cd jgk_tiles
./install.sh
```

Needs `build-essential`, `pkg-config`, `libx11-dev`, `libwayland-dev`, and `wayland-protocols` (`install.sh` installs them if missing).

On GNOME Wayland, log out and back in after the first extension install (or after changing `extension.js`), then:

```bash
gnome-extensions enable jgk-tiles@local
test -S "$XDG_RUNTIME_DIR/jgk_tiles.sock" && echo bridge ok
```

## Usage

1. Focus the window you want to tile.
2. Press **Ctrl+Esc**.
3. Click a start tile, then an end tile (any rectangle on the 16×8 grid).
4. **Esc** or **Ctrl+Esc** again cancels.

## How it works

- A small C daemon draws the grid overlay and listens for the hotkey.
- A GNOME Shell extension (`jgk-tiles@local`) stores the focused window and applies the resize through Mutter (`move_resize_frame`). Native Wayland windows cannot be resized via X11/EWMH.

Runtime socket: `$XDG_RUNTIME_DIR/jgk_tiles.sock`. Log: `~/.cache/jgk_tiles.log`.
