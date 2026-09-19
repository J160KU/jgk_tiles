# PROGRESS

status:usable
grid:16x8
session:GNOME Wayland
hotkey:Ctrl+Esc

## done
- C daemon overlay + GNOME extension Meta resize
- work-area grid (not under panel/dock)
- hover magenta; selection turquoise
- install.sh + make install (bin, extension, keys, autostart)

## verify
```bash
./install.sh
gnome-extensions enable jgk-tiles@local
test -S "$XDG_RUNTIME_DIR/jgk_tiles.sock" && echo bridge ok
pgrep -a jgk_tiles
```

## next
- kit install-all via this folder
