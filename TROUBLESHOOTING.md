# TROUBLESHOOTING

## overlay not transparent / covers apps
cause:C xdg_toplevel is a real window; GNOME does not composite through it
fix:enable `jgk-tiles@local` (Shell chrome overlay after session restart)

## windows under top bar or dock
cause:resize used full-screen origin (y=0) instead of work area
fix:current code uses `_NET_WORKAREA` / Meta work area; reinstall if running an old binary

## hover highlight offset from grid
cause:draw used screen coords (dock/panel offset) on a work-area-sized surface
fix:surface-local `grid_rect_span` / `grid_hit_span` in current sources

## clicks do nothing visually
cause:Wayland buffer commit not flushed
fix:current `wl_draw` calls `wl_display_flush`

## no resize on Wayland
cause:extension OUT OF DATE / socket missing
fix:log out/in; `gnome-extensions enable jgk-tiles@local`; `test -S "$XDG_RUNTIME_DIR/jgk_tiles.sock"`

## hotkey dead
fix:`./install.sh` (gsettings Ctrl+Esc → `jgk_tiles --toggle`); daemon autostart

## daemon not running
fix:`pgrep -a jgk_tiles`; `jgk_tiles &` or log in again (autostart)

## build
```bash
sudo apt install build-essential pkg-config libx11-dev libwayland-dev wayland-protocols
make
```
